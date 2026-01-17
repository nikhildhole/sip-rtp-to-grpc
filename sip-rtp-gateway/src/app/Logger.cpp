#include "Logger.h"
#include <chrono>
#include <iomanip>

Logger &Logger::instance() {
  static Logger instance;
  return instance;
}

void Logger::setLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  currentLevel_ = level;
}

void Logger::log(LogLevel level, const std::string &msg) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::cout << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ";

  switch (level) {
  case LogLevel::DEBUG:
    std::cout << "[DEBUG] ";
    break;
  case LogLevel::INFO:
    std::cout << "[INFO]  ";
    break;
  case LogLevel::WARN:
    std::cout << "[WARN]  ";
    break;
  case LogLevel::ERROR:
    std::cout << "[ERROR] ";
    break;
  }

  std::cout << msg << std::endl;
}

bool Logger::isEnabled(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  return level >= currentLevel_;
}
