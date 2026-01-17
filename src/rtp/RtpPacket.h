#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

class RtpPacket {
public:
  // Fixed header is 12 bytes
  uint8_t buffer[1500]; // Max MTU usually
  size_t size = 0;

  void parse(size_t len);

  // Header accessors
  uint8_t getVersion() const;
  bool getMarker() const;
  uint8_t getPayloadType() const;
  uint16_t getSequenceNumber() const;
  uint32_t getTimestamp() const;
  uint32_t getSsrc() const;

  // Payload access
  const uint8_t *getPayload() const;
  size_t getPayloadSize() const;

  // Setters for outgoing
  void setHeader(uint8_t pt, uint16_t seq, uint32_t ts, uint32_t ssrc);
  void setPayload(const uint8_t *data, size_t len);
};
