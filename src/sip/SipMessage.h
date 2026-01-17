#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

enum class SipMethod { INVITE, ACK, BYE, CANCEL, OPTIONS, REFER, UNKNOWN };

class SipMessage {
public:
  bool isRequest = false;

  // Request Line
  SipMethod method = SipMethod::UNKNOWN;
  std::string methodStr;
  std::string uri;
  std::string version = "SIP/2.0";

  // Status Line (if response)
  int statusCode = 0;
  std::string statusPhrase;

  // Headers
  std::multimap<std::string, std::string> headers;

  // Body
  std::string body;

  // Helpers
  void addHeader(const std::string &name, const std::string &value);
  std::optional<std::string> getHeader(const std::string &name) const;
  std::string toString() const;

  // Common Header Parsing helpers
  std::string getCallId() const;
  std::string getFromTag() const;
  std::string getToTag() const;
  int getCSeq() const;
  SipMethod getCSeqMethod() const;
  std::string getBranch() const;
  std::string getFromUser() const;
  std::string getToUser() const;
};
