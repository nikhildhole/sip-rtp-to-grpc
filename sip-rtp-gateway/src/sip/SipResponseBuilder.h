#pragma once

#include "SipMessage.h"

class SipResponseBuilder {
public:
  static SipMessage createResponse(const SipMessage &req, int code,
                                   const std::string &phrase);
};
