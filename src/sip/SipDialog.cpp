#include "SipDialog.h"

SipDialog::SipDialog(const SipMessage &invite, const std::string &localTag)
    : state_(DialogState::EARLY), localTag_(localTag) {
  callId_ = invite.getCallId();
  remoteTag_ = invite.getFromTag();
  remoteSeq_ = invite.getCSeq();
}

void SipDialog::updateRemoteSeq(int seq) {
  if (seq > remoteSeq_) {
    remoteSeq_ = seq;
  }
}
