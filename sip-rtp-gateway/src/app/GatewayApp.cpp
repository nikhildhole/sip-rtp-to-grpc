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

GatewayApp::GatewayApp() {}

GatewayApp::~GatewayApp() {
  if (cliThread_.joinable())
    cliThread_.join();
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
    sipServer_->poll();
    cleanupTransactions();
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Reduce CPU spin
  }

  running_ = false;
  LOG_INFO("Shutting down...");
}

void GatewayApp::handleSipMessage(const SipMessage &msg,
                                  const sockaddr_in &sender) {
  std::string callId = msg.getCallId();
  if (callId.empty())
    return;

  std::string branch = msg.getBranch();
  std::string txKey = callId + ":" + branch + ":" + msg.methodStr;

  auto session = CallRegistry::instance().getCall(callId);

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

      if (msg.method == SipMethod::INVITE) {
        if (!session) {
          if (CallRegistry::instance().count() >= Config::instance().maxCalls) {
            auto res = SipResponseBuilder::createResponse(msg, 486, "Busy Here");
            transaction->sendResponse(res);
            sipServer_->sendResponse(res, sender);
            return;
          }
          session = std::make_shared<CallSession>(callId);
          session->init(msg, sender);
          CallRegistry::instance().addCall(callId, session);
        }

        auto tryingRes = SipResponseBuilder::createResponse(msg, 100, "Trying");
        transaction->sendResponse(tryingRes);
        sipServer_->sendResponse(tryingRes, sender);

        auto sdpOpt = SdpParser::parse(msg.body);
        if (!sdpOpt) {
          auto badRes = SipResponseBuilder::createResponse(msg, 400, "Bad Request (No SDP)");
          transaction->sendResponse(badRes);
          sipServer_->sendResponse(badRes, sender);
          CallRegistry::instance().removeCall(callId);
          return;
        }

        NegotiatedCodec codec;
        int localRtpPort = RtpServer::instance().allocatePort();
        if (localRtpPort < 0) {
          auto errRes = SipResponseBuilder::createResponse(msg, 500, "Internal Server Error (No Ports)");
          transaction->sendResponse(errRes);
          sipServer_->sendResponse(errRes, sender);
          CallRegistry::instance().removeCall(callId);
          return;
        }

        CallRegistry::instance().registerRtpPort(localRtpPort, callId);
        LOG_DEBUG("Allocated RTP port " << localRtpPort << " for call " << callId);

        std::string sdpAnswer = SdpAnswer::generate(
            *sdpOpt, Config::instance().bindIp, localRtpPort,
            Config::instance().codecPreference, codec);

        if (sdpAnswer.empty()) {
          auto errRes = SipResponseBuilder::createResponse(msg, 488, "Not Acceptable Here");
          transaction->sendResponse(errRes);
          sipServer_->sendResponse(errRes, sender);
          RtpServer::instance().releasePort(localRtpPort);
          CallRegistry::instance().removeCall(callId);
          return;
        }

        std::string remoteIp = sdpOpt->connectionIp;
        int remotePort = 0;
        for (auto &m : sdpOpt->media)
          if (m.type == "audio")
            remotePort = m.port;

        session->startPipeline(localRtpPort, remoteIp, remotePort,
                               codec.payloadType);

        auto res = SipResponseBuilder::createResponse(msg, 200, "OK");
        res.addHeader("Content-Type", "application/sdp");
        res.addHeader("Contact", "<sip:" + Config::instance().bindIp + ":" +
                                     std::to_string(Config::instance().sipPort) +
                                     ">");
        res.body = sdpAnswer;
        transaction->sendResponse(res);
        sipServer_->sendResponse(res, sender);

      } else if (msg.method == SipMethod::BYE) {
        auto res = SipResponseBuilder::createResponse(msg, 200, "OK");
        transaction->sendResponse(res);
        sipServer_->sendResponse(res, sender);
        if (session) {
          session->onSipMessage(msg, sender);
          CallRegistry::instance().removeCall(callId);
        }
      } else if (msg.method == SipMethod::CANCEL) {
        auto res = SipResponseBuilder::createResponse(msg, 200, "OK");
        transaction->sendResponse(res);
        sipServer_->sendResponse(res, sender);
        if (session) {
          CallRegistry::instance().removeCall(callId);
        }
      } else if (msg.method == SipMethod::ACK) {
        std::string inviteTxKey = callId + ":" + branch + ":INVITE";
        auto it = transactions_.find(inviteTxKey);
        if (it != transactions_.end()) {
            it->second->receiveRequest(msg);
        }
      } else if (msg.method == SipMethod::REFER) {
        auto res = SipResponseBuilder::createResponse(msg, 202, "Accepted");
        transaction->sendResponse(res);
        sipServer_->sendResponse(res, sender);
        LOG_INFO("Received REFER, blind transfer support minimal.");
      } else if (msg.method == SipMethod::OPTIONS) {
        auto res = SipResponseBuilder::createResponse(msg, 200, "OK");
        transaction->sendResponse(res);
        sipServer_->sendResponse(res, sender);
      } else {
        auto res = SipResponseBuilder::createResponse(msg, 501, "Not Implemented");
        transaction->sendResponse(res);
        sipServer_->sendResponse(res, sender);
      }
    } else {
      std::string resBranch = msg.getBranch();
      std::string resTxKey = callId + ":" + resBranch + ":" + msg.methodStr;
      auto it = transactions_.find(resTxKey);
      if (it != transactions_.end()) {
        LOG_DEBUG("Received response " << msg.statusCode << " for transaction " << resTxKey);
        it->second->receiveResponse(msg);
      } else {
        LOG_WARN("Received response for unknown transaction: " << resTxKey);
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
  while (running_ && std::getline(std::cin, line)) {
    if (line == "list") {
      LOG_INFO("Active Calls: " << CallRegistry::instance().count());
      // TODO: List IDs
    } else if (line.find("cut ") == 0) {
      std::string id = line.substr(4);
      CallRegistry::instance().removeCall(id);
      LOG_INFO("Cut call " << id);
    } else if (line == "exit" || line == "quit") {
      SignalHandler::setExit();
      std::raise(SIGINT);
    }
  }
}
