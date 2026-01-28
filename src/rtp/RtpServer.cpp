#include "RtpServer.h"
#include "../app/Logger.h"
#include <thread>

RtpServer &RtpServer::instance() {
  static RtpServer instance;
  return instance;
}

void RtpServer::init(int startPort, int endPort, int threadCount) {
  startPort_ = startPort;
  endPort_ = endPort;

  if (threadCount <= 0) {
    threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 4; // Fallback
  }
  
  // Reserve 1 core for SIP/Main? 
  // User asked for efficient use of 16 cores. 
  // Using all 16 is fine, OS scheduler handles overlap with low-duty SIP thread.
  // Actually, creating 16 polling threads + 1 main thread = 17 threads.
  // If 16 physical cores, this is okay.
  
  int portRange = endPort - startPort + 1;
  int portsPerWorker = portRange / threadCount;
  
  LOG_INFO("Initializing RtpServer with " << threadCount << " workers. Ports per worker: " << portsPerWorker);

  for (int i = 0; i < threadCount; ++i) {
    int wStart = startPort + (i * portsPerWorker);
    int wEnd = wStart + portsPerWorker - 1;
    // Align to even start (RTP)
    if (wStart % 2 != 0) wStart++;
    
    // Ensure last worker takes up to endPort
    if (i == threadCount - 1) {
        wEnd = endPort;
    }
    
    auto worker = std::make_unique<RtpWorker>(i, wStart, wEnd);
    worker->start();
    workers_.push_back(std::move(worker));
  }
}

int RtpServer::allocatePort() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Round robin try
  int startWorker = nextWorker_;
  do {
    int port = workers_[nextWorker_]->allocatePort();
    nextWorker_ = (nextWorker_ + 1) % workers_.size();
    if (port > 0) {
      return port;
    }
  } while (nextWorker_ != startWorker);
  
  LOG_ERROR("All RTP workers full or out of ports");
  return -1;
}

void RtpServer::releasePort(int port) {
  // Find worker
  for (auto &w : workers_) {
    if (port >= w->getStartPort() && port <= w->getEndPort()) {
      w->releasePort(port);
      return;
    }
  }
}

void RtpServer::setPacketHandler(PacketHandler handler) {
  // Propagate to all existing workers (init should be called first usually)
  for (auto &w : workers_) {
    w->setPacketHandler(handler);
  }
}

void RtpServer::setRtcpHandler(RtcpHandler handler) {
  for (auto &w : workers_) {
    w->setRtcpHandler(handler);
  }
}

void RtpServer::send(int localPort, const RtpPacket &pkt,
                     const sockaddr_in &dest) {
   for (auto &w : workers_) {
    if (localPort >= w->getStartPort() && localPort <= w->getEndPort()) {
      w->send(localPort, pkt, dest);
      return;
    }
  }
}
