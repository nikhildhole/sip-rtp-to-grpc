#pragma once

#include <atomic>

class SignalHandler {
public:
  static void init();
  static bool shouldExit();
  static void setExit();

private:
  static void handleSignal(int signum);
  static std::atomic<bool> exitFlag_;
};
