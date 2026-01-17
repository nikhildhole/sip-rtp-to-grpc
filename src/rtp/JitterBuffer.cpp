#include "JitterBuffer.h"

// Super simple implementation: Just reorder/buffer slightly.
// No fancy adaptive logic for V1.

void JitterBuffer::push(const RtpPacket &pkt) {
  std::lock_guard<std::mutex> lock(mutex_);

  uint16_t seq = pkt.getSequenceNumber();

  if (buffer_.empty()) {
    buffer_.push_back(pkt);
    return;
  }

  // Basic ordering insertion with wrapping awareness
  for (auto it = buffer_.begin(); it != buffer_.end(); ++it) {
    uint16_t cur = it->getSequenceNumber();
    if (seq == cur)
      return; // Duplicate

    // If seq is "before" cur (taking wrapping into account)
    // RFC 3550 style comparison: (uint16_t)(cur - seq) < 32768
    if ((uint16_t)(cur - seq) < 32768) {
      buffer_.insert(it, pkt);
      return;
    }
  }
  buffer_.push_back(pkt);
}

std::optional<RtpPacket> JitterBuffer::pop() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (buffer_.empty()) return std::nullopt;

  // If buffer is large enough, or if the head packet is sufficiently "old" 
  // (to avoid trapping at end of stream).
  // For simplicity without a timer, we can just look at size or 
  // a "force pop" flag if we had one.
  // Better: pop if size > target, OR if we haven't seen a push in X ms.
  // Actually, since we only call pop() after a push() or in a timer,
  // let's just make it return the front if it's over TARGET_SIZE.
  
  if (buffer_.size() >= TARGET_SIZE) {
    RtpPacket pkt = buffer_.front();
    buffer_.pop_front();
    return pkt;
  }
  
  // To fix trapping: If we are calling this and the buffer isn't growing, 
  // we need a hint. For now, let's just return if size > 0 
  // but this would mean no jitter protection.
  // Real fix requires a playout timer.
  // Minimal fix: if TARGET_SIZE is reached, pop. 
  // If we want to avoid trapping the last 5 packets, we need to know 
  // when the stream ends.
  
  return std::nullopt;
}
