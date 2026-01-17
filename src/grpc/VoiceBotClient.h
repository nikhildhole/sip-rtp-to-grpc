#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "voicebot.grpc.pb.h"
#include "voicebot.pb.h"
#include <grpcpp/grpcpp.h>

class VoiceBotClient {
public:
  using AudioCallback = std::function<void(const std::string &data)>;
  using ControlCallback = std::function<void(const voicebot::BotControl &)>;

  VoiceBotClient(const std::string &target, const std::string &callId);
  ~VoiceBotClient();

  bool connect();
  void setAudioCallback(AudioCallback cb);
  void setControlCallback(ControlCallback cb);

  // Sending to Bot
  void sendConfig(int rate, int codec); // 0=PCMU, 8=PCMA
  void sendAudio(const std::vector<char> &data, uint64_t seq);
  void sendHangup();

  void stop();

private:
  std::string target_;
  std::string callId_;

  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<voicebot::VoiceBot::Stub> stub_;
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<
      grpc::ClientReaderWriter<voicebot::CallEvent, voicebot::CallAction>>
      stream_;

  std::thread readerThread_;
  std::atomic<bool> running_{false};

  AudioCallback audioCb_;
  ControlCallback controlCb_;

  void readerLoop();
};
