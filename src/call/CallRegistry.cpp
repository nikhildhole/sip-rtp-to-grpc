#include "CallRegistry.h"

CallRegistry &CallRegistry::instance() {
  static CallRegistry instance;
  return instance;
}

bool CallRegistry::addCall(const std::string &callId,
                           std::shared_ptr<CallSession> session) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (calls_.find(callId) != calls_.end())
    return false;
  calls_[callId] = session;
  return true;
}

std::shared_ptr<CallSession> CallRegistry::getCall(const std::string &callId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = calls_.find(callId);
  if (it != calls_.end())
    return it->second;
  return nullptr;
}

void CallRegistry::removeCall(const std::string &callId) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = callToPorts_.find(callId);
  if (it != callToPorts_.end()) {
    for (int port : it->second) {
      portMap_.erase(port);
    }
    callToPorts_.erase(it);
  }
  
  calls_.erase(callId);
}

void CallRegistry::registerRtpPort(int port, const std::string &callId) {
  std::lock_guard<std::mutex> lock(mutex_);
  portMap_[port] = callId;
  callToPorts_[callId].push_back(port);
}

std::shared_ptr<CallSession> CallRegistry::getCallByPort(int port) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = portMap_.find(port);
  if (it != portMap_.end()) {
    auto cit = calls_.find(it->second);
    if (cit != calls_.end())
      return cit->second;
  }
  return nullptr;
}

size_t CallRegistry::count() {
  std::lock_guard<std::mutex> lock(mutex_);
  return calls_.size();
}

std::vector<std::string> CallRegistry::getAllCallIds() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> ids;
  for (const auto &pair : calls_) {
    ids.push_back(pair.first);
  }
  return ids;
}
