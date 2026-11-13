#pragma once

#include "Math3D.h"

namespace ts {

class CameraSystem {
public:
    void setViewport(int width, int height);
    void setPosition(const Vec3& pos) { position_ = pos; }
    void setAngles(float yawRad, float pitchRad);
    void addRecoilOffset(float pitchKick, float yawKick);
    void setAimFactor(float adsFactor) { adsFactor_ = clampf(adsFactor, 0.0f, 1.0f); }
    void setSprintFactor(float sprintFactor) { sprintFactor_ = clampf(sprintFactor, 0.0f, 1.0f); }
    void addShake(float intensity);
    void update(float dt);

    const Vec3& getPosition() const { return effectivePos_; }
    Vec3 getForward() const;
    Vec3 getRight() const;
    Vec3 getUp() const;
    float getYaw() const { return yaw_; }
    float getPitch() const { return pitch_; }
    float getFovDeg() const { return currentFovDeg_; }
    float getAspect() const { return aspect_; }

    const Mat4& getViewMatrix() const { return view_; }
    const Mat4& getProjectionMatrix() const { return proj_; }
    const Mat4& getViewProjMatrix() const { return viewProj_; }
    const Frustum& getFrustum() const { return frustum_; }

    bool isBoxVisible(const AABB& box) const {
        return frustum_.intersectsAABB(box);
    }

private:
    Vec3 position_{0.0f, 1.7f, 0.0f};
    Vec3 effectivePos_{0.0f, 1.7f, 0.0f};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float recoilPitch_ = 0.0f;
    float recoilYaw_ = 0.0f;
    float shakeTimer_ = 0.0f;
    float shakeIntensity_ = 0.0f;

    float baseFovDeg_ = 72.0f;
    float adsFovDeg_ = 46.0f;
    float currentFovDeg_ = 72.0f;
    float adsFactor_ = 0.0f;
    float sprintFactor_ = 0.0f;
    float aspect_ = 16.0f / 9.0f;
    float nearZ_ = 0.08f;
    float farZ_ = 180.0f;

    Mat4 view_;
    Mat4 proj_;
    Mat4 viewProj_;
    Frustum frustum_;
};

} // namespace ts
