#pragma once

#include "stages/Stage.h"
#include <memory>
#include <vector>

class MediaPipeline {
public:
  void addStage(std::shared_ptr<Stage> stage);

  void processUplink(const std::vector<char> &input);
  std::vector<char> processDownlink();

private:
  std::vector<std::shared_ptr<Stage>> stages_;
};
