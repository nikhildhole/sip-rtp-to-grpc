#pragma once

#include "SipMessage.h"
#include <string>

enum class DialogState { EARLY, CONFIRMED, TERMINATED };

class SipDialog {
public:
  SipDialog(const SipMessage &invite, const std::string &localTag);

  std::string getCallId() const { return callId_; }
  std::string getRemoteTag() const { return remoteTag_; }
  std::string getLocalTag() const { return localTag_; }

  DialogState getState() const { return state_; }
  void setState(DialogState s) { state_ = s; }

  int getLocalSeq() const { return localSeq_; }
  int getNextLocalSeq() { return ++localSeq_; }
  int getRemoteSeq() const { return remoteSeq_; }

  void updateRemoteSeq(int seq);

private:
  std::string callId_;
  std::string localTag_;
  std::string remoteTag_;
  DialogState state_;

  int localSeq_ = 0;
  int remoteSeq_ = 0;
};
