#include "EnemyAI.h"
#include <algorithm>
#include <cmath>

namespace ts {

float EnemyAI::rand01() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ & 0xFFFFu) / 65535.0f;
}

Vec3 EnemyAI::pickCoverPoint(const Vec3& enemyPos, const Vec3& playerPos, const CollisionSystem& collision) {
    Vec3 away = (enemyPos - playerPos);
    away.y = 0.0f;
    if (away.lengthSq() < 1e-4f) away = {0, 0, 1};
    away = away.normalized();
    Vec3 side{-away.z, 0.0f, away.x};
    if (rand01() > 0.5f) side = -side;

    Vec3 candidate = enemyPos + (side * 3.5f + away * 2.0f);
    // Keep candidate valid if unobstructed
    if (collision.hasLineOfSight(enemyPos + Vec3{0, 1.0f, 0}, candidate + Vec3{0, 1.0f, 0})) {
        return candidate;
    }
    return enemyPos + side * 2.5f;
}

void EnemyAI::update(float dt,
                     EnemySystem& enemies,
                     PlayerController& player,
                     const CollisionSystem& collision,
                     const AnimationSystem& animSystem,
                     const ModelAsset* enemyModel,
                     ParticleSystem& particles,
                     LightingSystem& lighting,
                     AudioSystem& audio) {
    Vec3 playerEye = player.getEyePosition();
    auto& pool = enemies.getPool();

    for (auto& e : pool) {
        if (!e.active) continue;

        if (e.hitFlash > 0.0f) e.hitFlash = std::max(0.0f, e.hitFlash - dt);
        e.animTime += dt;

        if (e.state == EnemyAIState::Dead) {
            e.deathTimer += dt;
            if (enemyModel) {
                animSystem.evaluateClip(*enemyModel, "death", e.deathTimer, false, e.pose);
            }
            if (e.deathTimer > 6.0f) {
                e.active = false;
            }
            continue;
        }

        Vec3 enemyEye = e.position + Vec3{0.0f, 1.55f, 0.0f};
        Vec3 toPlayer = playerEye - enemyEye;
        float distToPlayer = toPlayer.length();
        Vec3 toPlayerFlat{toPlayer.x, 0.0f, toPlayer.z};
        float flatDist = toPlayerFlat.length();

        bool canSeePlayer = false;
        if (player.isAlive() && distToPlayer < 42.0f) {
            Vec3 enemyFwd{std::sin(e.yaw), 0.0f, -std::cos(e.yaw)};
            float fovDot = flatDist > 0.01f ? enemyFwd.dot(toPlayerFlat * (1.0f / flatDist)) : 1.0f;
            if (fovDot > 0.15f || distToPlayer < 8.5f || e.state != EnemyAIState::Patrol) {
                canSeePlayer = collision.hasLineOfSight(enemyEye, playerEye);
            }
        }

        float moveSpeed = (e.type == EnemyType::Scout) ? 4.8f : ((e.type == EnemyType::Enforcer) ? 2.8f : 3.6f);
        Vec3 moveDir{0, 0, 0};
        std::string clip = "idle";

        switch (e.state) {
            case EnemyAIState::Patrol: {
                if (canSeePlayer) {
                    e.state = EnemyAIState::Alert;
                    e.stateTimer = 0.70f;
                    break;
                }
                if (!e.patrolPoints.empty()) {
                    Vec3 target = e.patrolPoints[e.patrolIndex % e.patrolPoints.size()];
                    Vec3 diff = target - e.position;
                    diff.y = 0.0f;
                    if (diff.length() < 0.7f) {
                        e.patrolIndex = (e.patrolIndex + 1) % static_cast<int>(e.patrolPoints.size());
                    } else {
                        moveDir = diff.normalized();
                        moveSpeed *= 0.55f;
                        e.yaw = std::atan2(moveDir.x, -moveDir.z);
                        clip = "walk";
                    }
                }
                break;
            }

            case EnemyAIState::Alert: {
                float targetYaw = std::atan2(toPlayer.x, -toPlayer.z);
                e.yaw = targetYaw;
                e.stateTimer -= dt;
                if (e.stateTimer <= 0.0f) {
                    e.state = EnemyAIState::Engage;
                    e.burstRemaining = 3 + static_cast<int>(rand01() * 3.0f);
                    e.fireCooldown = std::max(e.fireCooldown, 0.35f + rand01() * 0.35f);
                }
                break;
            }

            case EnemyAIState::Engage: {
                float targetYaw = std::atan2(toPlayer.x, -toPlayer.z);
                e.yaw = targetYaw;

                if (!canSeePlayer) {
                    e.state = EnemyAIState::Flank;
                    e.stateTimer = 3.0f;
                    break;
                }

                // Maintain optimal engagement range
                if (flatDist > 16.0f) {
                    moveDir = toPlayerFlat.normalized();
                    clip = "run";
                } else if (flatDist < 5.5f) {
                    moveDir = -toPlayerFlat.normalized() * 0.65f;
                    clip = "walk";
                } else {
                    // Tactical strafe
                    Vec3 strafe{-toPlayerFlat.z, 0.0f, toPlayerFlat.x};
                    strafe = strafe.normalized();
                    if (std::fmod(e.animTime, 3.0f) > 1.5f) strafe = -strafe;
                    moveDir = strafe * 0.55f;
                    clip = "walk";
                }

                e.fireCooldown -= dt;
                if (e.fireCooldown <= 0.0f && player.isAlive()) {
                    if (e.ammoInMag <= 0) {
                        e.state = EnemyAIState::Reload;
                        e.stateTimer = 1.8f;
                        break;
                    }
                    --e.ammoInMag;
                    clip = "shoot";

                    Vec3 enemyFwd{std::sin(e.yaw), 0.0f, -std::cos(e.yaw)};
                    Vec3 muzzlePos = enemyEye + enemyFwd * 0.55f - Vec3{0, 0.12f, 0};
                    particles.spawnMuzzleFlash(muzzlePos, toPlayer.normalized());
                    lighting.spawnTransientLight(muzzlePos, {1.0f, 0.65f, 0.25f}, 3.0f, 5.0f, 0.05f);
                    audio.playSound(SoundId::RifleShot, clampf(1.0f - distToPlayer / 45.0f, 0.1f, 0.65f));

                    // Accuracy scales with distance & enemy type
                    float hitProb = clampf(0.46f - distToPlayer * 0.014f, 0.08f, 0.48f);
                    if (rand01() < hitProb) {
                        float dmg = (e.type == EnemyType::Enforcer) ? 14.0f : 9.0f;
                        player.applyDamage(dmg, e.position);
                        audio.playSound(SoundId::DamageHit, 0.7f);
                    }

                    --e.burstRemaining;
                    if (e.burstRemaining <= 0) {
                        e.fireCooldown = 0.75f + rand01() * 0.65f;
                        e.burstRemaining = 3 + static_cast<int>(rand01() * 3.0f);
                        if (e.health < e.maxHealth * 0.45f && rand01() < 0.5f) {
                            e.coverTarget = pickCoverPoint(e.position, player.getPosition(), collision);
                            e.state = EnemyAIState::TakeCover;
                            e.stateTimer = 2.0f;
                        }
                    } else {
                        e.fireCooldown = (e.type == EnemyType::Enforcer) ? 0.13f : 0.17f;
                    }
                }
                break;
            }

            case EnemyAIState::TakeCover: {
                Vec3 diff = e.coverTarget - e.position;
                diff.y = 0.0f;
                e.stateTimer -= dt;
                if (diff.length() > 0.6f && e.stateTimer > 0.0f) {
                    moveDir = diff.normalized();
                    e.yaw = std::atan2(moveDir.x, -moveDir.z);
                    clip = "run";
                } else {
                    e.state = EnemyAIState::Engage;
                }
                break;
            }

            case EnemyAIState::Flank: {
                e.stateTimer -= dt;
                if (canSeePlayer) {
                    e.state = EnemyAIState::Engage;
                    break;
                }
                if (flatDist > 2.5f) {
                    moveDir = toPlayerFlat.normalized();
                    e.yaw = std::atan2(moveDir.x, -moveDir.z);
                    clip = "run";
                }
                if (e.stateTimer <= 0.0f) {
                    e.state = EnemyAIState::Patrol;
                }
                break;
            }

            case EnemyAIState::Reload: {
                e.stateTimer -= dt;
                if (e.stateTimer <= 0.0f) {
                    e.ammoInMag = (e.type == EnemyType::Enforcer) ? 45 : 30;
                    e.state = EnemyAIState::Engage;
                }
                break;
            }

            default:
                break;
        }

        // Integrate enemy movement & collision
        e.velocity.x = lerpf(e.velocity.x, moveDir.x * moveSpeed, clampf(dt * 10.0f, 0.0f, 1.0f));
        e.velocity.z = lerpf(e.velocity.z, moveDir.z * moveSpeed, clampf(dt * 10.0f, 0.0f, 1.0f));
        e.velocity.y -= 18.0f * dt;
        e.position += e.velocity * dt;

        bool grounded = false;
        collision.resolveCharacterMove(e.position, e.velocity, 0.38f, 1.82f, grounded);

        if (enemyModel) {
            animSystem.evaluateClip(*enemyModel, clip, e.animTime, true, e.pose);
        }
    }
}

} // namespace ts
