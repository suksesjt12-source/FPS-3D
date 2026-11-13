#pragma once

#include "AnimationSystem.h"
#include "AudioSystem.h"
#include "CollisionSystem.h"
#include "EnemySystem.h"
#include "LightingSystem.h"
#include "ModelLoader.h"
#include "ParticleSystem.h"
#include "PlayerController.h"

namespace ts {

class EnemyAI {
public:
    void update(float dt,
                EnemySystem& enemies,
                PlayerController& player,
                const CollisionSystem& collision,
                const AnimationSystem& animSystem,
                const ModelAsset* enemyModel,
                ParticleSystem& particles,
                LightingSystem& lighting,
                AudioSystem& audio);

private:
    uint32_t rng_ = 0xCAFEBABEu;
    float rand01();
    Vec3 pickCoverPoint(const Vec3& enemyPos, const Vec3& playerPos, const CollisionSystem& collision);
};

} // namespace ts
