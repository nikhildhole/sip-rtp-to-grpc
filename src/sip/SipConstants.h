#pragma once

#include <string>

namespace SipConstants {
    // Response Codes
    constexpr int TRYING = 100;
    constexpr int RINGING = 180;
    constexpr int OK = 200;
    constexpr int ACCEPTED = 202;
    constexpr int BAD_REQUEST = 400;
    constexpr int FORBIDDEN = 403;
    constexpr int NOT_FOUND = 404;
    constexpr int BUSY_HERE = 486;
    constexpr int REQUEST_TERMINATED = 487;
    constexpr int NOT_ACCEPTABLE = 488;
    constexpr int REQUEST_PENDING = 491;
    constexpr int INTERNAL_SERVER_ERROR = 500;
    constexpr int NOT_IMPLEMENTED = 501;
    constexpr int DECLINE = 603;

    // Headers
    const std::string HDR_CALL_ID = "Call-ID";
    const std::string HDR_CSEQ = "CSeq";
    const std::string HDR_FROM = "From";
    const std::string HDR_TO = "To";
    const std::string HDR_VIA = "Via";
    const std::string HDR_CONTACT = "Contact";
    const std::string HDR_CONTENT_TYPE = "Content-Type";
    const std::string HDR_CONTENT_LENGTH = "Content-Length";
    const std::string HDR_USER_AGENT = "User-Agent";
    const std::string HDR_SUPPORTED = "Supported";
    const std::string HDR_ALLOW = "Allow";
    const std::string HDR_SESSION_EXPIRES = "Session-Expires";
    const std::string HDR_MIN_SE = "Min-SE";

    // Defaults
    constexpr int DEFAULT_SESSION_EXPIRES = 1800; // 30 minutes
    constexpr int MIN_SESSION_EXPIRES = 90;
}
