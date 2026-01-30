#include "GatewayApp.h"
#include "../call/CallRegistry.h"
#include "../call/CallSession.h"
#include "../rtp/RtpServer.h"
#include "../sdp/SdpAnswer.h"
#include "../sdp/SdpParser.h"
#include "../sip/SipResponseBuilder.h"
#include "../util/Net.h"
#include "Config.h"
#include "Logger.h"
#include "SignalHandler.h"
#include <csignal>
#include <iostream>
#include "../sip/states/IdleState.h"

GatewayApp::GatewayApp() {}

GatewayApp::~GatewayApp() {
  running_ = false;
  // If the CLI thread is stuck in std::getline, we can't join it easily without it blocking us.
  // We'll detach it to allow the main thread (and thus the process) to exit.
  if (cliThread_.joinable()) {
      cliThread_.detach();
  }
}

bool GatewayApp::init(const std::string &configPath) {
  if (!Config::instance().load(configPath))
    return false;

  auto &config = Config::instance();

  // Init Logger
  if (config.logLevel == "DEBUG")
    Logger::instance().setLevel(LogLevel::DEBUG);
  else if (config.logLevel == "WARN")
    Logger::instance().setLevel(LogLevel::WARN);
  else if (config.logLevel == "ERROR")
    Logger::instance().setLevel(LogLevel::ERROR);
  else
    Logger::instance().setLevel(LogLevel::INFO);

  // Init RTP Server
  RtpServer::instance().init(config.rtpPortStart, config.rtpPortEnd);
  RtpServer::instance().setPacketHandler(
      [this](int port, const RtpPacket &pkt, const sockaddr_in &sender) {
        this->handleRtpPacket(port, pkt, sender);
      });

  // Init SIP Server
  sipServer_ = std::make_unique<SipServer>(config.sipPort);
  sipServer_->setRequestHandler(
      [this](const SipMessage &msg, const sockaddr_in &sender) {
        this->handleSipMessage(msg, sender);
      });

  return sipServer_->start();
}

void GatewayApp::run() {
  running_ = true;
  cliThread_ = std::thread(&GatewayApp::cliLoop, this);

  LOG_INFO("Gateway running. Press Ctrl+C to exit.");

  while (running_ && !SignalHandler::shouldExit()) {
    sipServer_->poll(10); // Wait up to 10ms for SIP packets
    cleanupTransactions();
    // std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Removed: poll provides pacing
  }

  running_ = false;
  LOG_INFO("Shutting down... Cleaning up active calls.");
  
  // Terminate all sessions
  auto callIds = CallRegistry::instance().getAllCallIds();
  for (const auto& id : callIds) {
      auto session = CallRegistry::instance().getCall(id);
      if (session) {
          LOG_INFO("Terminating call " << id << " during shutdown");
          session->terminate();
      }
  }
}

void GatewayApp::handleSipMessage(const SipMessage &msg,
                                  const sockaddr_in &sender) {
  std::string callId = msg.getCallId();
  if (callId.empty())
    return;

  // Global methods handling (Stateless)
  if (msg.isRequest) {
      if (msg.method == SipMethod::OPTIONS) {
          auto res = SipResponseBuilder::createResponse(msg, 200, "OK");
          sipServer_->sendResponse(res, sender);
          return;
      }
      if (msg.method == SipMethod::REGISTER) {
          // Simple REGISTER handling: Always accept
          auto res = SipResponseBuilder::createResponse(msg, 200, "OK");
          // Add Expires header?
          res.addHeader("Expires", "3600");
          sipServer_->sendResponse(res, sender);
          LOG_INFO("Accepted REGISTER from " << msg.getFromUser());
          return;
      }
  }

  std::string branch = msg.getBranch();
  std::string txKey = callId + ":" + branch + ":" + msg.methodStr;

  {
      std::lock_guard<std::mutex> lock(transactionsMutex_);
      if (msg.isRequest) {
          auto txIt = transactions_.find(txKey);
          if (txIt != transactions_.end()) {
              if (txIt->second->shouldResendResponse(msg)) {
                  auto res = txIt->second->getLastResponse();
                  if (res) {
                      LOG_INFO("Resending retransmission for " << txKey);
                      sipServer_->sendResponse(*res, sender);
                      return;
                  }
              }
              if (msg.method != SipMethod::ACK) return;
          }
          
          auto transaction = std::make_shared<SipTransaction>(msg);
          if (msg.method != SipMethod::ACK) {
              transactions_[txKey] = transaction;
          }
          // Note: ResponseSender in session needs to update transaction?
          // For now, simpler: Transaction layer just deduplicates REQUESTS.
          // Responses sent by Session are just sent.
          // Ideally, Session should feed back the response to Transaction so it can cache it for retransmission.
          // We can capture 'transaction' in the responseSender lambda?
          
          auto session = CallRegistry::instance().getCall(callId);
          if (!session) {
             if (msg.isRequest && msg.method == SipMethod::INVITE) {
                  LOG_INFO("New Call: " << callId);
                  session = std::make_shared<CallSession>(callId);
                  
                  // Capture weak_ptr to transaction to update it when response is sent
                  std::weak_ptr<SipTransaction> weakTx = transaction;
                  auto responseSender = [this, weakTx](const SipMessage& res, const sockaddr_in& addr) {
                      this->sipServer_->sendResponse(res, addr);
                      if (auto tx = weakTx.lock()) {
                          tx->sendResponse(res); // Cache it
                      }
                  };

                  session->init(msg, sender, responseSender);
                  session->setState(std::make_shared<IdleState>());
                  CallRegistry::instance().addCall(callId, session);
                  session->onSipMessage(msg, sender);
             } else if (msg.method != SipMethod::ACK) {
                 // 481
                  auto res = SipResponseBuilder::createResponse(msg, 481, "Call/Transaction Does Not Exist");
                  sipServer_->sendResponse(res, sender);
             }
          } else {
              // Existing session
               // We need to make sure the session uses THIS transaction for THIS message.
               // But session has a generic 'responseSender'.
               // If we just call onSipMessage, it uses the stored sender (from INIT).
               // That stored sender is bound to the INITIAL INVITE transaction.
               // THIS IS A PROBLEM.
               // Subsequent requests (RE-INVITE, BYE) have NEW transactions.
               // SOLUTION: Pass response sender to onSipMessage!
               
               std::weak_ptr<SipTransaction> weakTx = transaction;
               auto scopedSender = [this, weakTx](const SipMessage& res, const sockaddr_in& addr) {
                   this->sipServer_->sendResponse(res, addr);
                   if (auto tx = weakTx.lock()) {
                       tx->sendResponse(res);
                   }
               };
               
               // But CallSession::onSipMessage doesn't take sender.
               // It uses stored `responseSender_`.
               // We should update `responseSender_` or pass it.
               // Passing it is cleaner for state machine.
               // BUT CallSession interface is `onSipMessage(msg, sender)`.
               // We can add `onSipMessage(msg, sender, responseCallback)`.
               
               // Hack for now: Update the specific transaction for the session? 
               // Or simpler: Session sends response, GatewayApp sniffs it? No.
               
               // Let's modify CallSession::onSipMessage to take an optional sender or just update it?
               // Update setResponseSender?
               session->setResponseSender(scopedSender);
               session->onSipMessage(msg, sender);
          }
      } else {
        // Response handling
        std::string resBranch = msg.getBranch();
        std::string resTxKey = callId + ":" + resBranch + ":" + msg.methodStr;
        auto it = transactions_.find(resTxKey);
        if (it != transactions_.end()) {
            it->second->receiveResponse(msg);
        }
      }
  }
}

void GatewayApp::cleanupTransactions() {
  static auto lastCleanup = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  
  if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCleanup).count() < 5) {
    return;
  }
  lastCleanup = now;

  std::lock_guard<std::mutex> lock(transactionsMutex_);
  for (auto it = transactions_.begin(); it != transactions_.end(); ) {
    auto state = it->second->getState();
    auto lastActive = it->second->getLastActive();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - lastActive).count();

    if ((state == TransactionState::TERMINATED || state == TransactionState::COMPLETED || state == TransactionState::CONFIRMED) && age > 32) {
      LOG_DEBUG("Cleaning up transaction " << it->first);
      it = transactions_.erase(it);
    } else if (age > 64) {
      LOG_DEBUG("Cleaning up expired transaction " << it->first);
      it = transactions_.erase(it);
    } else {
      ++it;
    }
  }
}

void GatewayApp::handleRtpPacket(int localPort, const RtpPacket &pkt,
                                 const sockaddr_in &sender) {
  auto session = CallRegistry::instance().getCallByPort(localPort);
  if (session) {
    session->onRtpPacket(pkt, sender);
  }
}

void GatewayApp::cliLoop() {
  std::string line;
  // We use running_ to check if we should keep reading, but std::getline is blocking.
  while (running_ && std::getline(std::cin, line)) {
    if (!running_) break;
    if (line == "list") {
      LOG_INFO("Active Calls: " << CallRegistry::instance().count());
    } else if (line.find("cut ") == 0) {
      std::string id = line.substr(4);
      CallRegistry::instance().removeCall(id);
      LOG_INFO("Cut call " << id);
    } else if (line == "exit" || line == "quit") {
      SignalHandler::setExit();
      // Only break if we are sure we want to stop this thread's loop.
      // SignalHandler::setExit() will also make the main loop exit.
      break;
    }
  }
  LOG_DEBUG("CLI Loop exiting");
}
