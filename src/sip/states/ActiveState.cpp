#include "ActiveState.h"
#include "TerminatedState.h"
#include "../../call/CallSession.h"
#include "../../app/Logger.h"
#include "../SipResponseBuilder.h"
#include "../SipConstants.h"
#include "../../sdp/SdpParser.h"
#include "../../sdp/SdpAnswer.h"
#include "../../app/Config.h"

void ActiveState::handleInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    // Re-Invite handling
    LOG_INFO("Received Re-INVITE in ActiveState");
    
    // Check for Glare (491) (Placeholder)
    // if (session.isClientTransactionPending()) {
    //     session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::REQUEST_PENDING, "Request Pending"), sender);
    //     return;
    // }
    
    // 1. Send 100 Trying
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::TRYING, "Trying"), sender);

    // 2. Parse SDP
    auto sdpOpt = SdpParser::parse(msg.body);
    if (!sdpOpt) {
        // Validation needed
        auto res = SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"); 
        // If no SDP offer, we should send offer?
        // Standard says if Re-Invite has no SDP, we send offer.
        // For now, assume it has SDP or reject.
         session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::BAD_REQUEST, "No SDP"), sender);
         return;
    }

    // 3. Negotiate (Reuse ports)
    NegotiatedCodec codec;
    std::string sdpAnswer = SdpAnswer::generate(
        *sdpOpt, Config::instance().bindIp, session.getLocalPort(),
        Config::instance().codecPreference, codec);

    if (sdpAnswer.empty()) {
        session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::NOT_ACCEPTABLE, "Not Acceptable"), sender);
        return;
    }

    // 4. Update Pipeline (if needed)
    // Note: session.startPipeline resets pipeline, might be heavy?
    // Ideally we just update remote params.
    // For now, we update remote media.
     std::string remoteIp = sdpOpt->connectionIp;
    int remotePort = 0;
    for (auto &m : sdpOpt->media)
        if (m.type == "audio")
            remotePort = m.port;
            
    session.setRemoteMedia(remoteIp, remotePort);
    // session.setPayloadType(codec.payloadType); // If changed

    // 5. Send 200 OK
    auto res = SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK");
    res.addHeader("Content-Type", "application/sdp");
    res.addHeader("Contact", "<sip:" + Config::instance().getEffectiveBindIp() + ":" + std::to_string(Config::instance().sipPort) + ">");
    res.body = sdpAnswer;
    session.sendResponse(res, sender);
}

void ActiveState::handleAck(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Received ACK in ActiveState");
    // Normal for 200 OK to Invite/Re-Invite
}

void ActiveState::handleBye(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_INFO("Received BYE in ActiveState");
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
    session.terminate();
    session.setState(std::make_shared<TerminatedState>());
}

void ActiveState::handleCancel(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    // CANCEL only applies to pending requests (INVITE).
    // In Active state, we might have a pending Re-Invite?
    // For now, just OK.
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
}

void ActiveState::handleReInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    // Dispatched to handleInvite by CallSession usually, but if separated:
    handleInvite(session, msg, sender);
}

void ActiveState::handleUpdate(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_INFO("Received UPDATE in ActiveState");
    // Handle Session Timer or Media Update
    // Similar to Re-Invite but cleaner.
    // Allow it with 200 OK (copying Recv-Info/SDP handling logic if strictly needed)
    // For now simple 200 OK.
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
}

void ActiveState::handleOptions(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    session.sendResponse(SipResponseBuilder::createResponse(msg, SipConstants::OK, "OK"), sender);
}

void ActiveState::handleError(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) {
    LOG_DEBUG("Protocol error in ActiveState");
}
