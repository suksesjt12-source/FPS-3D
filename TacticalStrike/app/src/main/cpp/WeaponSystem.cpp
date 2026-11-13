#include "WeaponSystem.h"
#include "EnemySystem.h"
#include <algorithm>
#include <cmath>

namespace ts {

void WeaponSystem::resetLoadout() {
    weapons_[0].spec = {WeaponId::AssaultRifle, "M4A1-TAC", 28.0f, 0.095f, 30, 180, 1.75f, 0.024f, 0.009f, 0.022f, 0.0028f, 130.0f, true, SoundId::RifleShot};
    weapons_[0].magAmmo = 30;
    weapons_[0].reserveAmmo = 150;

    weapons_[1].spec = {WeaponId::Smg, "MP7-SPEC", 20.0f, 0.068f, 40, 200, 1.45f, 0.017f, 0.011f, 0.028f, 0.0050f, 90.0f, true, SoundId::SmgShot};
    weapons_[1].magAmmo = 40;
    weapons_[1].reserveAmmo = 160;

    weapons_[2].spec = {WeaponId::Sidearm, "USP-45", 36.0f, 0.22f, 12, 72, 1.25f, 0.032f, 0.006f, 0.016f, 0.0022f, 80.0f, false, SoundId::PistolShot};
    weapons_[2].magAmmo = 12;
    weapons_[2].reserveAmmo = 48;

    activeSlot_ = 0;
    grenades_ = 2;
    fireCooldown_ = 0.0f;
    reloading_ = false;
    reloadTimer_ = 0.0f;
    aimFactor_ = 0.0f;
    recoilKickZ_ = 0.0f;
    recoilRotX_ = 0.0f;
    switchAnim_ = 0.0f;
    hitmarkerTimer_ = 0.0f;
    lastHitKill_ = false;
    shotsFired_ = 0;
    shotsHit_ = 0;
}

void WeaponSystem::addAmmo(int rifleReserve, int smgReserve, int pistolReserve, int grenades) {
    weapons_[0].reserveAmmo = std::min(weapons_[0].spec.maxReserve, weapons_[0].reserveAmmo + rifleReserve);
    weapons_[1].reserveAmmo = std::min(weapons_[1].spec.maxReserve, weapons_[1].reserveAmmo + smgReserve);
    weapons_[2].reserveAmmo = std::min(weapons_[2].spec.maxReserve, weapons_[2].reserveAmmo + pistolReserve);
    grenades_ = std::min(4, grenades_ + grenades);
}

float WeaponSystem::randSym(float mag) {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    float t = (static_cast<float>(rng_ & 0xFFFFu) / 32767.5f) - 1.0f;
    return t * mag;
}

float WeaponSystem::getReloadProgress() const {
    if (!reloading_) return 0.0f;
    float total = weapons_[activeSlot_].spec.reloadTime;
    return total > 0.01f ? clampf(1.0f - (reloadTimer_ / total), 0.0f, 1.0f) : 1.0f;
}

void WeaponSystem::update(float dt,
                          const InputState& input,
                          float playerSpeedRatio,
                          CameraSystem& camera,
                          const CollisionSystem& collision,
                          EnemySystem& enemies,
                          ParticleSystem& particles,
                          LightingSystem& lighting,
                          PhysicsSystem& physics,
                          AudioSystem& audio) {
    if (fireCooldown_ > 0.0f) fireCooldown_ = std::max(0.0f, fireCooldown_ - dt);
    if (hitmarkerTimer_ > 0.0f) hitmarkerTimer_ = std::max(0.0f, hitmarkerTimer_ - dt);
    if (switchAnim_ > 0.0f) switchAnim_ = std::max(0.0f, switchAnim_ - dt * 4.0f);

    // Weapon switch
    if (input.weaponSwitchReq >= 0) {
        int nextSlot = (input.weaponSwitchReq == 99)
            ? ((activeSlot_ + 1) % 3)
            : std::clamp(input.weaponSwitchReq, 0, 2);
        if (nextSlot != activeSlot_) {
            activeSlot_ = nextSlot;
            reloading_ = false;
            reloadTimer_ = 0.0f;
            switchAnim_ = 1.0f;
            audio.playSound(SoundId::Reload, 0.4f);
        }
    }

    auto& w = weapons_[activeSlot_];

    // Reload start / completion
    if (input.reloadPressed && !reloading_ && w.magAmmo < w.spec.magCapacity && w.reserveAmmo > 0) {
        reloading_ = true;
        reloadTimer_ = w.spec.reloadTime;
        audio.playSound(SoundId::Reload, 0.8f);
    }

    if (reloading_) {
        reloadTimer_ -= dt;
        if (reloadTimer_ <= 0.0f) {
            int needed = w.spec.magCapacity - w.magAmmo;
            int take = std::min(needed, w.reserveAmmo);
            w.magAmmo += take;
            w.reserveAmmo -= take;
            reloading_ = false;
        }
    }

    // ADS interpolation
    float targetAim = (input.aim && !reloading_) ? 1.0f : 0.0f;
    aimFactor_ = lerpf(aimFactor_, targetAim, clampf(dt * 12.0f, 0.0f, 1.0f));

    // Grenade throw
    if (input.grenadePressed && grenades_ > 0) {
        --grenades_;
        Vec3 dir = (camera.getForward() + Vec3{0.0f, 0.22f, 0.0f}).normalized();
        Vec3 spawnPos = camera.getPosition() + camera.getForward() * 0.55f + camera.getRight() * 0.15f;
        physics.spawnGrenade(spawnPos, dir * 16.5f, 2.4f, true);
    }

    // Firing
    bool wantsFire = w.spec.automatic ? input.fire : (input.fire && !triggerHeldLastFrame_);
    triggerHeldLastFrame_ = input.fire;

    if (wantsFire && !reloading_ && switchAnim_ < 0.2f && fireCooldown_ <= 0.0f) {
        if (w.magAmmo > 0) {
            fireShot(camera, collision, enemies, particles, lighting, audio);
            --w.magAmmo;
            fireCooldown_ = w.spec.fireInterval;
            if (w.magAmmo == 0 && w.reserveAmmo > 0) {
                reloading_ = true;
                reloadTimer_ = w.spec.reloadTime;
                audio.playSound(SoundId::Reload, 0.8f);
            }
        } else if (w.reserveAmmo > 0) {
            reloading_ = true;
            reloadTimer_ = w.spec.reloadTime;
            audio.playSound(SoundId::Reload, 0.8f);
        }
    }

    // Viewmodel recoil recovery & sway
    float decay = std::exp(-14.0f * dt);
    recoilKickZ_ *= decay;
    recoilRotX_ *= decay;
    swayPhase_ += dt * (2.2f + playerSpeedRatio * 7.5f);
}

void WeaponSystem::fireShot(CameraSystem& camera,
                            const CollisionSystem& collision,
                            EnemySystem& enemies,
                            ParticleSystem& particles,
                            LightingSystem& lighting,
                            AudioSystem& audio) {
    const auto& spec = weapons_[activeSlot_].spec;
    ++shotsFired_;

    float spread = lerpf(spec.hipSpread, spec.adsSpread, aimFactor_);
    Vec3 fwd = camera.getForward();
    Vec3 right = camera.getRight();
    Vec3 up = camera.getUp();
    Vec3 shotDir = (fwd + right * randSym(spread) + up * randSym(spread)).normalized();

    Vec3 muzzleWorld = camera.getPosition() + fwd * 0.68f + right * lerpf(0.22f, 0.0f, aimFactor_) - up * 0.15f;
    particles.spawnMuzzleFlash(muzzleWorld, fwd);
    lighting.spawnTransientLight(muzzleWorld, {1.0f, 0.72f, 0.30f}, 4.5f, 7.0f, 0.06f);
    audio.playSound(spec.shotSound, 0.95f);

    camera.addRecoilOffset(spec.recoilPitch * lerpf(1.0f, 0.6f, aimFactor_), randSym(spec.recoilYaw));
    recoilKickZ_ = std::min(0.08f, recoilKickZ_ + 0.045f);
    recoilRotX_ = std::min(0.18f, recoilRotX_ + 0.09f);

    Ray ray{camera.getPosition(), shotDir};
    RayHit worldHit = collision.raycastWorld(ray, spec.range);

    float maxEnemyDist = worldHit.hit ? worldHit.distance : spec.range;
    EnemyHitResult enemyHit = enemies.raycastHit(ray, maxEnemyDist);

    if (enemyHit.hit) {
        ++shotsHit_;
        float dmg = spec.damage * (enemyHit.headshot ? 2.1f : 1.0f);
        bool killed = enemies.damageEnemy(enemyHit.enemyIndex, dmg, camera.getPosition());
        particles.spawnBloodBurst(enemyHit.point, enemyHit.normal, enemyHit.headshot ? 14 : 9);
        hitmarkerTimer_ = 0.16f;
        lastHitKill_ = killed;
        audio.playSound(SoundId::Hitmarker, killed ? 1.0f : 0.7f);
    } else if (worldHit.hit) {
        particles.spawnImpactSparks(worldHit.point, worldHit.normal, 8);
    }
}

Mat4 WeaponSystem::getViewmodelMatrix(const CameraSystem& camera, float playerSpeedRatio) const {
    Vec3 camPos = camera.getPosition();
    float totalPitch = clampf(camera.getPitch(), -1.45f, 1.45f);
    float totalYaw = camera.getYaw();

    Mat4 camBasis = Mat4::translate(camPos) *
                    Mat4::rotateY(-totalYaw) *
                    Mat4::rotateX(totalPitch);

    float swayX = std::cos(swayPhase_) * 0.008f * (0.25f + playerSpeedRatio) * (1.0f - aimFactor_ * 0.7f);
    float swayY = std::abs(std::sin(swayPhase_)) * -0.010f * (0.25f + playerSpeedRatio) * (1.0f - aimFactor_ * 0.7f);

    float reloadDip = 0.0f;
    float reloadTilt = 0.0f;
    if (reloading_) {
        float p = getReloadProgress();
        float curve = std::sin(p * PI);
        reloadDip = -0.14f * curve;
        reloadTilt = 0.45f * curve;
    }

    float switchDip = -0.24f * switchAnim_;

    Vec3 hipOffset{0.21f, -0.20f, -0.44f};
    Vec3 adsOffset{0.0f, -0.135f, -0.35f};
    Vec3 localPos = lerp(hipOffset, adsOffset, aimFactor_);
    localPos.x += swayX;
    localPos.y += swayY + reloadDip + switchDip;
    localPos.z += recoilKickZ_;

    Mat4 local = Mat4::translate(localPos) *
                 Mat4::rotateX(recoilRotX_ - reloadTilt) *
                 Mat4::rotateZ(reloadTilt * 0.35f);
    return camBasis * local;
}

} // namespace ts
