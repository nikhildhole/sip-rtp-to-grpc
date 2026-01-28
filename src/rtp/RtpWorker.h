#pragma once

#include "RtpPacket.h"
#include "RtcpPacket.h"
#include <atomic>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <netinet/in.h>
#include <thread>
#include <vector>

class RtpWorker {
public:
  using PacketHandler = std::function<void(int localPort, const RtpPacket &,
                                           const sockaddr_in &)>;
  using RtcpHandler = std::function<void(int localPort, const RtcpPacket &,
                                           const sockaddr_in &)>;

  RtpWorker(int workerId, int startPort, int endPort);
  ~RtpWorker();

  void start();
  void stop();

  // Thread-safe methods called from other threads (mostly RtpServer/Main)
  int allocatePort();
  void releasePort(int port);
  void send(int localPort, const RtpPacket &packet, const sockaddr_in &dest);

  void setPacketHandler(PacketHandler handler);
  void setRtcpHandler(RtcpHandler handler);
  
  int getStartPort() const { return startPort_; }
  int getEndPort() const { return endPort_; }

private:
  void loop();

  int workerId_;
  int startPort_;
  int endPort_;

  std::atomic<bool> running_{false};
  std::thread thread_;

  std::mutex mutex_;
  std::vector<int> freePorts_;
  std::unordered_map<int, int> activeSockets_; // port -> fd
  PacketHandler handler_;
  RtcpHandler rtcpHandler_;
  std::atomic<bool> handlersChanged_{true};

#ifdef __linux__
  int epollFd_ = -1;
#endif
};
