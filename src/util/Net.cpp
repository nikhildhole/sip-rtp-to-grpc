#include "Net.h"
#include "../app/Logger.h"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

int Net::createUdpSocket() {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    LOG_ERROR("Failed to create socket");
    return fd;
  }
  
  // Enable address reuse
  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    LOG_WARN("Failed to set SO_REUSEADDR");
  }
  
  return fd;
}

bool Net::bindSocket(int fd, const std::string &ip, int port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
    LOG_ERROR("Invalid IP address: " << ip);
    return false;
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    LOG_ERROR("Bind failed on " << ip << ":" << port << " error: " << strerror(errno));
    return false;
  }
  return true;
}

bool Net::setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    LOG_ERROR("fcntl F_GETFL failed: " << strerror(errno));
    return false;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    LOG_ERROR("fcntl F_SETFL O_NONBLOCK failed: " << strerror(errno));
    return false;
  }
  return true;
}

void Net::closeSocket(int fd) {
  if (fd >= 0)
    close(fd);
}

std::string Net::ipFromSockAddr(const sockaddr_in &addr) {
  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
  return std::string(ip);
}

int Net::portFromSockAddr(const sockaddr_in &addr) {
  return ntohs(addr.sin_port);
}
