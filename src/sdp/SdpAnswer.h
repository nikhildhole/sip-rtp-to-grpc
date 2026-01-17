#pragma once

#include "SdpParser.h"
#include <string>
#include <vector>

struct NegotiatedCodec {
  int payloadType;
  std::string name;
  int rate;
};

class SdpAnswer {
public:
  static std::string generate(const SdpSession &offer,
                              const std::string &localIp, int localPort,
                              const std::vector<std::string> &preference,
                              NegotiatedCodec &outCodec);
};
