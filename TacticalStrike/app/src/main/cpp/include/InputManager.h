#pragma once

#include "Math3D.h"
#include <mutex>

namespace ts {

struct InputState {
    Vec2 moveAxis{0, 0};     // x=strafe[-1..1], y=forward[-1..1]
    Vec2 lookDelta{0, 0};    // accumulated look delta in radians
    Vec2 gyroDelta{0, 0};    // accumulated gyroscope delta
    bool sprint = false;
    bool fire = false;
    bool aim = false;
    bool crouch = false;
    bool jumpPressed = false;
    bool reloadPressed = false;
    bool grenadePressed = false;
    bool interactPressed = false;
    int weaponSwitchReq = -1; // -1 none, 0..2 slot, 99 next
};

class InputManager {
public:
    void setMoveAxis(float x, float y);
    void addLookDelta(float dx, float dy);
    void addGyroRate(float wx, float wy, float dt);
    void setSprint(bool active) { std::lock_guard<std::mutex> lk(mutex_); state_.sprint = active; }
    void setFire(bool active) { std::lock_guard<std::mutex> lk(mutex_); state_.fire = active; }
    void setAim(bool active) { std::lock_guard<std::mutex> lk(mutex_); state_.aim = active; }
    void toggleCrouch() { std::lock_guard<std::mutex> lk(mutex_); state_.crouch = !state_.crouch; }
    void setCrouch(bool active) { std::lock_guard<std::mutex> lk(mutex_); state_.crouch = active; }
    void triggerJump() { std::lock_guard<std::mutex> lk(mutex_); state_.jumpPressed = true; }
    void triggerReload() { std::lock_guard<std::mutex> lk(mutex_); state_.reloadPressed = true; }
    void triggerGrenade() { std::lock_guard<std::mutex> lk(mutex_); state_.grenadePressed = true; }
    void triggerInteract() { std::lock_guard<std::mutex> lk(mutex_); state_.interactPressed = true; }
    void requestWeaponSwitch(int slot) { std::lock_guard<std::mutex> lk(mutex_); state_.weaponSwitchReq = slot; }

    void setSensitivity(float touchSens, float adsMult, float gyroSens, bool gyroEnabled, bool invertY);

    InputState consumeFrameInput();
    void resetAll();

private:
    std::mutex mutex_;
    InputState state_;
    float touchSensitivity_ = 0.0032f;
    float adsMultiplier_ = 0.55f;
    float gyroSensitivity_ = 0.85f;
    bool gyroEnabled_ = true;
    bool invertY_ = false;
};

} // namespace ts
