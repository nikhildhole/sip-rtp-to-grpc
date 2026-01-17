#pragma once

#include "Stage.h"

// In Echo mode, Uplink is copied to Downlink queue if pipeline allows,
// or we just rely on the pipeline to route it.
// Actually, echo means: Valid Audio from Source -> Send back to Source.
// In the pipeline model:
// Pipeline receives RTP -> processUplink -> ...
// Pipeline sends RTP <- processDownlink <- ...
//
// EchoStage needs to inject uplink audio into downlink stream.

class EchoStage : public Stage {
public:
  void processUplink(std::vector<char> &audio) override;
  void processDownlink(std::vector<char> &audio) override;

  // Thread safe single frame buffer
  std::vector<char> buffer_;
  bool hasData_ = false;
};
