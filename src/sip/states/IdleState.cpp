#include "IdleState.h"
#include "ActiveState.h"
#include "TerminatedState.h"
#include "../../call/CallSession.h"
#include "../../call/CallRegistry.h"
#include "../../app/Config.h"
#include "../../app/Logger.h"
#include "../SipResponseBuilder.h"
#include "../SipConstants.h"
#include "../../sdp/SdpParser.h"
#include "../../sdp/SdpAnswer.h"

void IdleState::handleInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    auto& config = Config::instance();

    // 1. Check Max Calls
    if (CallRegistry::instance().count() > config.maxCalls) {
        LOG_WARN("Max calls reached, rejecting " << msg.getCallId());
        session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::BUSY_HERE, "Busy Here"), sender);
        session.setState(std::make_shared<TerminatedState>());
        return;
    }

    // 2. Send 100 Trying
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::TRYING, "Trying"), sender);

    // 3. Parse SDP
    auto sdpOpt = SdpParser::parse(msg.body);
    if (!sdpOpt) {
        LOG_ERROR("No SDP in INVITE for " << msg.getCallId());
        session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::BAD_REQUEST, "Bad Request (No SDP)"), sender);
        session.setState(std::make_shared<TerminatedState>());
        return;
    }

    // 4. Allocate Ports
    if (!session.allocateLocalPort()) {
        LOG_ERROR("Failed to allocate ports for " << msg.getCallId());
        session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::INTERNAL_SERVER_ERROR, "Internal Server Error"), sender);
        session.setState(std::make_shared<TerminatedState>());
        return;
    }

    LOG_DEBUG("Allocated RTP port " << session.getLocalPort() << " for call " << session.getCallId());

    // 5. Generate SDP Answer
    NegotiatedCodec codec;
    std::string sdpAnswer = SdpAnswer::generate(
        *sdpOpt, config.getEffectiveBindIp(), session.getLocalPort(),
        config.codecPreference, codec);

    if (sdpAnswer.empty()) {
        LOG_ERROR("No common codec for " << msg.getCallId());
        session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::NOT_ACCEPTABLE, "Not Acceptable Here"), sender);
        session.setState(std::make_shared<TerminatedState>());
        return;
    }

    // 6. Setup Pipeline
    std::string remoteIp = sdpOpt->connectionIp;
    int remotePort = 0;
    for (auto &m : sdpOpt->media)
        if (m.type == "audio")
            remotePort = m.port;

    if (!session.startPipeline(session.getLocalPort(), remoteIp, remotePort, codec.payloadType)) {
        LOG_ERROR("Failed to start media pipeline for " << msg.getCallId());
        session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::SERVICE_UNAVAILABLE, "Service Unavailable (Backend Connection Failed)"), sender);
        session.setState(std::make_shared<TerminatedState>());
        return;
    }

    // 7. Send 200 OK
    auto res = SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK");
    res.addHeader("Content-Type", "application/sdp");
    res.addHeader("Contact", "<sip:" + config.getEffectiveBindIp() + ":" + std::to_string(config.sipPort) + ">");
    res.body = sdpAnswer; 
    
    // Initialize dialog before sending response so it's ready
    session.setDialog(std::make_shared<SipDialog>(msg, res.getToTag()));

    session.sendResponse(res, sender);

    // 8. Transition to Active
    session.setState(std::make_shared<ActiveState>());
}

void IdleState::handleAck(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_WARN("Received ACK in IdleState, ignoring");
}

void IdleState::handleBye(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    // Race condition or late BYE?
    LOG_INFO("Received BYE in IdleState, terminating");
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
    session.terminate();
    session.setState(std::make_shared<TerminatedState>());
}

void IdleState::handleCancel(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_INFO("Received CANCEL in IdleState");
    // Since we sent 100 Trying, we should send 200 OK to CANCEL and 487 to INVITE?
    // But we are processing INVITE synchronously in handleInvite usually.
    // If we were async, we would be in ProceedingState.
    // However, in this simple implementation, we go straight to 200 OK in handleInvite.
    // So if CANCEL arrives it might be for a race, or if we implemented 'Ringing' state.
    // Since we don't have Ringing state in this simplified Idle handleInvite (auto-answer),
    // we just reply 200 OK to CANCEL.
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
    session.setState(std::make_shared<TerminatedState>());
}

void IdleState::handleReInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
     LOG_WARN("Received Re-INVITE in IdleState, ignoring");
}

void IdleState::handleUpdate(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
     session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::NOT_IMPLEMENTED, "Not Implemented"), sender);
}

void IdleState::handleOptions(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
}

void IdleState::handleError(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_ERROR("Protocol error in IdleState");
}
