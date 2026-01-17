#include "SdpParser.h"
#include <iostream>
#include <sstream>

static std::string trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\r\n");
  auto end = s.find_last_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  return s.substr(start, end - start + 1);
}

std::optional<SdpSession> SdpParser::parse(const std::string &sdpBody) {
  SdpSession session;
  std::istringstream stream(sdpBody);
  std::string line;

  while (std::getline(stream, line)) {
    line = trim(line);
    if (line.empty())
      continue;

    if (line.size() < 2 || line[1] != '=')
      continue;
    char type = line[0];
    std::string value = line.substr(2);

    if (type == 'v')
      session.version = value;
    else if (type == 'o')
      session.origin = value;
    else if (type == 's')
      session.sessionName = value;
    else if (type == 'c') {
      // c=IN IP4 1.2.3.4
      if (value.find("IN IP4") != std::string::npos) {
        auto pos = value.rfind(' ');
        if (pos != std::string::npos) {
          session.connectionIp = value.substr(pos + 1);
        }
      }
    } else if (type == 'm') {
      // m=audio 12345 RTP/AVP 0 8 101
      std::istringstream mStream(value);
      SdpMedia m;
      mStream >> m.type >> m.port >> m.proto;
      int pt;
      while (mStream >> pt) {
        m.payloadTypes.push_back(pt);
      }
      session.media.push_back(m);
    } else if (type == 'a') {
      // a=rtpmap:0 PCMU/8000
      if (value.find("rtpmap:") == 0) {
        auto colon = value.find(':');
        auto space = value.find(' ');
        if (colon != std::string::npos && space != std::string::npos) {
          int pt = std::stoi(value.substr(colon + 1, space - colon - 1));
          std::string codec = value.substr(space + 1);
          // trim sampling rate
          auto slash = codec.find('/');
          if (slash != std::string::npos)
            codec = codec.substr(0, slash);
          session.rtpMap[pt] = codec;
        }
      }
    }
  }
  return session;
}
