#pragma once

#include "SipMessage.h"
#include <optional>
#include <string_view>

class SipParser {
public:
  static std::optional<SipMessage> parse(const char *buffer, size_t length);

private:
  static SipMethod parseMethod(std::string_view sv);
};
