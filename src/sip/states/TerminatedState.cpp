#include "TerminatedState.h"
#include "../../call/CallSession.h"
#include "../../app/Logger.h"

void TerminatedState::handleInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored INVITE in TerminatedState");
}

void TerminatedState::handleAck(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored ACK in TerminatedState");
}

void TerminatedState::handleBye(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored BYE in TerminatedState");
}

void TerminatedState::handleCancel(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored CANCEL in TerminatedState");
}

void TerminatedState::handleReInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored RE-INVITE in TerminatedState");
}

void TerminatedState::handleUpdate(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored UPDATE in TerminatedState");
}

void TerminatedState::handleOptions(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored OPTIONS in TerminatedState");
}

void TerminatedState::handleError(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Ignored Error in TerminatedState");
}
