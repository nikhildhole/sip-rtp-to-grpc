#pragma once

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

class Config {
public:
  static Config &instance();
  bool load(const std::string &path);

  std::string getEffectiveBindIp() const {
    if (bindIp == "0.0.0.0" || bindIp.empty()) {
      return "127.0.0.1";
    }
    return bindIp;
  }

  std::string bindIp;
  int sipPort;
  int rtpPortStart;
  int rtpPortEnd;
  size_t maxCalls;

  std::vector<std::string> codecPreference;
  enum class GatewayMode { ECHO, AUDIOSOCKET };
  GatewayMode mode;
  std::string audiosocketTarget;
  bool recordingMode;
  std::string recordingPath;
  std::string logLevel;
};
