#include "CollisionSystem.h"
#include <algorithm>
#include <cmath>

namespace ts {

void CollisionSystem::clear() {
    colliders_.clear();
}

void CollisionSystem::addStaticBox(const AABB& box, bool blocksBullets, bool blocksMovement, int materialTag) {
    colliders_.push_back({box, blocksBullets, blocksMovement, materialTag});
}

bool CollisionSystem::resolveCharacterMove(Vec3& feetPos,
                                           Vec3& velocity,
                                           float radius,
                                           float height,
                                           bool& outGrounded) const {
    outGrounded = false;
    bool collided = false;

    // Ground floor y = 0 fallback
    if (feetPos.y <= 0.0f) {
        feetPos.y = 0.0f;
        if (velocity.y < 0.0f) velocity.y = 0.0f;
        outGrounded = true;
    }

    for (int iter = 0; iter < 3; ++iter) {
        AABB charBox{
            {feetPos.x - radius, feetPos.y, feetPos.z - radius},
            {feetPos.x + radius, feetPos.y + height, feetPos.z + radius}
        };

        for (const auto& c : colliders_) {
            if (!c.blocksMovement || !charBox.intersects(c.box)) continue;

            float penX1 = c.box.max.x - charBox.min.x;
            float penX2 = charBox.max.x - c.box.min.x;
            float penY1 = c.box.max.y - charBox.min.y;
            float penY2 = charBox.max.y - charBox.min.y;
            float penZ1 = c.box.max.z - charBox.min.z;
            float penZ2 = charBox.max.z - c.box.min.z;

            float minX = (penX1 < penX2) ? penX1 : -penX2;
            float minY = (penY1 < penY2) ? penY1 : -penY2;
            float minZ = (penZ1 < penZ2) ? penZ1 : -penZ2;

            // Step-up support for low curbs/platforms (<= 0.45m)
            if (penY1 > 0.0f && penY1 <= 0.45f && velocity.y <= 0.5f) {
                feetPos.y += penY1;
                velocity.y = 0.0f;
                outGrounded = true;
                collided = true;
                charBox.min.y = feetPos.y;
                charBox.max.y = feetPos.y + height;
                continue;
            }

            float ax = std::fabs(minX);
            float ay = std::fabs(minY);
            float az = std::fabs(minZ);

            if (ay <= ax && ay <= az) {
                feetPos.y += minY;
                if (minY > 0.0f) {
                    outGrounded = true;
                    if (velocity.y < 0.0f) velocity.y = 0.0f;
                } else if (velocity.y > 0.0f) {
                    velocity.y = 0.0f;
                }
            } else if (ax <= az) {
                feetPos.x += minX;
                velocity.x = 0.0f;
            } else {
                feetPos.z += minZ;
                velocity.z = 0.0f;
            }
            collided = true;
            charBox.min = {feetPos.x - radius, feetPos.y, feetPos.z - radius};
            charBox.max = {feetPos.x + radius, feetPos.y + height, feetPos.z + radius};
        }
    }

    return collided;
}

bool CollisionSystem::intersectRayAABB(const Ray& ray, const AABB& box, float maxDist,
                                       float& outT, Vec3& outNormal) {
    float tmin = 0.0f;
    float tmax = maxDist;
    Vec3 hitNorm{0, 0, 0};

    const float orig[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dir[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    const float bmin[3] = {box.min.x, box.min.y, box.min.z};
    const float bmax[3] = {box.max.x, box.max.y, box.max.z};

    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dir[i]) < 1e-6f) {
            if (orig[i] < bmin[i] || orig[i] > bmax[i]) return false;
        } else {
            float invD = 1.0f / dir[i];
            float t1 = (bmin[i] - orig[i]) * invD;
            float t2 = (bmax[i] - orig[i]) * invD;
            Vec3 n{0, 0, 0};
            if (i == 0) n.x = (invD < 0.0f) ? 1.0f : -1.0f;
            if (i == 1) n.y = (invD < 0.0f) ? 1.0f : -1.0f;
            if (i == 2) n.z = (invD < 0.0f) ? 1.0f : -1.0f;

            if (t1 > t2) {
                std::swap(t1, t2);
            }
            if (t1 > tmin) {
                tmin = t1;
                hitNorm = n;
            }
            if (t2 < tmax) {
                tmax = t2;
            }
            if (tmin > tmax) return false;
        }
    }

    if (tmin < 0.0f || tmin > maxDist) return false;
    outT = tmin;
    outNormal = hitNorm;
    return true;
}

bool CollisionSystem::intersectRayCapsuleY(const Ray& ray, const Vec3& basePos,
                                           float radius, float height, float maxDist,
                                           float& outT, Vec3& outPoint, Vec3& outNormal) {
    AABB approx{
        {basePos.x - radius, basePos.y, basePos.z - radius},
        {basePos.x + radius, basePos.y + height, basePos.z + radius}
    };
    float tBox = 0.0f;
    Vec3 nBox;
    if (!intersectRayAABB(ray, approx, maxDist, tBox, nBox)) return false;

    // Refine horizontal radial distance at midpoint segment
    Vec3 p = ray.origin + ray.dir * tBox;
    float dx = p.x - basePos.x;
    float dz = p.z - basePos.z;
    float distSq = dx * dx + dz * dz;
    if (distSq > radius * radius * 1.35f) return false;

    outT = tBox;
    outPoint = p;
    outNormal = nBox;
    return true;
}

RayHit CollisionSystem::raycastWorld(const Ray& ray, float maxDist) const {
    RayHit best;
    best.distance = maxDist;
    for (size_t i = 0; i < colliders_.size(); ++i) {
        if (!colliders_[i].blocksBullets) continue;
        float t = 0.0f;
        Vec3 norm;
        if (intersectRayAABB(ray, colliders_[i].box, best.distance, t, norm)) {
            best.hit = true;
            best.distance = t;
            best.point = ray.origin + ray.dir * t;
            best.normal = norm;
            best.colliderIndex = static_cast<int>(i);
        }
    }
    return best;
}

bool CollisionSystem::hasLineOfSight(const Vec3& a, const Vec3& b) const {
    Vec3 diff = b - a;
    float dist = diff.length();
    if (dist < 0.05f) return true;
    Ray ray{a, diff * (1.0f / dist)};
    RayHit hit = raycastWorld(ray, dist - 0.05f);
    return !hit.hit;
}

} // namespace ts
