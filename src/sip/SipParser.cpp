#include "SipParser.h"
#include "SipMessage.h"
#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

// Optimized trim helper using string_view
static std::string_view trim_sv(std::string_view s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos)
    return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

SipMethod SipParser::parseMethod(std::string_view sv) {
  if (sv == "INVITE")
    return SipMethod::INVITE;
  if (sv == "ACK")
    return SipMethod::ACK;
  if (sv == "BYE")
    return SipMethod::BYE;
  if (sv == "CANCEL")
    return SipMethod::CANCEL;
  if (sv == "OPTIONS")
    return SipMethod::OPTIONS;
  if (sv == "REFER")
    return SipMethod::REFER;
  if (sv == "REGISTER")
    return SipMethod::REGISTER;
  if (sv == "UPDATE")
    return SipMethod::UPDATE;
  return SipMethod::UNKNOWN;
}

std::optional<SipMessage> SipParser::parse(const char *buffer, size_t length) {
  SipMessage msg;
  std::string_view raw(buffer, length);
  size_t pos = 0;

  auto get_line = [&]() -> std::optional<std::string_view> {
    if (pos >= raw.length()) return std::nullopt;
    size_t end = raw.find("\n", pos);
    std::string_view line;
    if (end == std::string_view::npos) {
      line = raw.substr(pos);
      pos = raw.length();
    } else {
      line = raw.substr(pos, end - pos);
      pos = end + 1;
    }
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    return line;
  };

  // Parse First Line
  auto firstLineOpt = get_line();
  if (!firstLineOpt) return std::nullopt;
  std::string_view line = *firstLineOpt;

  if (line.rfind("SIP/2.0", 0) == 0) {
    // Response: SIP/2.0 200 OK
    msg.isRequest = false;
    msg.version = "SIP/2.0";
    auto firstSpace = line.find(' ');
    if (firstSpace != std::string_view::npos) {
      auto secondSpace = line.find(' ', firstSpace + 1);
      if (secondSpace != std::string_view::npos) {
        try {
          msg.statusCode = std::stoi(std::string(line.substr(firstSpace + 1, secondSpace - firstSpace - 1)));
          msg.statusPhrase = std::string(line.substr(secondSpace + 1));
        } catch (...) {
          return std::nullopt;
        }
      } else {
        return std::nullopt;
      }
    } else {
      return std::nullopt;
    }
  } else {
    // Request: INVITE sip:user@host SIP/2.0
    msg.isRequest = true;
    auto firstSpace = line.find(' ');
    auto lastSpace = line.rfind(' ');
    if (firstSpace != std::string_view::npos && lastSpace != std::string_view::npos &&
        firstSpace != lastSpace) {
      std::string methodStr = std::string(line.substr(0, firstSpace));
      std::transform(methodStr.begin(), methodStr.end(), methodStr.begin(), ::toupper);
      msg.methodStr = methodStr;
      msg.method = parseMethod(msg.methodStr);
      msg.uri = std::string(line.substr(firstSpace + 1, lastSpace - firstSpace - 1));
      msg.version = std::string(line.substr(lastSpace + 1));
    } else {
      return std::nullopt;
    }
  }

  // Headers
  int contentLength = 0;
  std::string lastName;
  while (auto lineOpt = get_line()) {
    std::string_view hLine = *lineOpt;
    if (hLine.empty())
      break; // End of headers

    // Handle folded headers (lines starting with space/tab)
    if (!hLine.empty() && (hLine[0] == ' ' || hLine[0] == '\t')) {
      if (!lastName.empty()) {
        auto it = msg.headers.find(lastName);
        if (it != msg.headers.end()) {
          it->second += " ";
          it->second += std::string(trim_sv(hLine));
        }
      }
      continue;
    }

    auto colon = hLine.find(':');
    if (colon != std::string_view::npos) {
      std::string name = std::string(trim_sv(hLine.substr(0, colon)));
      std::string value = std::string(trim_sv(hLine.substr(colon + 1)));
      msg.addHeader(name, value);
      lastName = name;
      std::transform(lastName.begin(), lastName.end(), lastName.begin(), ::tolower);
      
      if (lastName == "content-length") {
        try {
          contentLength = std::stoi(value);
        } catch (...) {
          contentLength = 0;
        }
      }
    }
  }

  // Body
  if (contentLength > 0) {
    size_t remaining = raw.length() - pos;
    size_t toRead = std::min((size_t)contentLength, remaining);
    msg.body.assign(raw.data() + pos, toRead);
  }

  return msg;
}

