#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <netinet/in.h>

class AudioSocketClient {
public:
    using AudioCallback = std::function<void(const std::vector<char>&)>;
    using TransferCallback = std::function<void(const std::string&)>;

    AudioSocketClient(const std::string& target, const std::string& callId, 
                       const std::string& fromUser, const std::string& toUser);
    ~AudioSocketClient();

    bool connect();
    void stop();

    void sendAudio(const std::vector<char>& pcmData);
    void sendUuid();
    void setAudioCallback(AudioCallback cb) { audioCb_ = cb; }
    void setTransferCallback(TransferCallback cb) { transferCb_ = cb; }

private:
    void readerLoop();
    bool sendAll(const char* data, size_t len);

    std::string target_;
    std::string callId_;
    std::string fromUser_;
    std::string toUser_;
    int sockfd_ = -1;
    std::atomic<bool> running_{false};
    std::thread readerThread_;
    AudioCallback audioCb_;
    TransferCallback transferCb_;
    std::mutex sendMutex_;
};
