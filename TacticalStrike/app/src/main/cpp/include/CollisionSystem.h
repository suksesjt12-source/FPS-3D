#pragma once

#include "Math3D.h"
#include <vector>

namespace ts {

struct StaticCollider {
    AABB box;
    bool blocksBullets = true;
    bool blocksMovement = true;
    int materialTag = 0; // 0=concrete, 1=metal, 2=crate
};

struct RayHit {
    bool hit = false;
    float distance = 1e9f;
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 1, 0};
    int colliderIndex = -1;
};

class CollisionSystem {
public:
    void clear();
    void addStaticBox(const AABB& box, bool blocksBullets = true, bool blocksMovement = true, int materialTag = 0);

    bool resolveCharacterMove(Vec3& feetPos,
                              Vec3& velocity,
                              float radius,
                              float height,
                              bool& outGrounded) const;

    RayHit raycastWorld(const Ray& ray, float maxDist) const;
    bool hasLineOfSight(const Vec3& a, const Vec3& b) const;

    static bool intersectRayAABB(const Ray& ray, const AABB& box, float maxDist,
                                 float& outT, Vec3& outNormal);

    static bool intersectRayCapsuleY(const Ray& ray, const Vec3& basePos,
                                     float radius, float height, float maxDist,
                                     float& outT, Vec3& outPoint, Vec3& outNormal);

    const std::vector<StaticCollider>& getColliders() const { return colliders_; }

private:
    std::vector<StaticCollider> colliders_;
};

} // namespace ts
