#include "CallStateMachine.h"
#include "../app/Logger.h"

CallStateMachine::CallStateMachine() {}

void CallStateMachine::setState(CallState newState) {
  if (state_ == newState)
    return;

  CallState old = state_;
  state_ = newState;

  LOG_DEBUG("Call State Changed: " << (int)old << " -> " << (int)newState);
  if (callback_) {
    callback_(old, newState);
  }
}

void CallStateMachine::setCallback(StateCallback cb) { callback_ = cb; }

bool CallStateMachine::isActive() const { return state_ == CallState::ACTIVE; }

bool CallStateMachine::isEnded() const { return state_ == CallState::ENDED; }
