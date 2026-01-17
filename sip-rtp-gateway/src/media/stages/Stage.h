#pragma once

#include <memory>
#include <vector>

class Stage {
public:
  virtual ~Stage() = default;

  // Process audio chunk (uplink: IP -> Bot, downlink: Bot -> IP)
  // For simplicity, we pass raw payload.
  // Return modified or same payload.
  virtual void processUplink(std::vector<char> &audio) = 0;
  virtual void processDownlink(std::vector<char> &audio) = 0;
};
