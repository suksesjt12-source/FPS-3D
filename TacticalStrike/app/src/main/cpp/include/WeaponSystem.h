#pragma once

#include "AudioSystem.h"
#include "CameraSystem.h"
#include "CollisionSystem.h"
#include "InputManager.h"
#include "LightingSystem.h"
#include "Math3D.h"
#include "ParticleSystem.h"
#include "PhysicsSystem.h"
#include <array>
#include <string>

namespace ts {

class EnemySystem;

enum class WeaponId {
    AssaultRifle = 0,
    Smg = 1,
    Sidearm = 2
};

struct WeaponSpec {
    WeaponId id;
    std::string name;
    float damage = 28.0f;
    float fireInterval = 0.10f;
    int magCapacity = 30;
    int maxReserve = 180;
    float reloadTime = 1.6f;
    float recoilPitch = 0.022f;
    float recoilYaw = 0.008f;
    float hipSpread = 0.022f;
    float adsSpread = 0.003f;
    float range = 120.0f;
    bool automatic = true;
    SoundId shotSound = SoundId::RifleShot;
};

struct WeaponRuntime {
    WeaponSpec spec;
    int magAmmo = 30;
    int reserveAmmo = 150;
};

class WeaponSystem {
public:
    void resetLoadout();
    void addAmmo(int rifleReserve, int smgReserve, int pistolReserve, int grenades);

    void update(float dt,
                const InputState& input,
                float playerSpeedRatio,
                CameraSystem& camera,
                const CollisionSystem& collision,
                EnemySystem& enemies,
                ParticleSystem& particles,
                LightingSystem& lighting,
                PhysicsSystem& physics,
                AudioSystem& audio);

    WeaponId getActiveWeaponId() const { return static_cast<WeaponId>(activeSlot_); }
    int getActiveSlot() const { return activeSlot_; }
    const WeaponRuntime& getActiveWeapon() const { return weapons_[activeSlot_]; }
    int getGrenadeCount() const { return grenades_; }
    bool isReloading() const { return reloading_; }
    float getReloadProgress() const;
    float getAimFactor() const { return aimFactor_; }
    float getHitmarkerTimer() const { return hitmarkerTimer_; }
    bool wasLastHitKill() const { return lastHitKill_; }
    int getShotsFired() const { return shotsFired_; }
    int getShotsHit() const { return shotsHit_; }

    Mat4 getViewmodelMatrix(const CameraSystem& camera, float playerSpeedRatio) const;

private:
    void fireShot(CameraSystem& camera,
                  const CollisionSystem& collision,
                  EnemySystem& enemies,
                  ParticleSystem& particles,
                  LightingSystem& lighting,
                  AudioSystem& audio);

    std::array<WeaponRuntime, 3> weapons_;
    int activeSlot_ = 0;
    int grenades_ = 2;

    float fireCooldown_ = 0.0f;
    bool reloading_ = false;
    float reloadTimer_ = 0.0f;
    float aimFactor_ = 0.0f;
    float recoilKickZ_ = 0.0f;
    float recoilRotX_ = 0.0f;
    float swayPhase_ = 0.0f;
    float switchAnim_ = 0.0f;
    float hitmarkerTimer_ = 0.0f;
    bool lastHitKill_ = false;
    bool triggerHeldLastFrame_ = false;

    int shotsFired_ = 0;
    int shotsHit_ = 0;
    uint32_t rng_ = 0x13579BDFu;

    float randSym(float mag);
};

} // namespace ts
