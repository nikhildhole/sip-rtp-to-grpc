#pragma once

#include "RtpPacket.h"
#include <deque>
#include <mutex>
#include <optional>

class JitterBuffer {
public:
  void push(const RtpPacket &pkt);
  std::optional<RtpPacket> pop();

private:
  std::deque<RtpPacket> buffer_;
  std::mutex mutex_;

  // very consistent size
  const size_t TARGET_SIZE = 5;
};
