#pragma once

#include <functional>
#include <string>

enum class CallState { IDLE, TRYING, RINGING, ACTIVE, ENDING, ENDED };

class CallStateMachine {
public:
  using StateCallback = std::function<void(CallState, CallState)>; // old, new

  CallStateMachine();

  CallState getState() const { return state_; }
  void setState(CallState newState);

  void setCallback(StateCallback cb);

  // Helpers
  bool isActive() const;
  bool isEnded() const;

private:
  CallState state_ = CallState::IDLE;
  StateCallback callback_;
};
