#pragma once

#include "CameraSystem.h"
#include "EnemySystem.h"
#include "LevelManager.h"
#include "LightingSystem.h"
#include "Math3D.h"
#include "ParticleSystem.h"
#include "PhysicsSystem.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "WeaponSystem.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/native_window.h>

namespace ts {

struct RenderSettings {
    bool shadowsEnabled = true;
    bool postProcessEnabled = true;
    float renderScale = 1.0f; // 0.65..1.0
    int targetFps = 60;
};

class NativeRenderer {
public:
    ~NativeRenderer();

    bool attachWindow(ANativeWindow* window, AAssetManager* mgr);
    void resize(int width, int height);
    void detachWindow();

    void setSettings(const RenderSettings& settings);
    const RenderSettings& getSettings() const { return settings_; }

    void renderFrame(const CameraSystem& camera,
                     const LevelData& level,
                     const EnemySystem& enemies,
                     const WeaponSystem& weapons,
                     const PhysicsSystem& physics,
                     ParticleSystem& particles,
                     const LightingSystem& lighting,
                     const LevelManager& levelMgr,
                     float playerSpeedRatio,
                     float damageFlash,
                     float timeSec);

    TextureManager& getTextureManager() { return texMgr_; }
    bool isReady() const { return eglDisplay_ != EGL_NO_DISPLAY && eglSurface_ != EGL_NO_SURFACE && eglContext_ != EGL_NO_CONTEXT; }

private:
    bool initEgl(ANativeWindow* window);
    void destroyEgl();
    bool initGlResources(AAssetManager* mgr);
    void recreatePostProcessFbo(int width, int height);
    void destroyPostProcessFbo();

    void drawPrimitive(GLuint prog, const MeshPrimitive& prim, const Mat4& model, bool shadowPass);
    void bindLightingUniforms(GLuint prog, const LightingSystem& lighting, const CameraSystem& camera);

    ANativeWindow* window_ = nullptr;
    EGLDisplay eglDisplay_ = EGL_NO_DISPLAY;
    EGLSurface eglSurface_ = EGL_NO_SURFACE;
    EGLContext eglContext_ = EGL_NO_CONTEXT;
    EGLConfig eglConfig_ = nullptr;

    int surfaceWidth_ = 1280;
    int surfaceHeight_ = 720;
    int fboWidth_ = 1280;
    int fboHeight_ = 720;

    RenderSettings settings_;
    ShaderManager shaders_;
    TextureManager texMgr_;

    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;
    GLuint ppFbo_ = 0;
    GLuint ppColorTex_ = 0;
    GLuint ppDepthRbo_ = 0;

    MeshPrimitive pickupBoxMesh_;
    MeshPrimitive beaconMesh_;
    bool glInitialized_ = false;
};

} // namespace ts
