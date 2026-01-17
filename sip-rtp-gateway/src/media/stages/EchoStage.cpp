#include "EchoStage.h"

void EchoStage::processUplink(std::vector<char> &audio) {
  if (!audio.empty()) {
    buffer_ = audio;
    hasData_ = true;
  }
}

void EchoStage::processDownlink(std::vector<char> &audio) {
  if (hasData_) {
    // If downlink is empty (usually is, coming from bot), fill it with echo
    if (audio.empty()) {
      audio = buffer_;
    }
    hasData_ = false;
  }
}
