#pragma once

#include "AnimationSystem.h"
#include "Math3D.h"
#include <array>
#include <string>
#include <vector>

namespace ts {

constexpr int MAX_ENEMIES = 24;

enum class EnemyType {
    Rifleman = 0,
    Scout = 1,
    Enforcer = 2
};

enum class EnemyAIState {
    Patrol = 0,
    Alert,
    Engage,
    TakeCover,
    Flank,
    Reload,
    Dead
};

struct EnemyEntity {
    bool active = false;
    int id = 0;
    EnemyType type = EnemyType::Rifleman;
    EnemyAIState state = EnemyAIState::Patrol;

    Vec3 position{0, 0, 0};
    Vec3 velocity{0, 0, 0};
    float yaw = 0.0f;
    float health = 100.0f;
    float maxHealth = 100.0f;
    float armor = 0.0f;

    std::vector<Vec3> patrolPoints;
    int patrolIndex = 0;
    Vec3 coverTarget{0, 0, 0};
    bool hasCoverTarget = false;

    float stateTimer = 0.0f;
    float fireCooldown = 0.0f;
    int burstRemaining = 0;
    int ammoInMag = 24;
    float animTime = 0.0f;
    float prevAnimTime = 0.0f;
    float animBlend = 1.0f;
    std::string currentClip = "idle";
    std::string prevClip = "idle";
    float hitFlash = 0.0f;
    float hitStagger = 0.0f;
    float deathTimer = 0.0f;
    bool alertTriggered = false;

    SkeletalPose pose;
};

struct EnemyHitResult {
    bool hit = false;
    int enemyIndex = -1;
    float distance = 1e9f;
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 1, 0};
    bool headshot = false;
};

class EnemySystem {
public:
    void clear();
    int spawnEnemy(EnemyType type, const Vec3& pos, float yawRad, const std::vector<Vec3>& patrol);

    EnemyHitResult raycastHit(const Ray& ray, float maxDist) const;
    bool damageEnemy(int index, float amount, const Vec3& attackerPos);
    void applyExplosionDamage(const Vec3& center, float radius, float maxDamage);

    int getAliveCount() const;
    int getKillCount() const { return killCount_; }
    void resetKillCount() { killCount_ = 0; }

    std::array<EnemyEntity, MAX_ENEMIES>& getPool() { return pool_; }
    const std::array<EnemyEntity, MAX_ENEMIES>& getPool() const { return pool_; }

private:
    std::array<EnemyEntity, MAX_ENEMIES> pool_{};
    int nextId_ = 1;
    int killCount_ = 0;
};

} // namespace ts
