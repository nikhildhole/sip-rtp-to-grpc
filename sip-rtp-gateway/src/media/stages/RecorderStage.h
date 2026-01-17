#pragma once

#include "Stage.h"
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <condition_variable>

class RecorderStage : public Stage {
public:
  RecorderStage(const std::string &pathPrefix, const std::string &callId);
  ~RecorderStage();

  void processUplink(std::vector<char> &audio) override;
  void processDownlink(std::vector<char> &audio) override;

private:
  struct Chunk {
      bool isUplink; // true = uplink, false = downlink
      std::vector<char> data;
  };

  void workerLoop();

  std::ofstream uplinkFile_;
  std::ofstream downlinkFile_;
  
  std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<Chunk> queue_;
  bool running_ = false;
  std::thread worker_;
};
