#include "AudioSocketStage.h"
#include "../../util/G711Utils.h"

AudioSocketStage::AudioSocketStage(std::shared_ptr<AudioSocketClient> client, int payloadType)
    : client_(client), payloadType_(payloadType) {
    client_->setAudioCallback([this](const std::vector<char> &data) {
        this->onAudioSocketData(data);
    });
}

void AudioSocketStage::onAudioSocketData(const std::vector<char> &data) {
    // data is PCM16 Little Endian
    // We need to encode it back to G.711 for the RTP stream
    std::vector<int16_t> pcm(data.size() / 2);
    memcpy(pcm.data(), data.data(), data.size());

    std::vector<char> encoded;
    if (payloadType_ == 0) { // PCMU
        G711Utils::encodeULaw(pcm, encoded);
    } else { // PCMA (8)
        G711Utils::encodeALaw(pcm, encoded);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    downlinkBuffer_.insert(downlinkBuffer_.end(), encoded.begin(), encoded.end());
    
    // Bounded buffer: 2 seconds of audio (8kHz * 1 byte/sample = 8000 bytes/sec)
    if (downlinkBuffer_.size() > 16000) {
        downlinkBuffer_.erase(downlinkBuffer_.begin(), downlinkBuffer_.begin() + (downlinkBuffer_.size() - 16000));
    }
}

void AudioSocketStage::processUplink(std::vector<char> &audio) {
    if (audio.empty()) return;

    // audio is G.711 (PCMU/PCMA)
    // We need to decode it to PCM16 for AudioSocket
    std::vector<int16_t> pcm;
    if (payloadType_ == 0) {
        G711Utils::decodeULaw(audio, pcm);
    } else {
        G711Utils::decodeALaw(audio, pcm);
    }

    std::vector<char> pcmBytes(pcm.size() * 2);
    memcpy(pcmBytes.data(), pcm.data(), pcmBytes.size());

    if (client_) {
        client_->sendAudio(pcmBytes);
    }
}

void AudioSocketStage::processDownlink(std::vector<char> &audio) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (downlinkBuffer_.size() >= 160) {
        audio.clear();
        audio.insert(audio.end(), downlinkBuffer_.begin(), downlinkBuffer_.begin() + 160);
        downlinkBuffer_.erase(downlinkBuffer_.begin(), downlinkBuffer_.begin() + 160);
    }
}
