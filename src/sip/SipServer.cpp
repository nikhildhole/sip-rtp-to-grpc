#include "SipServer.h"
#include "../app/Logger.h"
#include "../util/Net.h"
#include <cstring>
#include <sys/socket.h>
#include <poll.h>

SipServer::SipServer(int port) : port_(port) {}

SipServer::~SipServer() { Net::closeSocket(socketFd_); }

bool SipServer::start() {
  socketFd_ = Net::createUdpSocket();
  if (socketFd_ < 0)
    return false;

  if (!Net::bindSocket(socketFd_, "0.0.0.0", port_)) {
    return false;
  }

  Net::setNonBlocking(socketFd_);
  LOG_INFO("SIP Server listening on port " << port_);
  return true;
}

void SipServer::setRequestHandler(RequestHandler handler) {
  requestHandler_ = handler;
}

void SipServer::poll(int timeoutMs) {
  struct pollfd pfd;
  pfd.fd = socketFd_;
  pfd.events = POLLIN;

  int ret = ::poll(&pfd, 1, timeoutMs);
  if (ret > 0 && (pfd.revents & POLLIN)) {
    while (true) { // Drain all available packets
        sockaddr_in senderAddr{};
        socklen_t addrLen = sizeof(senderAddr);
    
        ssize_t n = recvfrom(socketFd_, buffer_, sizeof(buffer_) - 1, 0,
                             (struct sockaddr *)&senderAddr, &addrLen);
        if (n > 0) {
          if (n < 32 && (buffer_[0] == '\r' || buffer_[0] == '\n' || buffer_[0] == ' ')) {
              continue; // Silence keep-alives or stray empty packets
          }
          buffer_[n] = '\0';
          LOG_INFO("Received UDP packet from "
                    << Net::ipFromSockAddr(senderAddr) << ":"
                    << Net::portFromSockAddr(senderAddr));
          
          std::string debugMsg(buffer_, std::min<size_t>(n, 256));
          LOG_DEBUG("SIP Packet (truncated): " << debugMsg);
    
          auto msg = SipParser::parse(buffer_, n);
          if (msg) {
            if (requestHandler_) {
              requestHandler_(*msg, senderAddr);
            }
          } else {
            LOG_WARN("Failed to parse SIP message");
          }
        } else if (n < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break; // Normal for non-blocking
          } else {
            LOG_ERROR("SIP recvfrom error: " << strerror(errno));
            break;
          }
        } else {
          // n == 0 for UDP means empty datagram
          break;
        }
        
        // If we processed a packet, check if there are more immediately
        // Note: recvfrom on non-blocking socket returns EAGAIN if empty, so the loop handles it.
    }
  } else if (ret < 0) {
      if (errno != EINTR) { 
          LOG_ERROR("SIP poll error: " << strerror(errno));
      }
  }
}

void SipServer::sendResponse(const SipMessage &res, const sockaddr_in &dest) {
  std::string raw = res.toString();
  LOG_INFO("Sending SIP Response to " << Net::ipFromSockAddr(dest) << ":"
                                       << Net::portFromSockAddr(dest));
  std::string debugMsg = raw.substr(0, std::min<size_t>(raw.size(), 256));
  LOG_DEBUG("SIP Out (truncated): " << debugMsg);
  sendto(socketFd_, raw.c_str(), raw.size(), 0, (struct sockaddr *)&dest,
         sizeof(dest));
}

void SipServer::sendRequest(const SipMessage &req, const sockaddr_in &dest) {
  std::string raw = req.toString();
  LOG_INFO("Sending SIP Request to " << Net::ipFromSockAddr(dest) << ":"
                                      << Net::portFromSockAddr(dest));
  std::string debugMsg = raw.substr(0, std::min<size_t>(raw.size(), 256));
  LOG_DEBUG("SIP Out (truncated): " << debugMsg);
  sendto(socketFd_, raw.c_str(), raw.size(), 0, (struct sockaddr *)&dest,
         sizeof(dest));
}
