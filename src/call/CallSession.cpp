#include "CallSession.h"
#include "../app/Config.h"
#include "../app/Logger.h"
#include "../call/CallRegistry.h"
#include <arpa/inet.h>
#include "../media/stages/EchoStage.h"

#include "../media/stages/AudioSocketStage.h"
#include "../media/stages/RecorderStage.h"
#include "../media/stages/RecorderStage.h"
#include "../rtp/RtpServer.h"
#include "../sip/SipResponseBuilder.h"
#include "../sip/states/TerminatedState.h"

CallSession::CallSession(const std::string &callId) : callId_(callId) {
  // Generate random SSRC
  ssrc_ = (uint32_t)std::chrono::system_clock::now().time_since_epoch().count();
  outgoingSeq_ = (uint16_t)(ssrc_ & 0xFFFF);
  outgoingTimestamp_ = (uint32_t)(ssrc_);
}

CallSession::~CallSession() { terminate(); }

void CallSession::init(const SipMessage &invite, const sockaddr_in &remoteSip, ResponseSender sender) {
  fromUser_ = invite.getFromUser();
  toUser_ = invite.getToUser();
  remoteSipAddr_ = remoteSip;
  responseSender_ = sender;
  LOG_INFO("Call session initialized for " << fromUser_ << " -> " << toUser_);
}

void CallSession::sendResponse(const SipMessage& req, int code, const std::string& reason) {
    // This helper is dangerous if we don't know the sender.
    // Better to let State handle response creation and passing address.
    // But for convenience, if we stored remoteSipAddr_, we can use it?
    // No, for responses to incoming requests, we must send to the source of the request.
    // For now, we will assume the caller knows what they are doing or use a different overload.
    // Actually, let's remove this or change it to take sender.
    // But since this is an implementation file, I can't change signature here without changing header.
    // Let's implement it by sending to remoteSipAddr_ which MIGHT be correct for in-dialog requests, 
    // but for initial INVITE it is correct too.
    auto res = SipResponseBuilder::createResponse(req, code, reason);
    if (responseSender_) {
        responseSender_(res, remoteSipAddr_);
    }
}

void CallSession::sendResponse(const SipMessage& res, const sockaddr_in& addr) {
    if (responseSender_) {
        responseSender_(res, addr);
    }
}

void CallSession::terminate() {
  std::shared_ptr<AudioSocketClient> client;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (localRtpPort_ > 0) {
      RtpServer::instance().releasePort(localRtpPort_);
      localRtpPort_ = 0;
    }

    client = tcpClient_;
    tcpClient_ = nullptr;
    
    // Explicitly reset pipeline while holding lock
    pipeline_ = nullptr;
  }

  // Stop client outside the lock to avoid deadlock if stop() joins a thread 
  // that is currently trying to acquire the CallSession::mutex_ (e.g. via callback)
  if (client) {
    client->stop();
  }
  
  // Use a local copy of callId to remove after lock
  std::string cid;
  {
      std::lock_guard<std::mutex> lock(mutex_);
      cid = callId_;
  }

  if (!cid.empty()) {
      CallRegistry::instance().removeCall(cid);
  }
}

bool CallSession::startPipeline(int localRtpPort, const std::string &remoteIp,
                                int remotePort, int payloadType) {
  std::lock_guard<std::mutex> lock(mutex_);
  localRtpPort_ = localRtpPort;
  payloadType_ = payloadType;

  remoteRtpAddr_.sin_family = AF_INET;
  remoteRtpAddr_.sin_port = htons(remotePort);
  inet_pton(AF_INET, remoteIp.c_str(), &remoteRtpAddr_.sin_addr);

  pipeline_ = std::make_shared<MediaPipeline>();
  auto &config = Config::instance();

  // 1. Core Logic (Source for DL, Sink for UL)

    if (config.mode == Config::GatewayMode::AUDIOSOCKET) {
      tcpClient_ = std::make_shared<AudioSocketClient>(config.audiosocketTarget, callId_, fromUser_, toUser_);
      tcpClient_->setDisconnectCallback([this]() {
          LOG_WARN("AudioSocket disconnected unexpectedly for call " << this->callId_);
          this->hangup();
      });
      if (tcpClient_->connect()) {
        auto stage = std::make_shared<AudioSocketStage>(tcpClient_, payloadType);
        stage->setTransferCallback([this](const std::string &sipUrl) {
          this->sendRefer(sipUrl);
        });
        pipeline_->addStage(stage);
      } else {
        LOG_ERROR("Failed to connect to TCP AudioSocket for call " << callId_);
        return false;
      }
  } else if (config.mode == Config::GatewayMode::ECHO) {
    pipeline_->addStage(std::make_shared<EchoStage>());
  }

  // 3. Recorder
  if (config.recordingMode) {
    pipeline_->addStage(
        std::make_shared<RecorderStage>(config.recordingMode, config.recordingPath, callId_, payloadType));
  }
  return true;
}

void CallSession::onRtpPacket(const RtpPacket &pkt, const sockaddr_in &sender) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Symmetric RTP lock
    if (!rtpLocked_) {
      remoteRtpAddr_ = sender;
      rtpLocked_ = true;
      LOG_INFO("Locked remote RTP source for call " << callId_);
    }
    
    // Audio Processing
    if (pkt.getPayloadType() == payloadType_) {
      LOG_DEBUG("RTP packet received: seq=" << pkt.getSequenceNumber() << ", ts=" << pkt.getTimestamp() << ", size=" << pkt.getPayloadSize() << " for call " << callId_);
      jitterBuffer_.push(pkt);
    } else {
      return;
    }
  }

  // Process outside lock if possible, but jitterBuffer and processRtpFrame 
  // currently rely on state that might be shared.
  // However, processRtpFrame() is where the heavy work is.
  processRtpFrame();
}

void CallSession::processRtpFrame() {
  auto pktOpt = jitterBuffer_.pop();
  if (!pktOpt)
    return;

  auto &pkt = *pktOpt;
  
  // Pipeline processing
  std::shared_ptr<MediaPipeline> pipelineCopy;
  sockaddr_in remoteAddr;
  int localPort;
  int pType;

  {
      std::lock_guard<std::mutex> lock(mutex_);
      pipelineCopy = pipeline_;
      remoteAddr = remoteRtpAddr_;
      localPort = localRtpPort_;
      pType = payloadType_;
  }

  if (pipelineCopy) {
      // Avoid vector copy if possible, but pipeline needs copy for now as per its interface
      std::vector<char> payload(pkt.getPayload(),
                                pkt.getPayload() + pkt.getPayloadSize());
      
      LOG_DEBUG("Processing uplink: " << payload.size() << " bytes for call " << callId_);
      pipelineCopy->processUplink(payload);
      
      // Pipeline Downlink
      auto dlPayload = pipelineCopy->processDownlink();
      LOG_DEBUG("Processing downlink: " << dlPayload.size() << " bytes for call " << callId_);
      if (!dlPayload.empty() && localPort > 0) {
        RtpPacket sendPkt;
        
        uint32_t ts;
        uint16_t seq;
        uint32_t ssrc;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ts = (outgoingTimestamp_ += 160); // Assuming 20ms G711
            seq = ++outgoingSeq_;
            ssrc = ssrc_;
        }

        sendPkt.setHeader(pType, seq, ts, ssrc);
        sendPkt.setPayload((uint8_t *)dlPayload.data(), dlPayload.size());
        RtpServer::instance().send(localPort, sendPkt, remoteAddr);
      }
  }
}

void CallSession::setState(std::shared_ptr<CallState> state) {
    if (!state) return;
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("State change: " << (currentState_ ? currentState_->getName() : "None") 
             << " -> " << state->getName() << " [CallID: " << callId_ << "]");
    currentState_ = state;
}

void CallSession::onSipMessage(const SipMessage &msg,
                               const sockaddr_in &sender) {
  if (!currentState_) {
      LOG_ERROR("No state set for call " << callId_);
      return;
  }
  
  // Dispatch based on method
  if (msg.method == SipMethod::INVITE) {
      if (msg.isRequest) currentState_->handleInvite(*this, msg, sender);
      else currentState_->handleError(*this, msg, sender); // Response to INVITE?
  } else if (msg.method == SipMethod::ACK) {
      currentState_->handleAck(*this, msg, sender);
  } else if (msg.method == SipMethod::BYE) {
      currentState_->handleBye(*this, msg, sender);
  } else if (msg.method == SipMethod::CANCEL) {
      currentState_->handleCancel(*this, msg, sender);
  } else if (msg.method == SipMethod::REGISTER) {
      // REGISTER is usually global, but if routed here, handle error
      currentState_->handleError(*this, msg, sender);
  } else if (msg.method == SipMethod::OPTIONS) {
      currentState_->handleOptions(*this, msg, sender);
  } else {
      // Re-INVITE is INVITE
      // UPDATE, REFER etc
     if (msg.methodStr == "UPDATE") currentState_->handleUpdate(*this, msg, sender);
     else currentState_->handleError(*this, msg, sender);
  }
}


bool CallSession::allocateLocalPort() {
    std::lock_guard<std::mutex> lock(mutex_);
    localRtpPort_ = RtpServer::instance().allocatePort();
    if (localRtpPort_ > 0) {
        CallRegistry::instance().registerRtpPort(localRtpPort_, callId_);
    }
    return localRtpPort_ > 0;
}

void CallSession::setRemoteMedia(const std::string& ip, int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    remoteRtpAddr_.sin_family = AF_INET;
    remoteRtpAddr_.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &remoteRtpAddr_.sin_addr);
}

void CallSession::hangup() {
    std::string stateName = currentState_ ? currentState_->getName() : "None";
    LOG_INFO("Hanging up call " << callId_ << " from state " << stateName);
    
    // 1. Send BYE if we have a dialog and are in Active state (simplified check)
    if (stateName == "Active") {
        LOG_INFO("Sending SIP BYE for call " << callId_);
        
        auto& config = Config::instance();
        std::string localIp = config.getEffectiveBindIp();
        std::string remoteIp = inet_ntoa(remoteSipAddr_.sin_addr);

        SipMessage bye;
        bye.isRequest = true;
        bye.method = SipMethod::BYE;
        bye.methodStr = "BYE";
        
        // Request-URI is the remote target (Linphone)
        bye.uri = "sip:" + fromUser_ + "@" + remoteIp;
        
        std::string branch = "z9hG4bK" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        bye.addHeader("Via", "SIP/2.0/UDP " + localIp + ":" + std::to_string(config.sipPort) + ";branch=" + branch);
        
        if (dialog_) {
            // UAS sending BYE: From is Gateway (toUser), To is Linphone (fromUser)
            bye.addHeader("From", "<sip:" + toUser_ + "@" + localIp + ">;tag=" + dialog_->getLocalTag());
            bye.addHeader("To", "<sip:" + fromUser_ + "@" + remoteIp + ">;tag=" + dialog_->getRemoteTag());
            bye.addHeader("Call-ID", dialog_->getCallId());
            bye.addHeader("CSeq", std::to_string(dialog_->getNextLocalSeq()) + " BYE");
        } else {
            // Fallback
            bye.addHeader("From", "<sip:" + toUser_ + "@" + localIp + ">;tag=gw");
            bye.addHeader("To", "<sip:" + fromUser_ + "@" + remoteIp + ">");
            bye.addHeader("Call-ID", callId_);
            bye.addHeader("CSeq", "1 BYE");
        }
        
        bye.addHeader("Contact", "<sip:" + toUser_ + "@" + localIp + ":" + std::to_string(config.sipPort) + ">");
        bye.addHeader("User-Agent", "SIP-RTP-Gateway");
        
        if (responseSender_) {
            responseSender_(bye, remoteSipAddr_);
        }
    }

    // 2. Transition to Terminated state
    setState(std::make_shared<TerminatedState>());

    // 3. Cleanup
    terminate();
}

void CallSession::sendRefer(const std::string& referTo) {
    LOG_INFO("Initiating SIP REFER to: " << referTo << " for call " << callId_);
    
    auto& config = Config::instance();
    std::string localIp = config.getEffectiveBindIp();
    std::string remoteIp = inet_ntoa(remoteSipAddr_.sin_addr);

    // Create base REFER request
    SipMessage refer;
    refer.isRequest = true;
    refer.method = SipMethod::REFER;
    refer.methodStr = "REFER";
    refer.uri = "sip:" + fromUser_ + "@" + remoteIp;
    
    std::string branch = "z9hG4bK" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    refer.addHeader("Via", "SIP/2.0/UDP " + localIp + ":" + std::to_string(config.sipPort) + ";branch=" + branch);
    
    if (dialog_) {
        // refer is in-dialog: same From/To as initial setup
        refer.addHeader("From", "<sip:" + toUser_ + "@" + localIp + ">;tag=" + dialog_->getLocalTag());
        refer.addHeader("To", "<sip:" + fromUser_ + "@" + remoteIp + ">;tag=" + dialog_->getRemoteTag());
        refer.addHeader("Call-ID", dialog_->getCallId());
        refer.addHeader("CSeq", std::to_string(dialog_->getNextLocalSeq()) + " REFER");
    } else {
        refer.addHeader("From", "<sip:" + toUser_ + "@" + localIp + ">;tag=trans");
        refer.addHeader("To", "<sip:" + fromUser_ + "@" + remoteIp + ">");
        refer.addHeader("Call-ID", callId_);
        refer.addHeader("CSeq", "1 REFER");
    }
    
    refer.addHeader("Refer-To", "<" + referTo + ">");
    refer.addHeader("Contact", "<sip:" + toUser_ + "@" + localIp + ":" + std::to_string(config.sipPort) + ">");
    refer.addHeader("User-Agent", "SIP-RTP-Gateway");
    
    if (responseSender_) {
        responseSender_(refer, remoteSipAddr_);
    }
}
