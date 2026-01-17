#include "RecorderStage.h"
#include "../../app/Logger.h"
#include "../../util/G711Utils.h"
#include "../../util/WavWriter.h"
#include <filesystem>
#include <algorithm>

RecorderStage::RecorderStage(bool recordingMode,
                             const std::string &pathPrefix,
                             const std::string &callId,
                             int payloadType)
    : recordingMode_(recordingMode), payloadType_(payloadType) {
  try {
      std::filesystem::create_directories(pathPrefix);
      if (recordingMode_) {
          std::string path = pathPrefix + "/" + callId + ".mixed.wav";
          mixedFile_.open(path, std::ios::binary | std::ios::out);
          if (mixedFile_.is_open()) {
              WavWriter::writeHeader(mixedFile_, 8000, 1, 16);
          } else {
              LOG_ERROR("Failed to open mixed recording file " << path);
          }
      } else {
          std::string upPath = pathPrefix + "/" + callId + ".uplink.raw";
          std::string downPath = pathPrefix + "/" + callId + ".downlink.raw";

          uplinkFile_.open(upPath, std::ios::binary | std::ios::app);
          downlinkFile_.open(downPath, std::ios::binary | std::ios::app);

          if (!uplinkFile_.is_open() || !downlinkFile_.is_open()) {
               LOG_ERROR("Failed to open recording files for call " << callId);
          }
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
  if (mixedFile_.is_open()) {
    WavWriter::updateHeader(mixedFile_);
    mixedFile_.close();
  }
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
            
            buffer.swap(queue_);
        }
        
        for (const auto& chunk : buffer) {
            if (recordingMode_) {
                if (mixedFile_.is_open()) {
                    std::vector<int16_t> pcm;
                    if (payloadType_ == 0) {
                        G711Utils::decodeULaw(chunk.data, pcm);
                    } else {
                        G711Utils::decodeALaw(chunk.data, pcm);
                    }

                    if (chunk.isUplink) {
                        ulBuffer_.insert(ulBuffer_.end(), pcm.begin(), pcm.end());
                    } else {
                        dlBuffer_.insert(dlBuffer_.end(), pcm.begin(), pcm.end());
                    }

                    // Simple mixer: write when we have data in both or one is much ahead
                    // Real implementation would interleave based on timestamps/order.
                    // Here we just mix what's available to keep it simple but effective.
                    size_t mixLen = std::min(ulBuffer_.size(), dlBuffer_.size());
                    if (mixLen > 0) {
                        std::vector<int16_t> mixed(mixLen);
                        for (size_t i = 0; i < mixLen; ++i) {
                            int32_t sample = (int32_t)ulBuffer_[i] + (int32_t)dlBuffer_[i];
                            // Clamp to int16
                            if (sample > 32767) sample = 32767;
                            if (sample < -32768) sample = -32768;
                            mixed[i] = (int16_t)sample;
                        }
                        mixedFile_.write(reinterpret_cast<const char*>(mixed.data()), mixed.size() * sizeof(int16_t));
                        ulBuffer_.erase(ulBuffer_.begin(), ulBuffer_.begin() + mixLen);
                        dlBuffer_.erase(dlBuffer_.begin(), dlBuffer_.begin() + mixLen);
                    }
                    
                    // If one buffer is getting too large (e.g. one-way audio), flush it
                    if (ulBuffer_.size() > 8000) { // 1 second
                        mixedFile_.write(reinterpret_cast<const char*>(ulBuffer_.data()), ulBuffer_.size() * sizeof(int16_t));
                        ulBuffer_.clear();
                    }
                    if (dlBuffer_.size() > 8000) {
                        mixedFile_.write(reinterpret_cast<const char*>(dlBuffer_.data()), dlBuffer_.size() * sizeof(int16_t));
                        dlBuffer_.clear();
                    }
                }
            } else {
                if (chunk.isUplink && uplinkFile_.is_open()) {
                    uplinkFile_.write(chunk.data.data(), chunk.data.size());
                } else if (!chunk.isUplink && downlinkFile_.is_open()) {
                    downlinkFile_.write(chunk.data.data(), chunk.data.size());
                }
            }
        }
    }
}

