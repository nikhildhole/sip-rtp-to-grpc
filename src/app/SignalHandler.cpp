#include "SignalHandler.h"
#include "Logger.h"
#include <csignal>

std::atomic<bool> SignalHandler::exitFlag_(false);

void SignalHandler::init() {
  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);
}

bool SignalHandler::shouldExit() { return exitFlag_; }

void SignalHandler::handleSignal(int signum) {
  LOG_INFO("Received signal " << signum << ", exiting...");
  setExit();
}

void SignalHandler::setExit() {
  exitFlag_ = true;
}
