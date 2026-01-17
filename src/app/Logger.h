#pragma once

#include <iostream>
#include <mutex>
#include <string>
#include <sstream>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& instance();
    
    void setLevel(LogLevel level);
    void log(LogLevel level, const std::string& msg);

    bool isEnabled(LogLevel level); 

private:
    Logger() = default;
    LogLevel currentLevel_ = LogLevel::INFO;
    std::mutex mutex_;
};


#define LOG_DEBUG(msg) \
  if (Logger::instance().isEnabled(LogLevel::DEBUG)) { \
    std::ostringstream oss; \
    oss << msg; \
    Logger::instance().log(LogLevel::DEBUG, oss.str()); \
  }

#define LOG_INFO(msg) \
  if (Logger::instance().isEnabled(LogLevel::INFO)) { \
    std::ostringstream oss; \
    oss << msg; \
    Logger::instance().log(LogLevel::INFO, oss.str()); \
  }

#define LOG_WARN(msg) \
  if (Logger::instance().isEnabled(LogLevel::WARN)) { \
    std::ostringstream oss; \
    oss << msg; \
    Logger::instance().log(LogLevel::WARN, oss.str()); \
  }

#define LOG_ERROR(msg) \
  if (Logger::instance().isEnabled(LogLevel::ERROR)) { \
    std::ostringstream oss; \
    oss << msg; \
    Logger::instance().log(LogLevel::ERROR, oss.str()); \
  }