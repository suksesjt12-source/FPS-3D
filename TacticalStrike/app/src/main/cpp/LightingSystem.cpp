#include "LightingSystem.h"

namespace ts {

LightingSystem::~LightingSystem() {
    destroy();
}

bool LightingSystem::initShadowMap(int size) {
    destroy();
    shadowSize_ = size;

    glGenFramebuffers(1, &shadowFbo_);
    glGenTextures(1, &shadowDepthTex_);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize_, shadowSize_,
                 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex_, 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return status == GL_FRAMEBUFFER_COMPLETE;
}

void LightingSystem::destroy() {
    if (shadowDepthTex_) { glDeleteTextures(1, &shadowDepthTex_); shadowDepthTex_ = 0; }
    if (shadowFbo_) { glDeleteFramebuffers(1, &shadowFbo_); shadowFbo_ = 0; }
}

void LightingSystem::resetLights() {
    for (auto& pl : pointLights_) {
        pl = PointLight{};
    }
}

int LightingSystem::addPersistentPointLight(const Vec3& pos, const Vec3& color, float intensity, float radius) {
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (!pointLights_[i].active) {
            pointLights_[i] = {pos, color, intensity, radius, -1.0f, true};
            return i;
        }
    }
    return -1;
}

void LightingSystem::spawnTransientLight(const Vec3& pos, const Vec3& color, float intensity, float radius, float duration) {
    int slot = -1;
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (!pointLights_[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        // Replace an existing transient light if pool is full
        for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
            if (pointLights_[i].ttl >= 0.0f) {
                slot = i;
                break;
            }
        }
    }
    if (slot >= 0) {
        pointLights_[slot] = {pos, color, intensity, radius, duration, true};
    }
}

void LightingSystem::update(float dt, const Vec3& focusCenter) {
    for (auto& pl : pointLights_) {
        if (pl.active && pl.ttl >= 0.0f) {
            pl.ttl -= dt;
            if (pl.ttl <= 0.0f) {
                pl.active = false;
                pl.intensity = 0.0f;
            }
        }
    }

    Vec3 lightDir = dirLight_.direction.normalized();
    Vec3 lightPos = focusCenter - lightDir * 35.0f;
    Mat4 lightView = Mat4::lookAt(lightPos, focusCenter, {0.0f, 1.0f, 0.0f});
    Mat4 lightProj = Mat4::ortho(-28.0f, 28.0f, -28.0f, 28.0f, 1.0f, 85.0f);
    lightSpaceMatrix_ = lightProj * lightView;
}

} // namespace ts
