#pragma once

#include <netinet/in.h>
#include <string>

class Net {
public:
  static int createUdpSocket();
  static bool bindSocket(int fd, const std::string &ip, int port);
  static bool setNonBlocking(int fd);
  static void closeSocket(int fd);

  // address helper
  static std::string ipFromSockAddr(const sockaddr_in &addr);
  static int portFromSockAddr(const sockaddr_in &addr);
};
