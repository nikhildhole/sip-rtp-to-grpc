#pragma once

#include <cstddef>
#include <cstdint>

class RtcpPacket {
public:
  uint8_t buffer[1500];
  size_t size = 0;

  void parse(size_t len);

  // Common Header
  uint8_t getVersion() const;
  bool getPadding() const;
  uint8_t getCount() const; // RC/SC
  uint8_t getPacketType() const;
  uint16_t getLength() const; // length in 32-bit words - 1
  uint32_t getSsrc() const; // Sending SSRC

  // Helpers
  bool isSenderReport() const;
  bool isReceiverReport() const;
  bool isBye() const;
};
