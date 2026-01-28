#include "AudioSocketClient.h"
#include "../app/Logger.h"
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <fcntl.h>
#include <poll.h>

// Protocol Constants
constexpr unsigned char TYPE_UUID = 0x01;
constexpr unsigned char TYPE_AUDIO = 0x10;
constexpr unsigned char TYPE_TRANSFER = 0x02;
constexpr unsigned char TYPE_TERM = 0x00;
constexpr size_t UUID_PAYLOAD_LEN = 32;

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

    // Set non-blocking
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags < 0 || fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

    // Initial connect attempt
    int ret = ::connect(sockfd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        // Wait for connection to complete
        struct pollfd pfd;
        pfd.fd = sockfd_;
        pfd.events = POLLOUT;
        
        int pollRet = poll(&pfd, 1, 3000); // 3 second timeout
        if (pollRet <= 0) { // Timeout or error
            close(sockfd_);
            sockfd_ = -1;
            return false; 
        }

        if (pfd.revents & (POLLERR | POLLHUP)) {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        // Verify connection success
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }
    }

    // Set back to blocking for simpler read logic implementation?
    // Actually, the original implementation assumed blocking reads in readerLoop.
    // If we keep it non-blocking, we need to poll in readerLoop. 
    // Let's use poll in readerLoop for clean shutdown support.

    running_ = true;
    readerThread_ = std::thread(&AudioSocketClient::readerLoop, this);
    
    sendUuid();
    
    return true;
}

void AudioSocketClient::stop() {
    if (!running_.exchange(false)) return;

    if (sockfd_ >= 0) {
        // Send terminate packet
        unsigned char terminate[3] = {TYPE_TERM, 0, 0};
        // Best effort send, don't wait too long
        sendAll((char*)terminate, 3);
        
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
    std::ostringstream ss;
    
    // 1. Format Dialer (10 digits)
    std::string dialer = fromUser_.size() > 10 ? fromUser_.substr(fromUser_.size() - 10) : fromUser_;
    ss << std::setw(10) << std::setfill('0') << dialer;
    
    // 2. Format Epoch (7 digits)
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::string epochStr = std::to_string(epoch);
    if (epochStr.size() > 7) epochStr = epochStr.substr(epochStr.size() - 7);
    ss << std::setw(7) << std::setfill('0') << epochStr;
    
    // 3. Format Dialed (15 digits)
    std::string dialed = toUser_.size() > 15 ? toUser_.substr(toUser_.size() - 15) : toUser_;
    ss << std::setw(15) << std::setfill('0') << dialed;
    
    std::string payload = ss.str();
    
    unsigned char header[3];
    header[0] = TYPE_UUID;
    header[1] = 0x00;
    header[2] = static_cast<unsigned char>(UUID_PAYLOAD_LEN);

    std::vector<char> packet;
    packet.insert(packet.end(), header, header + 3);
    packet.insert(packet.end(), payload.begin(), payload.end());

    LOG_INFO("Sending UUID: " << payload << " for call " << callId_);
    std::lock_guard<std::mutex> lock(sendMutex_);
    sendAll(packet.data(), packet.size());
}

void AudioSocketClient::sendAudio(const std::vector<char>& pcmData) {
    if (sockfd_ < 0 || pcmData.empty()) return;

    size_t len = pcmData.size();
    if (len > 0xFFFF) len = 0xFFFF; // Cap at max 16-bit length

    unsigned char header[3];
    header[0] = TYPE_AUDIO;
    header[1] = (len >> 8) & 0xFF;
    header[2] = len & 0xFF;

    std::lock_guard<std::mutex> lock(sendMutex_);
    
    struct iovec iov[2];
    iov[0].iov_base = header;
    iov[0].iov_len = 3;
    iov[1].iov_base = (void*)pcmData.data();
    iov[1].iov_len = len;

    ssize_t totalToWrite = 3 + len;
    ssize_t written = 0;
    
    // Simple writev implementation loop
    // Note: This assumes simple blocking or partially blocking behavior.
    // For full non-blocking correctness with writev and partial writes, it gets complex.
    // However, sendAll was doing separate sends anyway.
    // Optimized sending: use writev to send header and data in one go
    ssize_t n = writev(sockfd_, iov, 2);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) return;
        n = 0;
    }
    
    written = n;
    if (written >= totalToWrite) return;
    
    // Partial write handling: send remaining data
    if (written < 3) {
        // Did not finish header
        if (sendAll((const char*)header + written, 3 - written)) {
            sendAll(pcmData.data(), len);
        }
    } else {
        // Finished header, send remaining body
        size_t bodyWritten = written - 3;
        if (bodyWritten < len) {
            sendAll(pcmData.data() + bodyWritten, len - bodyWritten);
        }
    }
}

bool AudioSocketClient::sendAll(const char* data, size_t len) {
    if (sockfd_ < 0) return false;
    
    size_t sent = 0;
    while (sent < len) {
        // We are non-blocking now, so we need to handle EAGAIN
        ssize_t n = send(sockfd_, data + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Wait for writeability
                struct pollfd pfd;
                pfd.fd = sockfd_;
                pfd.events = POLLOUT;
                poll(&pfd, 1, 100); // 100ms timeout
                continue;
            }
            return false;
        }
        sent += n;
    }
    return true;
}

void AudioSocketClient::readerLoop() {
    const int TIMEOUT_MS = 1000;
    
    while (running_) {
        struct pollfd pfd;
        pfd.fd = sockfd_;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, TIMEOUT_MS);
        if (ret < 0) {
            LOG_ERROR("Poll error in reader loop");
            break;
        }
        if (ret == 0) continue; // Timeout

        if (pfd.revents & (POLLERR | POLLHUP)) {
            LOG_INFO("AudioSocket disconnected");
            break;
        }

        if (pfd.revents & POLLIN) {
            unsigned char header[3];
            // Since we are non-blocking, we loop until we get all bytes
            size_t received = 0;
            bool error = false;
            
            while (received < 3) {
                 ssize_t n = recv(sockfd_, header + received, 3 - received, 0);
                 if (n > 0) received += n;
                 else if (n == 0) { error = true; break; } // EOF
                 else if (errno != EAGAIN && errno != EWOULDBLOCK) { error = true; break; }
                 else {
                     // Wait more
                     poll(&pfd, 1, 100);
                 }
            }
            
            if (error) break;

            unsigned char type = header[0];
            uint16_t len = (header[1] << 8) | header[2];

            if (len > 0) {
                std::vector<char> payload(len);
                size_t pReceived = 0;
                while (pReceived < len) {
                     ssize_t n = recv(sockfd_, payload.data() + pReceived, len - pReceived, 0);
                     if (n > 0) pReceived += n;
                     else if (n == 0) { error = true; break; }
                     else if (errno != EAGAIN && errno != EWOULDBLOCK) { error = true; break; }
                     else { poll(&pfd, 1, 100); }
                }
                
                if (error) break;

                if (type == TYPE_AUDIO && audioCb_) {
                    audioCb_(payload);
                } else if (type == TYPE_TRANSFER && transferCb_) {
                    std::string sipUrl(payload.begin(), payload.end());
                    LOG_INFO("AudioSocket received transfer request to: " << sipUrl);
                    transferCb_(sipUrl);
                } else if (type == TYPE_TERM) {
                    LOG_INFO("AudioSocket received terminate");
                    break;
                }
            } else if (type == TYPE_TERM) {
                LOG_INFO("AudioSocket received terminate (no payload)");
                break;
            }
        }
    }
    running_ = false;
    LOG_INFO("AudioSocket reader loop stopped for call " << callId_);
}
