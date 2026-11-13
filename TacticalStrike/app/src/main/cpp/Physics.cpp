#include "PhysicsSystem.h"

namespace ts {

void PhysicsSystem::clear() {
    for (auto& g : grenades_) g.active = false;
}

void PhysicsSystem::spawnGrenade(const Vec3& origin, const Vec3& velocity, float fuseSec, bool fromPlayer) {
    for (auto& g : grenades_) {
        if (!g.active) {
            g.pos = origin;
            g.vel = velocity;
            g.fuseRemaining = fuseSec;
            g.fromPlayer = fromPlayer;
            g.active = true;
            return;
        }
    }
}

void PhysicsSystem::update(float dt, const CollisionSystem& collision) {
    constexpr float GRAVITY = 18.0f;
    for (auto& g : grenades_) {
        if (!g.active) continue;
        g.fuseRemaining -= dt;
        if (g.fuseRemaining <= 0.0f) {
            Vec3 blastPos = g.pos;
            bool byPlayer = g.fromPlayer;
            g.active = false;
            if (onExplosion_) {
                onExplosion_(blastPos, 7.5f, 130.0f, byPlayer);
            }
            continue;
        }

        g.vel.y -= GRAVITY * dt;
        Vec3 step = g.vel * dt;
        float dist = step.length();
        if (dist > 1e-4f) {
            RayHit hit = collision.raycastWorld({g.pos, step * (1.0f / dist)}, dist + g.radius);
            if (hit.hit) {
                g.pos = hit.point + hit.normal * (g.radius + 0.01f);
                float vn = g.vel.dot(hit.normal);
                if (vn < 0.0f) {
                    g.vel -= hit.normal * (1.45f * vn); // Restitution ~0.45
                    g.vel *= 0.72f;
                }
            } else {
                g.pos += step;
            }
        }
        if (g.pos.y < g.radius) {
            g.pos.y = g.radius;
            if (g.vel.y < 0.0f) {
                g.vel.y = -g.vel.y * 0.42f;
                g.vel.x *= 0.75f;
                g.vel.z *= 0.75f;
            }
        }
    }
}

} // namespace ts
