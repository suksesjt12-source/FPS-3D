#include "ParticleSystem.h"
#include <algorithm>
#include <cstddef>

namespace ts {

ParticleSystem::~ParticleSystem() {
    destroy();
}

void ParticleSystem::init(GLuint atlasTexture) {
    destroy();
    atlasTex_ = atlasTexture;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 4 * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);

    indices_.reserve(MAX_PARTICLES * 6);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        uint16_t base = static_cast<uint16_t>(i * 4);
        indices_.push_back(base + 0);
        indices_.push_back(base + 1);
        indices_.push_back(base + 2);
        indices_.push_back(base + 0);
        indices_.push_back(base + 2);
        indices_.push_back(base + 3);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices_.size() * sizeof(uint16_t)),
                 indices_.data(),
                 GL_STATIC_DRAW);

    constexpr GLsizei stride = sizeof(ParticleVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(ParticleVertex, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(ParticleVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(ParticleVertex, r)));

    glBindVertexArray(0);
    vertices_.reserve(MAX_PARTICLES * 4);
}

void ParticleSystem::destroy() {
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ibo_) { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
}

void ParticleSystem::clear() {
    for (auto& p : pool_) p.active = false;
}

float ParticleSystem::randRange(float lo, float hi) {
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    float t = static_cast<float>(rngState_ & 0xFFFFu) / 65535.0f;
    return lo + (hi - lo) * t;
}

Particle* ParticleSystem::allocParticle() {
    for (auto& p : pool_) {
        if (!p.active) return &p;
    }
    // Reuse oldest active particle
    Particle* oldest = &pool_[0];
    for (auto& p : pool_) {
        if (p.age > oldest->age) oldest = &p;
    }
    return oldest;
}

void ParticleSystem::spawnMuzzleFlash(const Vec3& pos, const Vec3& dir) {
    for (int i = 0; i < 4; ++i) {
        Particle* p = allocParticle();
        p->active = true;
        p->pos = pos + dir * (0.04f * static_cast<float>(i));
        p->vel = dir * randRange(2.0f, 5.0f) + Vec3{randRange(-0.4f, 0.4f), randRange(-0.4f, 0.4f), randRange(-0.4f, 0.4f)};
        p->color = {1.0f, 0.78f, 0.30f, 0.95f};
        p->size = randRange(0.14f, 0.26f);
        p->age = 0.0f;
        p->life = 0.06f;
        p->gravity = 0.0f;
        p->additive = true;
    }
}

void ParticleSystem::spawnTracer(const Vec3& start, const Vec3& end, const Vec4& color) {
    Vec3 delta = end - start;
    float dist = delta.length();
    if (dist < 0.25f) return;
    Vec3 dir = delta * (1.0f / dist);
    float speed = 135.0f;
    float travelLife = clampf(dist / speed, 0.04f, 0.14f);
    for (int i = 0; i < 4; ++i) {
        Particle* p = allocParticle();
        p->active = true;
        p->pos = start + dir * (0.25f + static_cast<float>(i) * 0.28f);
        p->vel = dir * speed;
        p->color = color;
        p->size = 0.065f - static_cast<float>(i) * 0.008f;
        p->age = 0.0f;
        p->life = travelLife;
        p->gravity = 0.0f;
        p->additive = true;
    }
}

void ParticleSystem::spawnImpactSparks(const Vec3& pos, const Vec3& normal, int count) {
    for (int i = 0; i < count; ++i) {
        Particle* p = allocParticle();
        p->active = true;
        p->pos = pos + normal * 0.03f;
        p->vel = normal * randRange(2.0f, 5.5f) +
                 Vec3{randRange(-2.5f, 2.5f), randRange(0.5f, 3.0f), randRange(-2.5f, 2.5f)};
        p->color = {1.0f, 0.72f, 0.28f, 1.0f};
        p->size = randRange(0.05f, 0.11f);
        p->age = 0.0f;
        p->life = randRange(0.18f, 0.36f);
        p->gravity = 12.0f;
        p->additive = true;
    }
    // Dust puff
    for (int i = 0; i < 3; ++i) {
        Particle* p = allocParticle();
        p->active = true;
        p->pos = pos + normal * 0.05f;
        p->vel = normal * randRange(0.4f, 1.2f) + Vec3{randRange(-0.5f, 0.5f), 0.6f, randRange(-0.5f, 0.5f)};
        p->color = {0.55f, 0.54f, 0.50f, 0.5f};
        p->size = randRange(0.22f, 0.42f);
        p->age = 0.0f;
        p->life = randRange(0.4f, 0.7f);
        p->gravity = -0.3f;
        p->additive = false;
    }
}

void ParticleSystem::spawnBloodBurst(const Vec3& pos, const Vec3& normal, int count) {
    for (int i = 0; i < count; ++i) {
        Particle* p = allocParticle();
        p->active = true;
        p->pos = pos;
        p->vel = normal * randRange(1.2f, 3.8f) +
                 Vec3{randRange(-1.8f, 1.8f), randRange(0.2f, 2.0f), randRange(-1.8f, 1.8f)};
        p->color = {0.72f, 0.06f, 0.06f, 0.92f};
        p->size = randRange(0.08f, 0.16f);
        p->age = 0.0f;
        p->life = randRange(0.22f, 0.45f);
        p->gravity = 9.8f;
        p->additive = false;
    }
}

void ParticleSystem::spawnExplosion(const Vec3& pos) {
    // Fireball
    for (int i = 0; i < 24; ++i) {
        Particle* p = allocParticle();
        p->active = true;
        p->pos = pos + Vec3{randRange(-0.4f, 0.4f), randRange(0.0f, 0.6f), randRange(-0.4f, 0.4f)};
        p->vel = Vec3{randRange(-6.0f, 6.0f), randRange(1.5f, 9.0f), randRange(-6.0f, 6.0f)};
        p->color = {1.0f, randRange(0.35f, 0.72f), 0.12f, 1.0f};
        p->size = randRange(0.45f, 1.1f);
        p->age = 0.0f;
        p->life = randRange(0.35f, 0.75f);
        p->gravity = 2.5f;
        p->additive = true;
    }
    // Smoke cloud
    for (int i = 0; i < 18; ++i) {
        Particle* p = allocParticle();
        p->active = true;
        p->pos = pos + Vec3{randRange(-0.6f, 0.6f), randRange(0.1f, 0.8f), randRange(-0.6f, 0.6f)};
        p->vel = Vec3{randRange(-2.2f, 2.2f), randRange(1.5f, 4.5f), randRange(-2.2f, 2.2f)};
        p->color = {0.22f, 0.22f, 0.24f, 0.75f};
        p->size = randRange(0.7f, 1.6f);
        p->age = 0.0f;
        p->life = randRange(0.9f, 1.6f);
        p->gravity = -0.8f;
        p->additive = false;
    }
}

void ParticleSystem::update(float dt) {
    for (auto& p : pool_) {
        if (!p.active) continue;
        p.age += dt;
        if (p.age >= p.life) {
            p.active = false;
            continue;
        }
        p.vel.y -= p.gravity * dt;
        p.pos += p.vel * dt;
    }
}

void ParticleSystem::render(GLuint shaderProg,
                            const Mat4& viewProj,
                            const Vec3& camRight,
                            const Vec3& camUp) {
    if (!shaderProg || !vao_) return;

    auto drawBatch = [&](bool additiveMode) {
        vertices_.clear();
        int count = 0;
        for (const auto& p : pool_) {
            if (!p.active || p.additive != additiveMode) continue;
            float t = clampf(p.age / p.life, 0.0f, 1.0f);
            float alpha = p.color.w * (1.0f - t);
            float half = p.size * (additiveMode ? (1.0f - t * 0.35f) : (0.75f + t * 0.65f));
            Vec3 r = camRight * half;
            Vec3 u = camUp * half;

            Vec3 p0 = p.pos - r - u;
            Vec3 p1 = p.pos + r - u;
            Vec3 p2 = p.pos + r + u;
            Vec3 p3 = p.pos - r + u;

            vertices_.push_back({p0.x, p0.y, p0.z, 0.0f, 0.0f, p.color.x, p.color.y, p.color.z, alpha});
            vertices_.push_back({p1.x, p1.y, p1.z, 1.0f, 0.0f, p.color.x, p.color.y, p.color.z, alpha});
            vertices_.push_back({p2.x, p2.y, p2.z, 1.0f, 1.0f, p.color.x, p.color.y, p.color.z, alpha});
            vertices_.push_back({p3.x, p3.y, p3.z, 0.0f, 1.0f, p.color.x, p.color.y, p.color.z, alpha});
            ++count;
        }
        if (count == 0) return;

        glBlendFunc(GL_SRC_ALPHA, additiveMode ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        static_cast<GLsizeiptr>(vertices_.size() * sizeof(ParticleVertex)),
                        vertices_.data());
        glDrawElements(GL_TRIANGLES, count * 6, GL_UNSIGNED_SHORT, nullptr);
    };

    glUseProgram(shaderProg);
    GLint vpLoc = glGetUniformLocation(shaderProg, "uViewProj");
    if (vpLoc >= 0) glUniformMatrix4fv(vpLoc, 1, GL_FALSE, viewProj.m);
    GLint texLoc = glGetUniformLocation(shaderProg, "uParticleTex");
    if (texLoc >= 0) glUniform1i(texLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTex_);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao_);

    drawBatch(false);
    drawBatch(true);

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} // namespace ts
