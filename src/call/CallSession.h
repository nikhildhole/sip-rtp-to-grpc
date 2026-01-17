#pragma once

#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <string>

#include "../grpc/VoiceBotClient.h"
#include "../audiosocket/AudioSocketClient.h"
#include "../media/MediaPipeline.h"
#include "../rtp/JitterBuffer.h"
#include "../sip/SipDialog.h"
#include "../sip/SipServer.h"

// Forward declaration
class CallRegistry;

class CallSession : public std::enable_shared_from_this<CallSession> {
public:
  CallSession(const std::string &callId);
  ~CallSession();

  std::string getCallId() const { return callId_; }

  // Setup
  void init(const SipMessage &invite, const sockaddr_in &remoteSip);

  // SIP Events
  void onSipMessage(const SipMessage &msg, const sockaddr_in &sender);

  // RTP Events
  void onRtpPacket(const RtpPacket &pkt, const sockaddr_in &sender);

  // Stop/Cleanup
  void terminate();

  // Pipeline access
  void startPipeline(int localRtpPort, const std::string &remoteIp,
                     int remotePort, int payloadType);

private:
  std::string callId_;
  std::string fromUser_;
  std::string toUser_;
  std::shared_ptr<SipDialog> dialog_;

  // Media
  std::shared_ptr<MediaPipeline> pipeline_;
  std::shared_ptr<VoiceBotClient> botClient_;
  std::shared_ptr<AudioSocketClient> tcpClient_;
  JitterBuffer jitterBuffer_;

  std::mutex mutex_;
  int localRtpPort_ = 0;
  sockaddr_in remoteRtpAddr_{};
  bool rtpLocked_ = false;
  int payloadType_ = 0;

  // RTP Stream State
  uint32_t ssrc_ = 0;
  uint32_t outgoingTimestamp_ = 0;
  uint16_t outgoingSeq_ = 0;

  void processRtpFrame();
};
