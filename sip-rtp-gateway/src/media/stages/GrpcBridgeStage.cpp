#include "GrpcBridgeStage.h"

GrpcBridgeStage::GrpcBridgeStage(std::shared_ptr<VoiceBotClient> client)
    : client_(client) {
  client_->setAudioCallback(
      [this](const std::string &data) { this->onBotAudio(data); });
}

void GrpcBridgeStage::onBotAudio(const std::string &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  downlinkBuffer_.insert(downlinkBuffer_.end(), data.begin(), data.end());
  
  // Bounded buffer: 2 seconds of audio at 8kHz is 16000 bytes
  if (downlinkBuffer_.size() > 16000) {
    downlinkBuffer_.erase(downlinkBuffer_.begin(), downlinkBuffer_.begin() + (downlinkBuffer_.size() - 16000));
  }
}

void GrpcBridgeStage::processUplink(std::vector<char> &audio) {
  if (client_ && !audio.empty()) {
    client_->sendAudio(audio, seq_++);
  }
}

void GrpcBridgeStage::processDownlink(std::vector<char> &audio) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (downlinkBuffer_.size() >= 160) {
    audio.clear();
    audio.insert(audio.end(), downlinkBuffer_.begin(), downlinkBuffer_.begin() + 160);
    downlinkBuffer_.erase(downlinkBuffer_.begin(), downlinkBuffer_.begin() + 160);
  }
}
