#include "RtpWorker.h"
#include "../app/Logger.h"
#include "../util/Net.h"
#include <poll.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

RtpWorker::RtpWorker(int workerId, int startPort, int endPort)
    : workerId_(workerId), startPort_(startPort), endPort_(endPort) {
  // Initialize free list
  freePorts_.reserve((endPort_ - startPort_) / 2 + 1);
  for (int p = startPort_; p <= endPort_; p += 2) {
    freePorts_.push_back(p);
  }

#ifdef __linux__
  epollFd_ = epoll_create1(0);
  if (epollFd_ < 0) {
      LOG_ERROR("Failed to create epoll instance for worker " << workerId_);
  }
#endif
}

RtpWorker::~RtpWorker() { 
  stop(); 
#ifdef __linux__
  if (epollFd_ >= 0) {
      close(epollFd_);
  }
#endif
}

void RtpWorker::start() {
  running_ = true;
  thread_ = std::thread(&RtpWorker::loop, this);
  LOG_INFO("RtpWorker " << workerId_ << " started [Ports " << startPort_ << "-" << endPort_ << "]");
}

void RtpWorker::stop() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

int RtpWorker::allocatePort() {
  int p = -1;
  int fd = -1;

  // 1. Get a free port from the list (holding lock)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!freePorts_.empty()) {
      p = freePorts_.back();
      freePorts_.pop_back();
    }
  }

  if (p == -1) {
    LOG_ERROR("Worker " << workerId_ << " failed to allocate any port (none free)");
    return -1;
  }

  // 2. Create and bind socket (WITHOUT lock)
  // This avoids holding the lock during system calls
  fd = Net::createUdpSocket();
  int rtcpFd = -1;
  int rtcpPort = p + 1;

  if (fd >= 0) {
    if (Net::bindSocket(fd, "0.0.0.0", p)) {
        
        // Try binding RTCP port
        rtcpFd = Net::createUdpSocket();
        if (rtcpFd >= 0) {
            if (!Net::bindSocket(rtcpFd, "0.0.0.0", rtcpPort)) {
                LOG_WARN("Failed to bind RTCP socket to port " << rtcpPort);
                Net::closeSocket(rtcpFd);
                rtcpFd = -1;
                // Treat as failure to maintain pairing? 
                // Yes, fail the whole allocation.
            }
        } else {
             LOG_ERROR("Failed to create UDP socket for RTCP port " << rtcpPort);
        }
        
    } else {
        // RTP bind failed
        Net::closeSocket(fd);
        fd = -1; 
    }
  }

  // If we have both sockets (or just RTP if we decided to be soft, but plan said strict)
  // Let's be strict: Both must succeed.
  if (fd >= 0 && rtcpFd >= 0) {
      Net::setNonBlocking(fd);
      Net::setNonBlocking(rtcpFd);

#ifdef __linux__
      struct epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.fd = fd;
      ev.data.u64 = (uint64_t)p << 32 | fd; 
      
      struct epoll_event evRtcp;
      evRtcp.events = EPOLLIN;
      evRtcp.data.fd = rtcpFd;
      evRtcp.data.u64 = (uint64_t)rtcpPort << 32 | rtcpFd;

      bool ok = true;
      if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
          LOG_ERROR("epoll_ctl add failed for RTP port " << p);
          ok = false;
      } else {
          if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, rtcpFd, &evRtcp) == -1) {
             LOG_ERROR("epoll_ctl add failed for RTCP port " << rtcpPort);
             // Cleanup RTP from epoll
             epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
             ok = false;
          }
      }

      if (ok) {
          // Success! Now allow others to see these active sockets
          std::lock_guard<std::mutex> lock(mutex_);
          activeSockets_[p] = fd;
          activeSockets_[rtcpPort] = rtcpFd;
          
          LOG_DEBUG("Worker " << workerId_ << " allocated (RTP=" << p << ", RTCP=" << rtcpPort << ")");
          return p;
      } else {
          Net::closeSocket(fd);
          Net::closeSocket(rtcpFd);
          // fall through to failure
      }
#else
      // Non-linux specific (for poll)
      std::lock_guard<std::mutex> lock(mutex_);
      activeSockets_[p] = fd;
      activeSockets_[rtcpPort] = rtcpFd;
      LOG_DEBUG("Worker " << workerId_ << " allocated (RTP=" << p << ", RTCP=" << rtcpPort << ")");
      return p;
#endif

  } 
  
  // Cleanup if partial success
  if (fd >= 0) Net::closeSocket(fd);
  if (rtcpFd >= 0) Net::closeSocket(rtcpFd);

  // Failure path: return port to free list
  {
    std::lock_guard<std::mutex> lock(mutex_);
    freePorts_.push_back(p);
  }

  return -1;
}

void RtpWorker::releasePort(int port) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = activeSockets_.find(port);
  if (it != activeSockets_.end()) {
    int fd = it->second;
    
#ifdef __linux__
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
#endif

    Net::closeSocket(fd);
    activeSockets_.erase(it);
    
    // Also release RTCP if present
    auto itRtcp = activeSockets_.find(port + 1);
    if (itRtcp != activeSockets_.end()) {
        int rtcpFd = itRtcp->second;
#ifdef __linux__
        epoll_ctl(epollFd_, EPOLL_CTL_DEL, rtcpFd, nullptr);
#endif
        Net::closeSocket(rtcpFd);
        activeSockets_.erase(itRtcp);
    }
    
    // Return to free list
    freePorts_.push_back(port);
  }
}

void RtpWorker::setPacketHandler(PacketHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  handler_ = handler;
  handlersChanged_ = true;
}

void RtpWorker::setRtcpHandler(RtcpHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  rtcpHandler_ = handler;
  handlersChanged_ = true;
}

void RtpWorker::send(int localPort, const RtpPacket &packet, const sockaddr_in &dest) {
  int fd = -1;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = activeSockets_.find(localPort);
    if (it != activeSockets_.end()) {
      fd = it->second;
    }
  }

  if (fd != -1) {
    sendto(fd, packet.buffer, packet.size, 0, (struct sockaddr *)&dest, sizeof(dest));
  }
}

void RtpWorker::loop() {
  PacketHandler h;
  RtcpHandler rh;

#ifdef __linux__
  const int MAX_EVENTS = 64;
  struct epoll_event events[MAX_EVENTS];
  
  while (running_) {
      if (handlersChanged_.exchange(false)) {
          std::lock_guard<std::mutex> lock(mutex_);
          h = handler_;
          rh = rtcpHandler_;
      }

      int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, 10);
      
      if (nfds > 0) {
          for (int i = 0; i < nfds; ++i) {
              int fd = (int)(events[i].data.u64 & 0xFFFFFFFF);
              int port = (int)(events[i].data.u64 >> 32);
              
              if (port % 2 == 0) {
                  // RTP
                  RtpPacket pkt;
                  sockaddr_in sender{};
                  socklen_t len = sizeof(sender);
                  ssize_t n = recvfrom(fd, pkt.buffer, sizeof(pkt.buffer), 0,
                                       (struct sockaddr *)&sender, &len);
                                       
                  if (n > 0) {
                    pkt.parse(n);
                    if (h) {
                      h(port, pkt, sender);
                    }
                  }
              } else {
                  // RTCP
                  RtcpPacket pkt;
                   sockaddr_in sender{};
                  socklen_t len = sizeof(sender);
                  ssize_t n = recvfrom(fd, pkt.buffer, sizeof(pkt.buffer), 0,
                                       (struct sockaddr *)&sender, &len);
                  if (n > 0) {
                      pkt.parse(n);
                      if (rh) {
                          rh(port, pkt, sender);
                      }
                  }
              }
          }
      }
  }
#else
  // Fallback for non-Linux (macOS/Development) using poll
  std::vector<struct pollfd> fds;
  std::vector<int> portMap;
  fds.reserve(128); 
  portMap.reserve(128);

  while (running_) {
    if (handlersChanged_.exchange(false)) {
        std::lock_guard<std::mutex> lock(mutex_);
        h = handler_;
        rh = rtcpHandler_;
    }

    fds.clear();
    portMap.clear();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (activeSockets_.size() > fds.capacity()) {
          fds.reserve(activeSockets_.size() + 16);
          portMap.reserve(activeSockets_.size() + 16);
      }

      for (const auto &[port, fd] : activeSockets_) {
        fds.push_back({fd, POLLIN, 0});
        portMap.push_back(port);
      }
    }

    if (fds.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    int ret = ::poll(fds.data(), fds.size(), 10);
    if (ret > 0) {
      for (size_t i = 0; i < fds.size(); ++i) {
        if (fds[i].revents & POLLIN) {
          int port = portMap[i];
          int fd = fds[i].fd;
          
          if (port % 2 == 0) {
              // RTP
              RtpPacket pkt;
              sockaddr_in sender{};
              socklen_t len = sizeof(sender);
              ssize_t n = recvfrom(fd, pkt.buffer, sizeof(pkt.buffer), 0,
                                   (struct sockaddr *)&sender, &len);
              if (n > 0) {
                pkt.parse(n);
                if (h) {
                  h(port, pkt, sender);
                }
              }
          } else {
              // RTCP
              RtcpPacket pkt;
              sockaddr_in sender{};
              socklen_t len = sizeof(sender);
              ssize_t n = recvfrom(fd, pkt.buffer, sizeof(pkt.buffer), 0,
                                   (struct sockaddr *)&sender, &len);
              if (n > 0) {
                  pkt.parse(n);
                  if (rh) {
                      rh(port, pkt, sender);
                  }
              }
          }
        }
      }
    }
  }
#endif
}


