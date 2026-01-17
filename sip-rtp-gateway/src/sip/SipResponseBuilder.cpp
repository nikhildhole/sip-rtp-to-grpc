#include "SipResponseBuilder.h"
#include <chrono>

SipMessage SipResponseBuilder::createResponse(const SipMessage &req, int code,
                                              const std::string &phrase) {
  SipMessage res;
  res.isRequest = false;
  res.version = req.version;
  res.statusCode = code;
  res.statusPhrase = phrase;

  // Copy critical headers
  auto vias = req.headers.equal_range("via");
  for (auto it = vias.first; it != vias.second; ++it) {
    res.addHeader("Via", it->second);
  }
  
  auto from = req.getHeader("From");
  if (from)
    res.addHeader("From", *from);
    
  auto to = req.getHeader("To");
  if (to) {
    std::string toVal = *to;
    if (code >= 200 && toVal.find("tag=") == std::string::npos) {
      // Simple tag generation if missing for final response
      toVal += ";tag=gen" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count() % 1000000);
    }
    res.addHeader("To", toVal);
  }
  
  auto callId = req.getHeader("Call-ID");
  if (callId)
    res.addHeader("Call-ID", *callId);
    
  auto cseq = req.getHeader("CSeq");
  if (cseq)
    res.addHeader("CSeq", *cseq);

  res.addHeader("User-Agent", "SIP-RTP-Gateway");
  // Note: Content-Length is automatically added by toString() based on body size

  return res;
}
