#include "CameraSystem.h"
#include <cmath>

namespace ts {

void CameraSystem::setViewport(int width, int height) {
    if (height > 0) {
        aspect_ = static_cast<float>(width) / static_cast<float>(height);
    }
}

void CameraSystem::setAngles(float yawRad, float pitchRad) {
    yaw_ = yawRad;
    pitch_ = clampf(pitchRad, -1.45f, 1.45f);
}

void CameraSystem::addRecoilOffset(float pitchKick, float yawKick) {
    recoilPitch_ = clampf(recoilPitch_ + pitchKick, -0.35f, 0.35f);
    recoilYaw_ = clampf(recoilYaw_ + yawKick, -0.25f, 0.25f);
}

void CameraSystem::addShake(float intensity) {
    shakeIntensity_ = std::max(shakeIntensity_, intensity);
    shakeTimer_ = 0.28f;
}

void CameraSystem::update(float dt) {
    float targetFov = lerpf(baseFovDeg_, adsFovDeg_, adsFactor_);
    currentFovDeg_ = lerpf(currentFovDeg_, targetFov, clampf(dt * 14.0f, 0.0f, 1.0f));

    // Smoothly recover recoil offset
    float decay = std::exp(-10.0f * dt);
    recoilPitch_ *= decay;
    recoilYaw_ *= decay;

    Vec3 shakeOffset{0, 0, 0};
    if (shakeTimer_ > 0.0f) {
        shakeTimer_ = std::max(0.0f, shakeTimer_ - dt);
        float mag = shakeIntensity_ * (shakeTimer_ / 0.28f);
        shakeOffset.x = std::sin(shakeTimer_ * 95.0f) * mag;
        shakeOffset.y = std::cos(shakeTimer_ * 120.0f) * mag * 0.8f;
        if (shakeTimer_ <= 0.0f) shakeIntensity_ = 0.0f;
    }

    effectivePos_ = position_ + shakeOffset;
    Vec3 fwd = getForward();
    view_ = Mat4::lookAt(effectivePos_, effectivePos_ + fwd, {0.0f, 1.0f, 0.0f});
    proj_ = Mat4::perspective(currentFovDeg_ * DEG2RAD, aspect_, nearZ_, farZ_);
    viewProj_ = proj_ * view_;
    frustum_.extractFromVP(viewProj_);
}

Vec3 CameraSystem::getForward() const {
    float totalPitch = clampf(pitch_ + recoilPitch_, -1.48f, 1.48f);
    float totalYaw = yaw_ + recoilYaw_;
    float cp = std::cos(totalPitch);
    return Vec3{
        std::sin(totalYaw) * cp,
        std::sin(totalPitch),
        -std::cos(totalYaw) * cp
    }.normalized();
}

Vec3 CameraSystem::getRight() const {
    return getForward().cross({0.0f, 1.0f, 0.0f}).normalized();
}

Vec3 CameraSystem::getUp() const {
    return getRight().cross(getForward()).normalized();
}

} // namespace ts
