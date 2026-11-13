#pragma once

#include "AudioSystem.h"
#include "CameraSystem.h"
#include "CollisionSystem.h"
#include "InputManager.h"
#include "Math3D.h"

namespace ts {

class PlayerController {
public:
    void spawnAt(const Vec3& feetPos, float yawRad);
    void update(float dt,
                const InputState& input,
                float aimFactor,
                const CollisionSystem& collision,
                CameraSystem& camera,
                AudioSystem& audio);

    void applyDamage(float amount, const Vec3& attackerPos);
    void heal(float healthAmt, float armorAmt);

    const Vec3& getPosition() const { return position_; }
    Vec3 getEyePosition() const;
    float getYaw() const { return yaw_; }
    float getPitch() const { return pitch_; }
    float getHealth() const { return health_; }
    float getMaxHealth() const { return maxHealth_; }
    float getArmor() const { return armor_; }
    float getMaxArmor() const { return maxArmor_; }
    float getStamina() const { return stamina_; }
    bool isGrounded() const { return grounded_; }
    bool isSprinting() const { return sprinting_; }
    bool isCrouching() const { return crouching_; }
    bool isAlive() const { return health_ > 0.0f; }
    float getSpeedRatio() const { return speedRatio_; }
    float getLandDip() const { return landDip_; }
    float getDamageFlash() const { return damageFlash_; }
    float getDamageDirAngle() const { return damageDirAngle_; }

private:
    Vec3 position_{0, 0, 0};
    Vec3 velocity_{0, 0, 0};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    float health_ = 100.0f;
    float maxHealth_ = 100.0f;
    float armor_ = 50.0f;
    float maxArmor_ = 100.0f;
    float stamina_ = 100.0f;

    float currentEyeHeight_ = 1.68f;
    float stepTimer_ = 0.0f;
    float bobPhase_ = 0.0f;
    float speedRatio_ = 0.0f;
    float landDip_ = 0.0f;
    float damageFlash_ = 0.0f;
    float damageDirAngle_ = 0.0f;

    bool grounded_ = false;
    bool sprinting_ = false;
    bool crouching_ = false;
};

} // namespace ts
