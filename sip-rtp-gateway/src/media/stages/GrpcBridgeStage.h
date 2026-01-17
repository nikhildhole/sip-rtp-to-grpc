#pragma once

#include "../../grpc/VoiceBotClient.h"
#include "Stage.h"
#include <deque>
#include <mutex>

class GrpcBridgeStage : public Stage {
public:
  GrpcBridgeStage(std::shared_ptr<VoiceBotClient> client);

  void processUplink(std::vector<char> &audio) override;
  void processDownlink(std::vector<char> &audio) override;

  // Callback from gRPC client to fill buffer
  void onBotAudio(const std::string &data);

private:
  std::shared_ptr<VoiceBotClient> client_;

  std::mutex mutex_;
  std::deque<char> downlinkBuffer_;
  uint64_t seq_ = 0;
};
