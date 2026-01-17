#pragma once

#include <functional>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <string>

#include "SipMessage.h"
#include "SipParser.h"
#include "SipTransaction.h"

class SipServer {
public:
  using RequestHandler =
      std::function<void(const SipMessage &, const sockaddr_in &)>;

  SipServer(int port);
  ~SipServer();

  bool start();
  void setRequestHandler(RequestHandler handler);
  void poll(); // Called from main loop

  void sendResponse(const SipMessage &res, const sockaddr_in &dest);
  void sendRequest(const SipMessage &req, const sockaddr_in &dest);

private:
  int port_;
  int socketFd_ = -1;
  RequestHandler requestHandler_;

  // Simple transaction cache could go here or in CallRegistry
  // For this design, server just dispaches.

  char buffer_[8192];
};
