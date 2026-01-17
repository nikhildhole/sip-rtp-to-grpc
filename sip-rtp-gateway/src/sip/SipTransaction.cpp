#include "SipTransaction.h"

SipTransaction::SipTransaction(const SipMessage &req)
    : state_(TransactionState::TRYING) {
  branch_ = req.getBranch();
  method_ = req.methodStr;
  lastActive_ = std::chrono::steady_clock::now();
}

bool SipTransaction::isInviteTransaction() const { return method_ == "INVITE"; }

void SipTransaction::sendResponse(const SipMessage &res) {
  lastActive_ = std::chrono::steady_clock::now();
  lastResponse_ = std::make_shared<SipMessage>(res);

  if (isInviteTransaction()) {
    if (state_ == TransactionState::TRYING ||
        state_ == TransactionState::PROCEEDING) {
      if (res.statusCode >= 100 && res.statusCode < 200) {
        state_ = TransactionState::PROCEEDING;
      } else if (res.statusCode >= 200 && res.statusCode < 300) {
        // 2xx for INVITE terminates server transaction immediately in RFC,
        // but we keep it around just to handle retransmissions if needed,
        // though usually 2xx retransmits are handled by UAC.
        // Strictly speaking transaction is done.
        state_ = TransactionState::TERMINATED;
      } else if (res.statusCode >= 300 && res.statusCode <= 699) {
        state_ = TransactionState::COMPLETED;
      }
    }
  } else {
    // Non-INVITE
    if (state_ == TransactionState::TRYING || state_ == TransactionState::PROCEEDING) {
      if (res.statusCode >= 200 && res.statusCode <= 699) {
        state_ = TransactionState::COMPLETED;
      } else if (res.statusCode >= 100 && res.statusCode < 200) {
        state_ = TransactionState::PROCEEDING;
      }
    }
  }
}

void SipTransaction::receiveRequest(const SipMessage &msg) {
  lastActive_ = std::chrono::steady_clock::now();
  if (isInviteTransaction()) {
    if (msg.method == SipMethod::ACK && (state_ == TransactionState::COMPLETED || state_ == TransactionState::CONFIRMED)) {
      state_ = TransactionState::CONFIRMED;
    }
  }
}

void SipTransaction::receiveResponse(const SipMessage &msg) {
  lastActive_ = std::chrono::steady_clock::now();
  if (!isInviteTransaction()) {
    if (msg.statusCode >= 200 && msg.statusCode <= 699) {
      state_ = TransactionState::COMPLETED;
    }
  } else {
    // INVITE UAC state machine:
    // PROCEEDING -> (3xx-6xx) -> COMPLETED
    // PROCEEDING -> (2xx) -> TERMINATED (dialog takes over)
    if (msg.statusCode >= 200 && msg.statusCode <= 299) {
       state_ = TransactionState::TERMINATED;
    } else if (msg.statusCode >= 300 && msg.statusCode <= 699) {
       state_ = TransactionState::COMPLETED;
    }
  }
}

bool SipTransaction::shouldResendResponse(const SipMessage &req) const {
  if (req.getBranch() != branch_) return false;
  
  if (isInviteTransaction()) {
      // For INVITE, we resend in PROCEEDING (1xx) or COMPLETED (3xx-600)
      // 2xx retransmissions are handled by UAS core / dialog, not transaction.
      return state_ == TransactionState::PROCEEDING || state_ == TransactionState::COMPLETED;
  } else {
      // For non-INVITE, we resend in PROCEEDING or COMPLETED
      return state_ == TransactionState::PROCEEDING || state_ == TransactionState::COMPLETED;
  }
}
