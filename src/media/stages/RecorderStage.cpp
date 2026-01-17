#include "RecorderStage.h"
#include "../../app/Logger.h"
#include <filesystem>

RecorderStage::RecorderStage(const std::string &pathPrefix,
                             const std::string &callId) {
  try {
      std::filesystem::create_directories(pathPrefix);
      std::string upPath = pathPrefix + "/" + callId + ".uplink.raw";
      std::string downPath = pathPrefix + "/" + callId + ".downlink.raw";

      uplinkFile_.open(upPath, std::ios::binary | std::ios::app);
      downlinkFile_.open(downPath, std::ios::binary | std::ios::app);
      
      if (!uplinkFile_.is_open() || !downlinkFile_.is_open()) {
           LOG_ERROR("Failed to open recording files for call " << callId);
      }
  } catch (const std::exception& e) {
       LOG_ERROR("Failed to create recording directory: " << e.what());
  }

  running_ = true;
  worker_ = std::thread(&RecorderStage::workerLoop, this);
}

RecorderStage::~RecorderStage() {
  {
      std::lock_guard<std::mutex> lock(mutex_);
      running_ = false;
  }
  cv_.notify_one();
  
  if (worker_.joinable()) {
    worker_.join();
  }

  if (uplinkFile_.is_open())
    uplinkFile_.close();
  if (downlinkFile_.is_open())
    downlinkFile_.close();
}

void RecorderStage::processUplink(std::vector<char> &audio) {
  if (audio.empty()) return;
  
  std::lock_guard<std::mutex> lock(mutex_);
  queue_.push_back({true, audio});
  cv_.notify_one();
}

void RecorderStage::processDownlink(std::vector<char> &audio) {
  if (audio.empty()) return;

  std::lock_guard<std::mutex> lock(mutex_);
  queue_.push_back({false, audio});
  cv_.notify_one();
}

void RecorderStage::workerLoop() {
    while (true) {
        std::vector<Chunk> buffer;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
            
            if (!running_ && queue_.empty()) {
                return;
            }
            
            // Swap queue to local buffer to minimize lock time
            buffer.swap(queue_);
        }
        
        // Write to disk without lock
        for (const auto& chunk : buffer) {
            if (chunk.isUplink && uplinkFile_.is_open()) {
                uplinkFile_.write(chunk.data.data(), chunk.data.size());
            } else if (!chunk.isUplink && downlinkFile_.is_open()) {
                downlinkFile_.write(chunk.data.data(), chunk.data.size());
            }
        }
    }
}
