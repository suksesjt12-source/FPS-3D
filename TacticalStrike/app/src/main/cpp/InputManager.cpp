#include "InputManager.h"

namespace ts {

void InputManager::setMoveAxis(float x, float y) {
    std::lock_guard<std::mutex> lk(mutex_);
    state_.moveAxis.x = clampf(x, -1.0f, 1.0f);
    state_.moveAxis.y = clampf(y, -1.0f, 1.0f);
}

void InputManager::addLookDelta(float dx, float dy) {
    std::lock_guard<std::mutex> lk(mutex_);
    float mult = touchSensitivity_ * (state_.aim ? adsMultiplier_ : 1.0f);
    float ySign = invertY_ ? 1.0f : -1.0f;
    state_.lookDelta.x += dx * mult;
    state_.lookDelta.y += dy * mult * ySign;
}

void InputManager::addGyroRate(float wx, float wy, float dt) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!gyroEnabled_ || dt <= 0.0f) return;
    float mult = gyroSensitivity_ * (state_.aim ? adsMultiplier_ : 0.7f);
    float ySign = invertY_ ? -1.0f : 1.0f;
    // In landscape orientation: sensor X = pitch rate, sensor Y = yaw rate (device-dependent axis mapping)
    state_.gyroDelta.x += wy * dt * mult;
    state_.gyroDelta.y += wx * dt * mult * ySign;
}

void InputManager::setSensitivity(float touchSens, float adsMult, float gyroSens, bool gyroEnabled, bool invertY) {
    std::lock_guard<std::mutex> lk(mutex_);
    touchSensitivity_ = clampf(touchSens, 0.0008f, 0.012f);
    adsMultiplier_ = clampf(adsMult, 0.2f, 1.2f);
    gyroSensitivity_ = clampf(gyroSens, 0.0f, 2.5f);
    gyroEnabled_ = gyroEnabled;
    invertY_ = invertY;
}

InputState InputManager::consumeFrameInput() {
    std::lock_guard<std::mutex> lk(mutex_);
    InputState out = state_;
    state_.lookDelta = {0, 0};
    state_.gyroDelta = {0, 0};
    state_.jumpPressed = false;
    state_.reloadPressed = false;
    state_.grenadePressed = false;
    state_.interactPressed = false;
    state_.weaponSwitchReq = -1;
    return out;
}

void InputManager::resetAll() {
    std::lock_guard<std::mutex> lk(mutex_);
    state_ = InputState{};
}

} // namespace ts
