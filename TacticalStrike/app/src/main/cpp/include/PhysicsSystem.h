#pragma once

#include "CollisionSystem.h"
#include "Math3D.h"
#include <array>
#include <functional>

namespace ts {

constexpr int MAX_GRENADES = 8;

struct GrenadeProjectile {
    Vec3 pos{0, 0, 0};
    Vec3 vel{0, 0, 0};
    float fuseRemaining = 0.0f;
    float radius = 0.12f;
    bool fromPlayer = true;
    bool active = false;
};

class PhysicsSystem {
public:
    using ExplosionCallback = std::function<void(const Vec3& pos, float radius, float maxDamage, bool fromPlayer)>;

    void clear();
    void setExplosionCallback(ExplosionCallback cb) { onExplosion_ = std::move(cb); }
    void spawnGrenade(const Vec3& origin, const Vec3& velocity, float fuseSec, bool fromPlayer);
    void update(float dt, const CollisionSystem& collision);

    const std::array<GrenadeProjectile, MAX_GRENADES>& getGrenades() const { return grenades_; }

private:
    std::array<GrenadeProjectile, MAX_GRENADES> grenades_{};
    ExplosionCallback onExplosion_;
};

} // namespace ts
