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
  std::lock_guard<std::mutex> lock(mutex_);
  // Simple linear search for now.
  // In a real high-perf scenario, we'd keep a free list or bitmap.
  for (int p = startPort_; p <= endPort_; p += 2) {
    if (activeSockets_.find(p) == activeSockets_.end()) {
      int fd = Net::createUdpSocket();
      if (fd >= 0) {
        if (Net::bindSocket(fd, "0.0.0.0", p)) {
          Net::setNonBlocking(fd);
          activeSockets_[p] = fd;

#ifdef __linux__
          struct epoll_event ev;
          ev.events = EPOLLIN;
          ev.data.fd = fd;
          // Store port in data.u64 or use map lookup later? 
          // data union only allows one. Let's store fd and lookup port in map (O(logN) is fast enough)
          // Or better: Pack port into u64? Port (16 bit) fits easily.
          ev.data.u64 = (uint64_t)p << 32 | fd; 
          
          if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
              LOG_ERROR("epoll_ctl add failed for port " << p);
              Net::closeSocket(fd);
              continue;
          }
#endif

          LOG_DEBUG("Worker " << workerId_ << " allocated port " << p << " with FD " << fd);
          LOG_INFO("Socket created: FD=" << fd << " Port=" << p);
          return p;
        } else {
          LOG_ERROR("Failed to bind socket to port " << p);
          Net::closeSocket(fd);
        }
      } else {
        LOG_ERROR("Failed to create UDP socket for port " << p);
      }
    }
  }
  LOG_ERROR("Worker " << workerId_ << " failed to allocate any port in range " << startPort_ << "-" << endPort_);
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
  }
}

void RtpWorker::setPacketHandler(PacketHandler handler) {
  std::lock_guard<std::mutex> lock(mutex_);
  handler_ = handler;
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
#ifdef __linux__
  const int MAX_EVENTS = 64;
  struct epoll_event events[MAX_EVENTS];
  
  while (running_) {
      int nfds = epoll_wait(epollFd_, events, MAX_EVENTS, 10);
      
      if (nfds > 0) {
          for (int i = 0; i < nfds; ++i) {
              int fd = (int)(events[i].data.u64 & 0xFFFFFFFF);
              int port = (int)(events[i].data.u64 >> 32);
              
              RtpPacket pkt;
              sockaddr_in sender{};
              socklen_t len = sizeof(sender);
              ssize_t n = recvfrom(fd, pkt.buffer, sizeof(pkt.buffer), 0,
                                   (struct sockaddr *)&sender, &len);
                                   
              if (n > 0) {
                pkt.parse(n);
                
                PacketHandler h;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    h = handler_;
                }
                
                if (h) {
                  h(port, pkt, sender);
                }
              }
          }
      }
  }
#else
  // Fallback for non-Linux (macOS/Development) using poll
  while (running_) {
    std::vector<struct pollfd> fds;
    std::vector<int> portMap;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      fds.reserve(activeSockets_.size());
      portMap.reserve(activeSockets_.size());
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
          RtpPacket pkt;
          sockaddr_in sender{};
          socklen_t len = sizeof(sender);
          ssize_t n = recvfrom(fds[i].fd, pkt.buffer, sizeof(pkt.buffer), 0,
                               (struct sockaddr *)&sender, &len);
          if (n > 0) {
            pkt.parse(n);
            
            PacketHandler h;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                h = handler_;
            }
            if (h) {
              h(portMap[i], pkt, sender);
            }
          }
        }
      }
    }
  }
#endif
}
