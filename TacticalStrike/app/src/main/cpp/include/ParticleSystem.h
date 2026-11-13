#pragma once

#include "Math3D.h"
#include <GLES3/gl3.h>
#include <array>
#include <vector>

namespace ts {

constexpr int MAX_PARTICLES = 512;

enum class ParticleKind {
    MuzzleFlash,
    Spark,
    Smoke,
    BloodImpact,
    Explosion
};

struct Particle {
    Vec3 pos{0, 0, 0};
    Vec3 vel{0, 0, 0};
    Vec4 color{1, 1, 1, 1};
    float size = 0.2f;
    float age = 0.0f;
    float life = 0.5f;
    float gravity = 0.0f;
    bool additive = false;
    bool active = false;
};

struct ParticleVertex {
    float px, py, pz;
    float u, v;
    float r, g, b, a;
};

class ParticleSystem {
public:
    ~ParticleSystem();

    void init(GLuint atlasTexture);
    void destroy();
    void clear();

    void spawnMuzzleFlash(const Vec3& pos, const Vec3& dir);
    void spawnImpactSparks(const Vec3& pos, const Vec3& normal, int count = 10);
    void spawnBloodBurst(const Vec3& pos, const Vec3& normal, int count = 10);
    void spawnExplosion(const Vec3& pos);

    void update(float dt);
    void render(GLuint shaderProg,
                const Mat4& viewProj,
                const Vec3& camRight,
                const Vec3& camUp);

private:
    Particle* allocParticle();

    std::array<Particle, MAX_PARTICLES> pool_{};
    std::vector<ParticleVertex> vertices_;
    std::vector<uint16_t> indices_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
    GLuint atlasTex_ = 0;
    uint32_t rngState_ = 0x9E3779B9u;

    float randRange(float lo, float hi);
};

} // namespace ts
