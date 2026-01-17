#pragma once

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

class Config {
public:
  static Config &instance();
  bool load(const std::string &path);

  std::string bindIp;
  int sipPort;
  int rtpPortStart;
  int rtpPortEnd;
  int maxCalls;
  std::string grpcTarget;
  std::vector<std::string> codecPreference;
  enum class GatewayMode { ECHO, GRPC, AUDIOSOCKET };
  GatewayMode mode;
  std::string audiosocketTarget;
  bool recordingMode;
  std::string recordingPath;
  std::string logLevel;
};
