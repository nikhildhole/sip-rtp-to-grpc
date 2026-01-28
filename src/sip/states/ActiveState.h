#pragma once

#include "../CallState.h"

class ActiveState : public CallState {
public:
    void handleInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;
    void handleAck(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;
    void handleBye(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;
    void handleCancel(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;
    void handleReInvite(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;
    void handleUpdate(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;
    void handleOptions(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;
    void handleError(CallSession& session, const SipMessage& msg, const sockaddr_in& sender) override;

    std::string getName() const override { return "Active"; }
};
