#pragma once

#include "Stage.h"
#include "../../audiosocket/AudioSocketClient.h"
#include <memory>
#include <mutex>

class AudioSocketStage : public Stage {
public:
    using TransferCallback = std::function<void(const std::string&)>;

    AudioSocketStage(std::shared_ptr<AudioSocketClient> client, int payloadType);
    
    void processUplink(std::vector<char> &audio) override;
    void processDownlink(std::vector<char> &audio) override;

    void setTransferCallback(TransferCallback cb) { transferCb_ = cb; }

private:
    void onAudioSocketData(const std::vector<char> &data);

    std::shared_ptr<AudioSocketClient> client_;
    TransferCallback transferCb_;
    int payloadType_;
    std::vector<char> downlinkBuffer_;
    std::mutex mutex_;
};
