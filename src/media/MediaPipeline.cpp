#include "MediaPipeline.h"

void MediaPipeline::addStage(std::shared_ptr<Stage> stage) {
  stages_.push_back(stage);
}

void MediaPipeline::processUplink(const std::vector<char> &input) {
  // Pipeline: Stage1 -> Stage2 -> ...
  std::vector<char> current = input;
  for (auto &stage : stages_) {
    stage->processUplink(current);
  }
}

std::vector<char> MediaPipeline::processDownlink() {
  // Pipeline: Stage1 <- Stage2 <- ...
  // Wait, typically downlink generation starts from a source (like GrpcBridge)
  // and flows through modifiers (like Recorder).
  // Let's iterate normally: Stage 1 fills, Stage 2 detects it..
  // Or Reverse?
  // Let's assume order is [Bridge, Echo, Recorder]
  // Bridge fills downlink. Echo fills if empty. Recorder records.

  std::vector<char> current; // Start empty
  for (auto &stage : stages_) {
    stage->processDownlink(current);
  }
  return current;
}
