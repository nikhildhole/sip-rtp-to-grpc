#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

struct SdpMedia {
  std::string type; // audio
  int port = 0;
  std::string proto; // RTP/AVP
  std::vector<int> payloadTypes;
};

struct SdpSession {
  std::string version;
  std::string origin;
  std::string sessionName;
  std::string connectionIp;
  std::vector<SdpMedia> media;
  std::map<int, std::string> rtpMap; // payload -> codec name
  std::map<int, int> fmtp;           // payload -> fmtp (simplified)
};

class SdpParser {
public:
  static std::optional<SdpSession> parse(const std::string &sdpBody);
};
