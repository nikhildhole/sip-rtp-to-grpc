#include "AudioSocketClient.h"
#include "../app/Logger.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <chrono>

AudioSocketClient::AudioSocketClient(const std::string& target, const std::string& callId, 
                                     const std::string& fromUser, const std::string& toUser)
    : target_(target), callId_(callId), fromUser_(fromUser), toUser_(toUser) {}

AudioSocketClient::~AudioSocketClient() {
    stop();
}

bool AudioSocketClient::connect() {
    size_t colonPos = target_.find(':');
    if (colonPos == std::string::npos) return false;

    std::string ip = target_.substr(0, colonPos);
    int port = std::stoi(target_.substr(colonPos + 1));

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) return false;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

    if (::connect(sockfd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    running_ = true;
    readerThread_ = std::thread(&AudioSocketClient::readerLoop, this);
    
    sendUuid();
    
    return true;
}

void AudioSocketClient::stop() {
    if (!running_) return;
    running_ = false;

    if (sockfd_ >= 0) {
        // Send terminate packet 0x00 0x00 0x00
        char terminate[3] = {0, 0, 0};
        sendAll(terminate, 3);
        shutdown(sockfd_, SHUT_RDWR);
        close(sockfd_);
        sockfd_ = -1;
    }

    if (readerThread_.joinable()) {
        readerThread_.join();
    }
}

void AudioSocketClient::sendUuid() {
    // Required format: dialer (10) + epoch (7) + dialed (15) = 32 chars
    
    // 1. Format Dialer (10 digits, zero-padded)
    std::string dialer = fromUser_;
    if (dialer.size() > 10) dialer = dialer.substr(dialer.size() - 10);
    std::ostringstream ds;
    ds << std::setw(10) << std::setfill('0') << dialer;
    
    // 2. Format Epoch (7 digits)
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::string epochStr = std::to_string(epoch);
    if (epochStr.size() > 7) epochStr = epochStr.substr(epochStr.size() - 7);
    std::ostringstream es;
    es << std::setw(7) << std::setfill('0') << epochStr;
    
    // 3. Format Dialed (15 digits, zero-padded)
    std::string dialed = toUser_;
    if (dialed.size() > 15) dialed = dialed.substr(dialed.size() - 15);
    std::ostringstream dds;
    dds << std::setw(15) << std::setfill('0') << dialed;
    
    std::string payload = ds.str() + es.str() + dds.str();
    
    unsigned char header[3];
    header[0] = 0x01; // UUID Type
    header[1] = 0x00;
    header[2] = 0x20; // Length 32 (0x20)

    std::vector<char> packet;
    packet.insert(packet.end(), header, header + 3);
    packet.insert(packet.end(), payload.begin(), payload.end());

    LOG_INFO("Sending UUID: " << payload << " for call " << callId_);
    sendAll(packet.data(), packet.size());
}

void AudioSocketClient::sendAudio(const std::vector<char>& pcmData) {
    if (sockfd_ < 0 || pcmData.empty()) return;

    size_t len = pcmData.size();
    if (len > 0xFFFF) len = 0xFFFF; // Cap at max 16-bit length

    unsigned char header[3];
    header[0] = 0x10; // Audio Type
    header[1] = (len >> 8) & 0xFF;
    header[2] = len & 0xFF;

    std::lock_guard<std::mutex> lock(sendMutex_);
    sendAll((const char*)header, 3);
    sendAll(pcmData.data(), len);
}

bool AudioSocketClient::sendAll(const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd_, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

void AudioSocketClient::readerLoop() {
    while (running_) {
        unsigned char header[3];
        ssize_t n = recv(sockfd_, header, 3, MSG_WAITALL);
        if (n <= 0) break;

        unsigned char type = header[0];
        uint16_t len = (header[1] << 8) | header[2];

        if (len > 0) {
            std::vector<char> payload(len);
            ssize_t pn = recv(sockfd_, payload.data(), len, MSG_WAITALL);
            if (pn <= 0) break;

            if (type == 0x10 && audioCb_) {
                audioCb_(payload);
            } else if (type == 0x00) {
                LOG_INFO("AudioSocket received terminate");
                break;
            }
        } else if (type == 0x00) {
            LOG_INFO("AudioSocket received terminate (no payload)");
            break;
        }
    }
    running_ = false;
    LOG_INFO("AudioSocket reader loop stopped for call " << callId_);
}
