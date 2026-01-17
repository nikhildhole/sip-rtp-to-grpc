#include "SdpAnswer.h"
#include "../app/Logger.h"
#include <sstream>

std::string SdpAnswer::generate(const SdpSession &offer,
                                const std::string &localIp, int localPort,
                                const std::vector<std::string> &preference,
                                NegotiatedCodec &outCodec) {
  outCodec = {-1, "", 0};

  // Find audio media
  const SdpMedia *audio = nullptr;
  for (const auto &m : offer.media) {
    if (m.type == "audio") {
      audio = &m;
      break;
    }
  }

  if (!audio)
    return "";

  // Negotiate Codec
  for (const auto &pref : preference) {
    for (int pt : audio->payloadTypes) {
      std::string name;
      auto it = offer.rtpMap.find(pt);
      if (it != offer.rtpMap.end()) {
        name = it->second;
      } else {
        // Static defaults
        if (pt == 0)
          name = "PCMU";
        if (pt == 8)
          name = "PCMA";
      }

      if (name == pref) {
        outCodec.payloadType = pt;
        outCodec.name = name;
        outCodec.rate = 8000;
        goto codec_found;
      }
    }
  }
codec_found:

  if (outCodec.payloadType == -1) {
    LOG_WARN("No matching codec found");
    return "";
  }

  // Construct Answer
  std::ostringstream oss;
  oss << "v=0\r\n";
  oss << "o=- 123456 123456 IN IP4 " << localIp << "\r\n";
  oss << "s=Gateway\r\n";
  oss << "c=IN IP4 " << localIp << "\r\n";
  oss << "t=0 0\r\n";

  oss << "m=audio " << localPort << " RTP/AVP " << outCodec.payloadType << "\r\n";

  oss << "a=rtpmap:" << outCodec.payloadType << " " << outCodec.name << "/"
      << outCodec.rate << "\r\n";
  oss << "a=sendrecv\r\n";

  return oss.str();
}
