#pragma once

#include "SipMessage.h"
#include <memory>
#include <netinet/in.h>

class CallSession;

class CallState {
public:
    virtual ~CallState() = default;

    virtual void handleInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    virtual void handleAck(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    virtual void handleBye(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    virtual void handleCancel(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    virtual void handleReInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    virtual void handleUpdate(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    virtual void handleOptions(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    virtual void handleError(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) = 0;
    
    // For timer events if needed
    virtual void onTimer(CallSession& session) {}

    // State name for debugging
    virtual std::string getName() const = 0;
};
