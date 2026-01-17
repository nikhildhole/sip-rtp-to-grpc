#include "SipParser.h"
#include "SipMessage.h"
#include <algorithm>
#include <sstream>
#include <vector>

// Simple trim helper
static std::string trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\r\n");
  auto end = s.find_last_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
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
  return SipMethod::UNKNOWN;
}

std::optional<SipMessage> SipParser::parse(const char *buffer, size_t length) {
  SipMessage msg;
  std::string_view raw(buffer, length);
  std::string rawStr(raw); // Still need a string for istringstream for now, or use string_view-friendly parsing
  std::istringstream stream(rawStr);
  std::string line;

  // Parse First Line
  if (!std::getline(stream, line))
    return std::nullopt;
  if (!line.empty() && line.back() == '\r')
    line.pop_back();

  if (line.rfind("SIP/2.0", 0) == 0) {
    // Response: SIP/2.0 200 OK
    msg.isRequest = false;
    msg.version = "SIP/2.0";
    auto firstSpace = line.find(' ');
    if (firstSpace != std::string::npos) {
      auto secondSpace = line.find(' ', firstSpace + 1);
      if (secondSpace != std::string::npos) {
        try {
          msg.statusCode = std::stoi(
              line.substr(firstSpace + 1, secondSpace - firstSpace - 1));
          msg.statusPhrase = line.substr(secondSpace + 1);
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
    if (firstSpace != std::string::npos && lastSpace != std::string::npos &&
        firstSpace != lastSpace) {
      std::string method = std::string(line.substr(0, firstSpace));
      std::transform(method.begin(), method.end(), method.begin(), ::toupper);
      msg.methodStr = method;
      msg.method = parseMethod(method);
      msg.uri = line.substr(firstSpace + 1, lastSpace - firstSpace - 1);
      msg.version = line.substr(lastSpace + 1);
    } else {
      return std::nullopt;
    }
  }

  // Headers
  int contentLength = 0;
  std::string lastName;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      break; // End of headers

    // Handle folded headers (lines starting with space/tab)
    if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
      if (!lastName.empty()) {
        auto existing = msg.getHeader(lastName).value_or("");
        // Multimap means getHeader might not be enough if we want to append to the SPECIFIC last one,
        // but for folded headers it's usually one line.
        // Actually we can just update the last inserted header if we had access to it.
        // Since we use multimap, we just replace the last value or append.
        // Better: let's re-add with appended value.
        auto it = msg.headers.find(lastName); // This might find any if multiple.
        // For simplicity, let's just append to the value of the 'last' one found.
        if (it != msg.headers.end()) {
          it->second += " " + trim(line);
        }
      }
      continue;
    }

    auto colon = line.find(':');
    if (colon != std::string::npos) {
      std::string name = trim(line.substr(0, colon));
      std::string value = trim(line.substr(colon + 1));
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
    std::vector<char> bodyBuf(contentLength);
    stream.read(bodyBuf.data(), contentLength);
    msg.body.assign(bodyBuf.data(), stream.gcount());
  }

  return msg;
}
