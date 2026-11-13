#pragma once

#include "Math3D.h"
#include <GLES3/gl3.h>
#include <array>

namespace ts {

constexpr int MAX_POINT_LIGHTS = 8;

struct DirectionalLight {
    Vec3 direction{-0.45f, -0.82f, -0.35f};
    Vec3 color{1.0f, 0.94f, 0.86f};
    float intensity = 2.2f;
    Vec3 ambientColor{0.18f, 0.22f, 0.28f};
};

struct PointLight {
    Vec3 position{0, 0, 0};
    Vec3 color{1.0f, 0.7f, 0.3f};
    float intensity = 0.0f;
    float radius = 8.0f;
    float ttl = -1.0f; // < 0 means persistent level light
    bool active = false;
};

struct FogSettings {
    Vec3 color{0.10f, 0.13f, 0.17f};
    float density = 0.015f;
};

class LightingSystem {
public:
    ~LightingSystem();

    bool initShadowMap(int size = 1024);
    void destroy();

    void resetLights();
    void setDirectionalLight(const DirectionalLight& dirLight) { dirLight_ = dirLight; }
    void setFog(const FogSettings& fog) { fog_ = fog; }
    int addPersistentPointLight(const Vec3& pos, const Vec3& color, float intensity, float radius);
    void spawnTransientLight(const Vec3& pos, const Vec3& color, float intensity, float radius, float duration);
    void update(float dt, const Vec3& focusCenter);

    const DirectionalLight& getDirectionalLight() const { return dirLight_; }
    const FogSettings& getFog() const { return fog_; }
    const std::array<PointLight, MAX_POINT_LIGHTS>& getPointLights() const { return pointLights_; }
    const Mat4& getLightSpaceMatrix() const { return lightSpaceMatrix_; }

    GLuint getShadowFbo() const { return shadowFbo_; }
    GLuint getShadowDepthTex() const { return shadowDepthTex_; }
    int getShadowMapSize() const { return shadowSize_; }

private:
    DirectionalLight dirLight_;
    FogSettings fog_;
    std::array<PointLight, MAX_POINT_LIGHTS> pointLights_{};
    Mat4 lightSpaceMatrix_;

    GLuint shadowFbo_ = 0;
    GLuint shadowDepthTex_ = 0;
    int shadowSize_ = 1024;
};

} // namespace ts
