#include "ProceedingState.h"
#include "../../call/CallSession.h"
#include "../../app/Logger.h"
#include "../SipResponseBuilder.h"
#include "../SipConstants.h"

void ProceedingState::handleInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    // Retransmission of INVITE?
    LOG_DEBUG("Received INVITE retransmission in ProceedingState");
}

void ProceedingState::handleAck(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    // Should not happen for provisional responses usually, unless we sent 200 OK and moved state.
}

void ProceedingState::handleBye(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
    session.terminate();
    // Move to Terminated
}

void ProceedingState::handleCancel(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_INFO("Received CANCEL in ProceedingState");
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
    // Needed: Send 487 to original INVITE?
    // Session doesn't store original INVITE here easily.
    // In full implementation we would.
    // For now, Terminate.
}

void ProceedingState::handleReInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
}

void ProceedingState::handleUpdate(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
}

void ProceedingState::handleOptions(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
}

void ProceedingState::handleError(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
}
