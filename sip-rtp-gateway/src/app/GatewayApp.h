#pragma once

#include "../rtp/RtpPacket.h"
#include "../sip/SipServer.h"
#include "../sip/SipTransaction.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

class GatewayApp {
public:
  GatewayApp();
  ~GatewayApp();

  bool init(const std::string &configPath);
  void run();

private:
  void handleSipMessage(const SipMessage &msg, const sockaddr_in &sender);
  void handleRtpPacket(int localPort, const RtpPacket &pkt,
                      const sockaddr_in &sender);
  void cliLoop();

  std::unique_ptr<SipServer> sipServer_;
  std::atomic<bool> running_{false};
  std::thread cliThread_;

  std::mutex transactionsMutex_;
  std::map<std::string, std::shared_ptr<SipTransaction>> transactions_;
  
  void cleanupTransactions();
};
