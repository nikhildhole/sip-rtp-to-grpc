#pragma once

#include "CallSession.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

class CallRegistry {
public:
  static CallRegistry &instance();

  bool addCall(const std::string &callId, std::shared_ptr<CallSession> session);
  std::shared_ptr<CallSession> getCall(const std::string &callId);
  void removeCall(const std::string &callId);

  // Lookup by RTP port
  std::shared_ptr<CallSession> getCallByPort(int port);
  void registerRtpPort(int port, const std::string &callId);

  size_t count();

private:
  std::map<std::string, std::shared_ptr<CallSession>> calls_;
  std::map<int, std::string> portMap_; // rtp port -> call id
  std::map<std::string, std::vector<int>> callToPorts_; // call id -> ports
  std::mutex mutex_;
};
