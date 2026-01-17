#pragma once

#include "SipMessage.h"
#include <chrono>
#include <memory>
#include <string>

enum class TransactionState {
  TRYING,
  PROCEEDING,
  COMPLETED,
  CONFIRMED, // For Invite
  TERMINATED
};

class SipTransaction {
public:
  SipTransaction(const SipMessage &req);

  std::string getBranch() const { return branch_; }
  std::string getMethod() const { return method_; }
  TransactionState getState() const { return state_; }

  // Updates state based on event (sending response, receiving ACK/Response)
  void receiveRequest(const SipMessage &msg);
  void receiveResponse(const SipMessage &msg);
  void sendResponse(const SipMessage &msg);
  bool shouldResendResponse(const SipMessage &msg) const;

  // Retransmission storage
  std::shared_ptr<SipMessage> getLastResponse() const { return lastResponse_; }
  std::chrono::steady_clock::time_point getLastActive() const { return lastActive_; }

private:
  std::string branch_;
  std::string method_; // INVITE, BYE, etc.
  TransactionState state_;

  // Helper to identify transaction type
  bool isInviteTransaction() const;

  std::shared_ptr<SipMessage> lastResponse_;
  std::chrono::steady_clock::time_point lastActive_;
};
