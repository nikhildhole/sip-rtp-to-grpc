#include "VoiceBotClient.h"
#include "../app/Logger.h"

VoiceBotClient::VoiceBotClient(const std::string &target,
                               const std::string &callId)
    : target_(target), callId_(callId) {
  channel_ = grpc::CreateChannel(target_, grpc::InsecureChannelCredentials());
  stub_ = voicebot::VoiceBot::NewStub(channel_);
}

VoiceBotClient::~VoiceBotClient() { stop(); }

bool VoiceBotClient::connect() {
  context_ = std::make_unique<grpc::ClientContext>();
  stream_ = stub_->StreamCall(context_.get());

  if (!stream_)
    return false;

  running_ = true;
  readerThread_ = std::thread(&VoiceBotClient::readerLoop, this);
  return true;
}

void VoiceBotClient::stop() {
  if (!running_)
    return;
  running_ = false;

  // Cancel context to unblock reader
  if (context_)
    context_->TryCancel();

  if (stream_) {
    // stream_->WritesDone(); // Can deadlock if reader is blocked
    // context cancel is better
  }

  if (readerThread_.joinable()) {
    readerThread_.join();
  }
}

void VoiceBotClient::readerLoop() {
  voicebot::CallAction action;
  while (running_ && stream_->Read(&action)) {
    if (action.has_audio()) {
      if (audioCb_) {
        audioCb_(action.audio().data());
      }
    } else if (action.has_control()) {
      if (controlCb_) {
        controlCb_(action.control());
      }
    }
  }
  LOG_INFO("VoiceBot stream closed for call " << callId_);
  running_ = false;
}

void VoiceBotClient::setAudioCallback(AudioCallback cb) { audioCb_ = cb; }

void VoiceBotClient::setControlCallback(ControlCallback cb) { controlCb_ = cb; }

void VoiceBotClient::sendConfig(int rate, int codec) {
  if (!running_)
    return;
  voicebot::CallEvent event;
  event.set_call_id(callId_);
  auto cfg = event.mutable_config();
  cfg->set_sample_rate(rate);
  cfg->set_codec(codec == 0 ? voicebot::PCMU : voicebot::PCMA);
  stream_->Write(event);
}

void VoiceBotClient::sendAudio(const std::vector<char> &data, uint64_t seq) {
  if (!running_)
    return;
  voicebot::CallEvent event;
  event.set_call_id(callId_);
  auto audio = event.mutable_audio();
  audio->set_data(data.data(), data.size());
  audio->set_seq(seq);
  // Timestamp logic if needed
  stream_->Write(event);
}

void VoiceBotClient::sendHangup() {
  if (!running_)
    return;
  voicebot::CallEvent event;
  event.set_call_id(callId_);
  event.mutable_control()->set_type(voicebot::ControlEvent::HANGUP);
  stream_->Write(event);
}
