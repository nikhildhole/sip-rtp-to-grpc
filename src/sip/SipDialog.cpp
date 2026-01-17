#include "SipDialog.h"

SipDialog::SipDialog(const SipMessage &invite, const std::string &localTag)
    : callId_(invite.getCallId()),
      localTag_(localTag),
      remoteTag_(invite.getFromTag()),
      state_(DialogState::EARLY),
      localSeq_(0),
      remoteSeq_(invite.getCSeq()) {}

void SipDialog::updateRemoteSeq(int seq) {
  if (seq > remoteSeq_) {
    remoteSeq_ = seq;
  }
}
