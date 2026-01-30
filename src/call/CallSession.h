#pragma once

#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <string>


#include "../audiosocket/AudioSocketClient.h"
#include "../media/MediaPipeline.h"
#include "../rtp/JitterBuffer.h"
#include "../sip/SipDialog.h"
#include "../sip/SipServer.h"
#include "../sip/CallState.h"

// Forward declaration
class CallRegistry;
class CallState;

using ResponseSender = std::function<void(const SipMessage &, const sockaddr_in &)>;

class CallSession : public std::enable_shared_from_this<CallSession> {
public:
  CallSession(const std::string &callId);
  ~CallSession();

  std::string getCallId() const { return callId_; }

  // Setup
  void init(const SipMessage &invite, const sockaddr_in &remoteSip, ResponseSender sender);
  void setResponseSender(ResponseSender sender) { responseSender_ = sender; }

  // SIP Events
  void onSipMessage(const SipMessage &msg, const sockaddr_in &sender);

  // State Management
  void setState(std::shared_ptr<CallState> state);
  std::shared_ptr<CallState> getState() const { return currentState_; }
  
  std::shared_ptr<SipDialog> getDialog() const { return dialog_; }
  void setDialog(std::shared_ptr<SipDialog> dialog) { dialog_ = dialog; }

  // Actions for States
  void sendResponse(const SipMessage& req, int code, const std::string& reason);
  void sendResponse(const SipMessage& res, const sockaddr_in& addr);
  
  // Media Control
  bool allocateLocalPort();
  int getLocalPort() const { return localRtpPort_; }
  void setRemoteMedia(const std::string& ip, int port);
  void setPayloadType(int pt) { payloadType_ = pt; }
  
  // Accessors
  std::shared_ptr<MediaPipeline> getPipeline() const { return pipeline_; }
  const std::string& getFromUser() const { return fromUser_; }
  const std::string& getToUser() const { return toUser_; }

  // RTP Events
  void onRtpPacket(const RtpPacket &pkt, const sockaddr_in &sender);

  // Stop/Cleanup
  void terminate();
  void hangup();
  void sendRefer(const std::string& referTo);

  // Pipeline access
  bool startPipeline(int localRtpPort, const std::string &remoteIp,
                     int remotePort, int payloadType);

private:
  std::string callId_;
  std::string fromUser_;
  std::string toUser_;
  std::shared_ptr<SipDialog> dialog_;

  // Media
  std::shared_ptr<MediaPipeline> pipeline_;

  std::shared_ptr<AudioSocketClient> tcpClient_;
  JitterBuffer jitterBuffer_;

  std::mutex mutex_;
  int localRtpPort_ = 0;
  sockaddr_in remoteRtpAddr_{};
  sockaddr_in remoteSipAddr_{}; // Stored from init/INVITE
  ResponseSender responseSender_;
  bool rtpLocked_ = false;
  int payloadType_ = 0;

  // RTP Stream State
  uint32_t ssrc_ = 0;
  uint32_t outgoingTimestamp_ = 0;
  uint16_t outgoingSeq_ = 0;

  void processRtpFrame();

  std::shared_ptr<CallState> currentState_;
};
