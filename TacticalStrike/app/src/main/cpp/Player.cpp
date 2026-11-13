#include "PlayerController.h"
#include <algorithm>
#include <cmath>

namespace ts {

void PlayerController::spawnAt(const Vec3& feetPos, float yawRad) {
    position_ = feetPos;
    velocity_ = {0, 0, 0};
    yaw_ = yawRad;
    pitch_ = 0.0f;
    health_ = maxHealth_;
    armor_ = 50.0f;
    stamina_ = 100.0f;
    currentEyeHeight_ = 1.68f;
    crouching_ = false;
    sprinting_ = false;
    grounded_ = true;
    damageFlash_ = 0.0f;
    landDip_ = 0.0f;
    stepTimer_ = 0.0f;
}

Vec3 PlayerController::getEyePosition() const {
    return {position_.x, position_.y + currentEyeHeight_, position_.z};
}

void PlayerController::applyDamage(float amount, const Vec3& attackerPos) {
    if (!isAlive() || amount <= 0.0f) return;

    float armorAbsorb = std::min(armor_, amount * 0.55f);
    armor_ -= armorAbsorb;
    float hpLoss = amount - armorAbsorb;
    health_ = std::max(0.0f, health_ - hpLoss);
    damageFlash_ = 1.0f;

    Vec3 toAttacker = attackerPos - position_;
    float attackAngle = std::atan2(toAttacker.x, -toAttacker.z);
    damageDirAngle_ = attackAngle - yaw_;
}

void PlayerController::heal(float healthAmt, float armorAmt) {
    health_ = std::min(maxHealth_, health_ + healthAmt);
    armor_ = std::min(maxArmor_, armor_ + armorAmt);
}

void PlayerController::update(float dt,
                              const InputState& input,
                              float aimFactor,
                              const CollisionSystem& collision,
                              CameraSystem& camera,
                              AudioSystem& audio) {
    if (damageFlash_ > 0.0f) {
        damageFlash_ = std::max(0.0f, damageFlash_ - dt * 2.4f);
    }
    if (landDip_ > 0.0f) {
        landDip_ = std::max(0.0f, landDip_ - dt * 0.45f);
    }

    if (!isAlive()) {
        camera.setPosition(getEyePosition());
        camera.setAngles(yaw_, pitch_);
        camera.update(dt);
        return;
    }

    yaw_ += input.lookDelta.x + input.gyroDelta.x;
    pitch_ = clampf(pitch_ + input.lookDelta.y + input.gyroDelta.y, -1.42f, 1.42f);

    crouching_ = input.crouch;
    bool wantsSprint = input.sprint && !crouching_ && input.moveAxis.y > 0.35f && aimFactor < 0.2f;
    if (wantsSprint && stamina_ > 5.0f) {
        sprinting_ = true;
        stamina_ = std::max(0.0f, stamina_ - dt * 24.0f);
    } else {
        sprinting_ = false;
        stamina_ = std::min(100.0f, stamina_ + dt * 18.0f);
    }

    float targetEyeH = crouching_ ? 1.08f : 1.68f;
    currentEyeHeight_ = lerpf(currentEyeHeight_, targetEyeH, clampf(dt * 12.0f, 0.0f, 1.0f));
    float colliderHeight = crouching_ ? 1.22f : 1.82f;

    // Horizontal movement basis from yaw
    Vec3 forward{std::sin(yaw_), 0.0f, -std::cos(yaw_)};
    Vec3 right{std::cos(yaw_), 0.0f, std::sin(yaw_)};
    Vec3 wishDir = right * input.moveAxis.x + forward * input.moveAxis.y;
    float wishLen = wishDir.length();
    if (wishLen > 1.0f) wishDir *= (1.0f / wishLen);

    float moveSpeed = 5.4f;
    if (crouching_) moveSpeed = 3.0f;
    else if (sprinting_) moveSpeed = 8.0f;
    if (aimFactor > 0.25f) moveSpeed *= 0.65f;

    float accel = grounded_ ? 14.0f : 4.5f;
    velocity_.x = lerpf(velocity_.x, wishDir.x * moveSpeed, clampf(dt * accel, 0.0f, 1.0f));
    velocity_.z = lerpf(velocity_.z, wishDir.z * moveSpeed, clampf(dt * accel, 0.0f, 1.0f));

    if (input.jumpPressed && grounded_ && !crouching_) {
        velocity_.y = 6.6f;
        grounded_ = false;
    }

    float preFallVy = velocity_.y;
    bool wasGrounded = grounded_;
    velocity_.y -= 18.5f * dt;
    position_ += velocity_ * dt;

    bool nowGrounded = false;
    collision.resolveCharacterMove(position_, velocity_, 0.36f, colliderHeight, nowGrounded);
    if (!wasGrounded && nowGrounded && preFallVy < -3.0f) {
        landDip_ = clampf(-preFallVy * 0.014f, 0.025f, 0.11f);
        audio.playSound(SoundId::Footstep, 0.65f);
    }
    grounded_ = nowGrounded;

    float horizSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    speedRatio_ = clampf(horizSpeed / 8.0f, 0.0f, 1.0f);

    Vec3 camPos = getEyePosition();
    camPos.y -= landDip_;
    if (grounded_ && horizSpeed > 0.6f) {
        float cadence = sprinting_ ? 12.5f : (crouching_ ? 6.5f : 9.2f);
        bobPhase_ += dt * cadence;
        float bobAmp = (sprinting_ ? 0.045f : 0.025f) * (1.0f - aimFactor * 0.7f);
        camPos.y += std::sin(bobPhase_) * bobAmp;

        stepTimer_ -= dt * (sprinting_ ? 1.5f : 1.0f);
        if (stepTimer_ <= 0.0f) {
            audio.playSound(SoundId::Footstep, crouching_ ? 0.25f : 0.5f);
            stepTimer_ = 0.44f;
        }
    } else {
        stepTimer_ = 0.1f;
    }

    camera.setPosition(camPos);
    camera.setAngles(yaw_, pitch_);
    camera.setAimFactor(aimFactor);
    camera.setSprintFactor(sprinting_ ? speedRatio_ : 0.0f);
    camera.update(dt);
}

} // namespace ts
