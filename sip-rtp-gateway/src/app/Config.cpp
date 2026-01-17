#include "Config.h"
#include "Logger.h"
#include <iostream>

Config &Config::instance() {
  static Config instance;
  return instance;
}

bool Config::load(const std::string &path) {
  try {
    YAML::Node config = YAML::LoadFile(path);

    bindIp = config["bind_ip"].as<std::string>("0.0.0.0");
    sipPort = config["sip_port"].as<int>(5060);
    rtpPortStart = config["rtp_port_start"].as<int>(20000);
    rtpPortEnd = config["rtp_port_end"].as<int>(30000);
    maxCalls = config["max_calls"].as<int>(200);
    grpcTarget = config["grpc_target"].as<std::string>("127.0.0.1:50051");

    if (config["codec_preference"]) {
      codecPreference =
          config["codec_preference"].as<std::vector<std::string>>();
    }


    echoMode = config["echo_mode"].as<bool>(false);
    recordingMode = config["recording_mode"].as<std::string>("OFF");
    recordingPath = config["recording_path"].as<std::string>("./recordings");
    logLevel = config["log_level"].as<std::string>("INFO");

    return true;
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to load config: " << e.what());
    return false;
  }
}
