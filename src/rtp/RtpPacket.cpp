#include "RtpPacket.h"
#include <cstring>
#include <netinet/in.h>

void RtpPacket::parse(size_t len) { size = len; }

uint8_t RtpPacket::getVersion() const {
  if (size < 1)
    return 0;
  return (buffer[0] >> 6) & 0x03;
}

bool RtpPacket::getMarker() const {
  if (size < 2)
    return false;
  return (buffer[1] >> 7) & 0x01;
}

uint8_t RtpPacket::getPayloadType() const {
  if (size < 2)
    return 0;
  return buffer[1] & 0x7F;
}

uint16_t RtpPacket::getSequenceNumber() const {
  if (size < 4)
    return 0;
  uint16_t seq;
  memcpy(&seq, &buffer[2], 2);
  return ntohs(seq);
}

uint32_t RtpPacket::getTimestamp() const {
  if (size < 8)
    return 0;
  uint32_t ts;
  memcpy(&ts, &buffer[4], 4);
  return ntohl(ts);
}

uint32_t RtpPacket::getSsrc() const {
  if (size < 12)
    return 0;
  uint32_t ssrc;
  memcpy(&ssrc, &buffer[8], 4);
  return ntohl(ssrc);
}

const uint8_t *RtpPacket::getPayload() const {
  if (size < 12)
    return nullptr;
  // Assuming no CSRC list or extensions for simplicity in this V1
  return &buffer[12];
}

size_t RtpPacket::getPayloadSize() const {
  if (size < 12)
    return 0;
  return size - 12;
}

void RtpPacket::setHeader(uint8_t pt, uint16_t seq, uint32_t ts,
                          uint32_t ssrc) {
  // V=2, P=0, X=0, CC=0
  buffer[0] = 0x80;
  // M=0, PT
  buffer[1] = pt & 0x7F;

  uint16_t n_seq = htons(seq);
  memcpy(&buffer[2], &n_seq, 2);

  uint32_t n_ts = htonl(ts);
  memcpy(&buffer[4], &n_ts, 4);

  uint32_t n_ssrc = htonl(ssrc);
  memcpy(&buffer[8], &n_ssrc, 4);

  if (size < 12)
    size = 12;
}

void RtpPacket::setPayload(const uint8_t *data, size_t len) {
  if (len + 12 > sizeof(buffer))
    len = sizeof(buffer) - 12;
  memcpy(&buffer[12], data, len);
  size = 12 + len;
}
