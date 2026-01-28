#include "RtcpPacket.h"
#include <arpa/inet.h>
#include <cstring>

void RtcpPacket::parse(size_t len) {
  size = len;
  // Basic sanity check can happen here
}

uint8_t RtcpPacket::getVersion() const {
  if (size < 1) return 0;
  return (buffer[0] >> 6) & 0x03;
}

bool RtcpPacket::getPadding() const {
  if (size < 1) return false;
  return (buffer[0] >> 5) & 0x01;
}

uint8_t RtcpPacket::getCount() const {
  if (size < 1) return 0;
  return buffer[0] & 0x1F;
}

uint8_t RtcpPacket::getPacketType() const {
  if (size < 2) return 0;
  return buffer[1];
}

uint16_t RtcpPacket::getLength() const {
  if (size < 4) return 0;
  uint16_t net;
  std::memcpy(&net, &buffer[2], sizeof(net));
  return ntohs(net);
}

uint32_t RtcpPacket::getSsrc() const {
  if (size < 8) return 0;
  uint32_t net;
  std::memcpy(&net, &buffer[4], sizeof(net)); // RTCP header is 4 bytes + SSRC 4 bytes
  return ntohl(net);
}

bool RtcpPacket::isSenderReport() const {
  return getPacketType() == 200;
}

bool RtcpPacket::isReceiverReport() const {
  return getPacketType() == 201;
}

bool RtcpPacket::isBye() const {
  return getPacketType() == 203;
}
