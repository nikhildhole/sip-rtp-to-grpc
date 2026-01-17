#include "SipMessage.h"
#include <algorithm>
#include <sstream>

void SipMessage::addHeader(const std::string &name, const std::string &value) {
  std::string lowerName = name;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
  headers.insert({lowerName, value});
}

std::optional<std::string>
SipMessage::getHeader(const std::string &name) const {
  std::string lowerName = name;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
  auto it = headers.find(lowerName);
  if (it != headers.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::string SipMessage::toString() const {
  std::ostringstream oss;
  if (isRequest) {
    oss << methodStr << " " << uri << " " << version << "\r\n";
  } else {
    oss << version << " " << statusCode << " " << statusPhrase << "\r\n";
  }

  for (const auto &[rawName, value] : headers) {
    // Basic normalization for output (Camel-Case like)
    // Though lowercase is technically valid too.
    // Let's keep them as they are in the map (which is lowercase now)
    // or we could store original names too, but SIP is case-insensitive.
    oss << rawName << ": " << value << "\r\n";
  }
  // Content-Length is special, we always recalculate it.
  // Note: we might have 'content-length' in the headers map now.
  // toString should avoid double output.
  if (getHeader("Content-Length") == std::nullopt) {
      oss << "Content-Length: " << body.size() << "\r\n";
  }
  
  oss << "\r\n";
  oss << body;
  return oss.str();
}

std::string SipMessage::getCallId() const {
  return getHeader("Call-ID").value_or("");
}

std::string SipMessage::getFromTag() const {
  auto from = getHeader("From").value_or("");
  auto pos = from.find("tag=");
  if (pos != std::string::npos) {
    auto tag = from.substr(pos + 4);
    auto end = tag.find(';');
    if (end != std::string::npos) {
      return tag.substr(0, end);
    }
    return tag;
  }
  return "";
}

std::string SipMessage::getToTag() const {
  auto to = getHeader("To").value_or("");
  auto pos = to.find("tag=");
  if (pos != std::string::npos) {
    auto tag = to.substr(pos + 4);
    auto end = tag.find(';');
    if (end != std::string::npos) {
      return tag.substr(0, end);
    }
    return tag;
  }
  return "";
}

int SipMessage::getCSeq() const {
  auto cseq = getHeader("CSeq").value_or("");
  std::istringstream iss(cseq);
  int seq = 0;
  if (!(iss >> seq)) return 0;
  return seq;
}

SipMethod SipMessage::getCSeqMethod() const {
  auto cseq = getHeader("CSeq").value_or("");
  auto pos = cseq.find(' ');
  if (pos != std::string::npos) {
    std::string m = cseq.substr(pos + 1);
    if (m == "INVITE")
      return SipMethod::INVITE;
    if (m == "ACK")
      return SipMethod::ACK;
    if (m == "BYE")
      return SipMethod::BYE;
    if (m == "CANCEL")
      return SipMethod::CANCEL;
    if (m == "OPTIONS")
      return SipMethod::OPTIONS;
    if (m == "REFER")
      return SipMethod::REFER;
  }
  return SipMethod::UNKNOWN;
}

std::string SipMessage::getBranch() const {
  auto via = getHeader("Via").value_or("");
  auto pos = via.find("branch=");
  if (pos != std::string::npos) {
    // branch is usually until ; or end
    auto end = via.find(';', pos);
    if (end == std::string::npos)
      return via.substr(pos + 7);
    return via.substr(pos + 7, end - (pos + 7));
  }
  return "";
}
