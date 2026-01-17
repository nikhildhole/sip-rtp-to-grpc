#include "CallSession.h"
#include "../app/Config.h"
#include "../app/Logger.h"
#include <arpa/inet.h>
#include "../media/stages/EchoStage.h"
#include "../media/stages/GrpcBridgeStage.h"
#include "../media/stages/RecorderStage.h"
#include "../rtp/RtpServer.h"

CallSession::CallSession(const std::string &callId) : callId_(callId) {
  // Generate random SSRC
  ssrc_ = (uint32_t)std::chrono::system_clock::now().time_since_epoch().count();
  outgoingSeq_ = (uint16_t)(ssrc_ & 0xFFFF);
  outgoingTimestamp_ = (uint32_t)(ssrc_);
}

CallSession::~CallSession() { terminate(); }

void CallSession::init(const SipMessage &invite, const sockaddr_in &remoteSip) {
  // Potential dialog initialization
}

void CallSession::terminate() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (localRtpPort_ > 0) {
    RtpServer::instance().releasePort(localRtpPort_);
    localRtpPort_ = 0;
  }
  if (botClient_) {
    botClient_->stop();
    botClient_ = nullptr;
  }
  pipeline_ = nullptr; // Explicitly reset pipeline
}

void CallSession::startPipeline(int localRtpPort, const std::string &remoteIp,
                                int remotePort, int payloadType) {
  std::lock_guard<std::mutex> lock(mutex_);
  localRtpPort_ = localRtpPort;
  payloadType_ = payloadType;

  remoteRtpAddr_.sin_family = AF_INET;
  remoteRtpAddr_.sin_port = htons(remotePort);
  inet_pton(AF_INET, remoteIp.c_str(), &remoteRtpAddr_.sin_addr);

  pipeline_ = std::make_shared<MediaPipeline>();
  auto &config = Config::instance();

  // 1. gRPC Bridge (Source for DL, Sink for UL)
  if (!config.echoMode) {
    botClient_ = std::make_shared<VoiceBotClient>(config.grpcTarget, callId_);
    if (botClient_->connect()) {
      botClient_->sendConfig(8000, payloadType == 8 ? 8 : 0);
      pipeline_->addStage(std::make_shared<GrpcBridgeStage>(botClient_));
    } else {
      LOG_ERROR("Failed to connect to gRPC bot for call " << callId_);
    }
  }

  // 2. Echo (Optional)
  if (config.echoMode) {
    pipeline_->addStage(std::make_shared<EchoStage>());
  }

  // 3. Recorder
  if (config.recordingMode != "OFF") {
    pipeline_->addStage(
        std::make_shared<RecorderStage>(config.recordingPath, callId_));
  }
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
      
      pipelineCopy->processUplink(payload);
      
      // Pipeline Downlink
      auto dlPayload = pipelineCopy->processDownlink();
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

void CallSession::onSipMessage(const SipMessage &msg,
                               const sockaddr_in &sender) {
  if (msg.isRequest && msg.method == SipMethod::BYE) {
    LOG_INFO("Received BYE for call " << callId_);
    // SipServer will send 200 OK, we just need to terminate
    terminate();
  }
}
