#include "EnemySystem.h"
#include "CollisionSystem.h"
#include <algorithm>
#include <cmath>

namespace ts {

void EnemySystem::clear() {
    for (auto& e : pool_) {
        e = EnemyEntity{};
    }
    killCount_ = 0;
}

int EnemySystem::spawnEnemy(EnemyType type, const Vec3& pos, float yawRad, const std::vector<Vec3>& patrol) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!pool_[i].active) {
            auto& e = pool_[i];
            e = EnemyEntity{};
            e.active = true;
            e.id = nextId_++;
            e.type = type;
            e.state = EnemyAIState::Patrol;
            e.position = pos;
            e.yaw = yawRad;
            e.patrolPoints = patrol;
            if (e.patrolPoints.empty()) e.patrolPoints.push_back(pos);

            switch (type) {
                case EnemyType::Scout:
                    e.maxHealth = 75.0f; e.armor = 0.0f; e.ammoInMag = 20; break;
                case EnemyType::Enforcer:
                    e.maxHealth = 180.0f; e.armor = 60.0f; e.ammoInMag = 45; break;
                case EnemyType::Rifleman:
                default:
                    e.maxHealth = 100.0f; e.armor = 20.0f; e.ammoInMag = 30; break;
            }
            e.health = e.maxHealth;
            e.fireCooldown = 0.85f;
            return i;
        }
    }
    return -1;
}

EnemyHitResult EnemySystem::raycastHit(const Ray& ray, float maxDist) const {
    EnemyHitResult best;
    best.distance = maxDist;

    for (int i = 0; i < MAX_ENEMIES; ++i) {
        const auto& e = pool_[i];
        if (!e.active || e.state == EnemyAIState::Dead) continue;

        float t = 0.0f;
        Vec3 pt, norm;
        if (CollisionSystem::intersectRayCapsuleY(ray, e.position, 0.42f, 1.82f, best.distance, t, pt, norm)) {
            best.hit = true;
            best.enemyIndex = i;
            best.distance = t;
            best.point = pt;
            best.normal = norm;
            best.headshot = (pt.y - e.position.y) >= 1.45f;
        }
    }
    return best;
}

bool EnemySystem::damageEnemy(int index, float amount, const Vec3& attackerPos) {
    if (index < 0 || index >= MAX_ENEMIES) return false;
    auto& e = pool_[index];
    if (!e.active || e.state == EnemyAIState::Dead) return false;

    float absorbed = std::min(e.armor, amount * 0.4f);
    e.armor -= absorbed;
    e.health -= (amount - absorbed);
    e.hitFlash = 0.18f;

    // Alert enemy toward attacker
    if (e.state == EnemyAIState::Patrol) {
        e.state = EnemyAIState::Alert;
        Vec3 d = attackerPos - e.position;
        e.yaw = std::atan2(d.x, -d.z);
        e.stateTimer = 0.35f;
    }

    if (e.health <= 0.0f) {
        e.health = 0.0f;
        e.state = EnemyAIState::Dead;
        e.deathTimer = 0.0f;
        e.animTime = 0.0f;
        ++killCount_;
        return true;
    }
    return false;
}

void EnemySystem::applyExplosionDamage(const Vec3& center, float radius, float maxDamage) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        auto& e = pool_[i];
        if (!e.active || e.state == EnemyAIState::Dead) continue;
        Vec3 bodyCenter = e.position + Vec3{0, 0.9f, 0};
        float dist = (bodyCenter - center).length();
        if (dist <= radius) {
            float falloff = 1.0f - (dist / radius);
            damageEnemy(i, maxDamage * falloff, center);
        }
    }
}

int EnemySystem::getAliveCount() const {
    int count = 0;
    for (const auto& e : pool_) {
        if (e.active && e.state != EnemyAIState::Dead) ++count;
    }
    return count;
}

} // namespace ts
