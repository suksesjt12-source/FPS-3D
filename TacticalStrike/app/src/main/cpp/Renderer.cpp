#include "NativeRenderer.h"
#include "AssetLoader.h"
#include <EGL/eglext.h>
#include <algorithm>
#include <cmath>

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

namespace ts {

NativeRenderer::~NativeRenderer() {
    detachWindow();
}

bool NativeRenderer::initEgl(ANativeWindow* window) {
    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        TS_LOGE("NativeRenderer: eglGetDisplay failed");
        return false;
    }
    if (!eglInitialize(eglDisplay_, nullptr, nullptr)) {
        TS_LOGE("NativeRenderer: eglInitialize failed");
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (!eglChooseConfig(eglDisplay_, configAttribs, &eglConfig_, 1, &numConfigs) || numConfigs < 1) {
        // Fallback to 16-bit depth if 24-bit is unsupported
        const EGLint fallbackAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        if (!eglChooseConfig(eglDisplay_, fallbackAttribs, &eglConfig_, 1, &numConfigs) || numConfigs < 1) {
            TS_LOGE("NativeRenderer: eglChooseConfig failed");
            return false;
        }
    }

    eglSurface_ = eglCreateWindowSurface(eglDisplay_, eglConfig_, window, nullptr);
    if (eglSurface_ == EGL_NO_SURFACE) {
        TS_LOGE("NativeRenderer: eglCreateWindowSurface failed");
        return false;
    }

    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    eglContext_ = eglCreateContext(eglDisplay_, eglConfig_, EGL_NO_CONTEXT, ctxAttribs);
    if (eglContext_ == EGL_NO_CONTEXT) {
        TS_LOGE("NativeRenderer: eglCreateContext ES 3.0 failed");
        return false;
    }

    if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
        TS_LOGE("NativeRenderer: eglMakeCurrent failed");
        return false;
    }

    eglSwapInterval(eglDisplay_, 1);
    eglQuerySurface(eglDisplay_, eglSurface_, EGL_WIDTH, &surfaceWidth_);
    eglQuerySurface(eglDisplay_, eglSurface_, EGL_HEIGHT, &surfaceHeight_);

    TS_LOGI("NativeRenderer: EGL OpenGL ES 3.0 ready (%dx%d), vendor=%s, renderer=%s",
            surfaceWidth_, surfaceHeight_,
            glGetString(GL_VENDOR), glGetString(GL_RENDERER));
    return true;
}

void NativeRenderer::destroyEgl() {
    if (eglDisplay_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglContext_ != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay_, eglContext_);
            eglContext_ = EGL_NO_CONTEXT;
        }
        if (eglSurface_ != EGL_NO_SURFACE) {
            eglDestroySurface(eglDisplay_, eglSurface_);
            eglSurface_ = EGL_NO_SURFACE;
        }
        eglTerminate(eglDisplay_);
        eglDisplay_ = EGL_NO_DISPLAY;
    }
    if (window_) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}

bool NativeRenderer::initGlResources(AAssetManager* mgr) {
    texMgr_.initDefaults();

    bool ok = true;
    ok &= shaders_.loadProgram(mgr, "pbr_static", "shaders/pbr_static.vert", "shaders/pbr_static.frag");
    ok &= shaders_.loadProgram(mgr, "pbr_skinned", "shaders/pbr_skinned.vert", "shaders/pbr_static.frag");
    ok &= shaders_.loadProgram(mgr, "shadow_static", "shaders/shadow_static.vert", "shaders/shadow.frag");
    ok &= shaders_.loadProgram(mgr, "shadow_skinned", "shaders/shadow_skinned.vert", "shaders/shadow.frag");
    ok &= shaders_.loadProgram(mgr, "skybox", "shaders/skybox.vert", "shaders/skybox.frag");
    ok &= shaders_.loadProgram(mgr, "particle", "shaders/particle.vert", "shaders/particle.frag");
    ok &= shaders_.loadProgram(mgr, "postprocess", "shaders/postprocess.vert", "shaders/postprocess.frag");

    if (!quadVao_) {
        const float quadVerts[24] = {
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 1.0f
        };
        glGenVertexArrays(1, &quadVao_);
        glGenBuffers(1, &quadVbo_);
        glBindVertexArray(quadVao_);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    PbrMaterial pkMat;
    pkMat.albedoTex = texMgr_.getDefaultWhite();
    pkMat.normalTex = texMgr_.getDefaultNormal();
    pkMat.metallic = 0.25f;
    pkMat.roughness = 0.4f;
    pickupBoxMesh_ = ModelLoader::createBoxPrimitive({0.25f, 0.20f, 0.25f}, 1.0f, pkMat);

    PbrMaterial bcMat;
    bcMat.albedoTex = texMgr_.getDefaultWhite();
    bcMat.normalTex = texMgr_.getDefaultNormal();
    bcMat.emissive = {0.15f, 0.75f, 1.0f};
    beaconMesh_ = ModelLoader::createBoxPrimitive({0.18f, 1.8f, 0.18f}, 1.0f, bcMat);

    recreatePostProcessFbo(surfaceWidth_, surfaceHeight_);
    glInitialized_ = ok;
    return ok;
}

void NativeRenderer::recreatePostProcessFbo(int width, int height) {
    destroyPostProcessFbo();
    fboWidth_ = std::max(320, static_cast<int>(static_cast<float>(width) * settings_.renderScale));
    fboHeight_ = std::max(180, static_cast<int>(static_cast<float>(height) * settings_.renderScale));

    glGenFramebuffers(1, &ppFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, ppFbo_);

    glGenTextures(1, &ppColorTex_);
    glBindTexture(GL_TEXTURE_2D, ppColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboWidth_, fboHeight_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ppColorTex_, 0);

    glGenRenderbuffers(1, &ppDepthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, ppDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fboWidth_, fboHeight_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ppDepthRbo_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TS_LOGW("NativeRenderer: post-process FBO incomplete, falling back to default framebuffer");
        destroyPostProcessFbo();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void NativeRenderer::destroyPostProcessFbo() {
    if (ppColorTex_) { glDeleteTextures(1, &ppColorTex_); ppColorTex_ = 0; }
    if (ppDepthRbo_) { glDeleteRenderbuffers(1, &ppDepthRbo_); ppDepthRbo_ = 0; }
    if (ppFbo_) { glDeleteFramebuffers(1, &ppFbo_); ppFbo_ = 0; }
}

bool NativeRenderer::attachWindow(ANativeWindow* window, AAssetManager* mgr) {
    detachWindow();
    window_ = window;
    if (!initEgl(window_)) return false;
    return initGlResources(mgr);
}

void NativeRenderer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    if (isReady()) {
        recreatePostProcessFbo(width, height);
    }
}

void NativeRenderer::detachWindow() {
    if (isReady()) {
        eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_);
        destroyPostProcessFbo();
        pickupBoxMesh_.destroy();
        beaconMesh_.destroy();
        if (quadVao_) { glDeleteVertexArrays(1, &quadVao_); quadVao_ = 0; }
        if (quadVbo_) { glDeleteBuffers(1, &quadVbo_); quadVbo_ = 0; }
        shaders_.clear();
        texMgr_.clear();
    }
    glInitialized_ = false;
    destroyEgl();
}

void NativeRenderer::setSettings(const RenderSettings& settings) {
    bool scaleChanged = std::fabs(settings.renderScale - settings_.renderScale) > 0.02f;
    settings_ = settings;
    if (scaleChanged && isReady()) {
        recreatePostProcessFbo(surfaceWidth_, surfaceHeight_);
    }
}

void NativeRenderer::bindLightingUniforms(GLuint prog, const LightingSystem& lighting, const CameraSystem& camera) {
    const auto& dl = lighting.getDirectionalLight();
    const auto& fog = lighting.getFog();
    const auto& pls = lighting.getPointLights();

    glUniform3f(shaders_.getUniformLocation(prog, "uCamPos"),
                camera.getPosition().x, camera.getPosition().y, camera.getPosition().z);
    glUniform3f(shaders_.getUniformLocation(prog, "uDirLightDir"),
                dl.direction.x, dl.direction.y, dl.direction.z);
    glUniform3f(shaders_.getUniformLocation(prog, "uDirLightColor"),
                dl.color.x * dl.intensity, dl.color.y * dl.intensity, dl.color.z * dl.intensity);
    glUniform3f(shaders_.getUniformLocation(prog, "uAmbientColor"),
                dl.ambientColor.x, dl.ambientColor.y, dl.ambientColor.z);
    glUniform3f(shaders_.getUniformLocation(prog, "uFogColor"),
                fog.color.x, fog.color.y, fog.color.z);
    glUniform1f(shaders_.getUniformLocation(prog, "uFogDensity"), fog.density);
    glUniformMatrix4fv(shaders_.getUniformLocation(prog, "uLightSpaceMatrix"),
                       1, GL_FALSE, lighting.getLightSpaceMatrix().m);
    glUniform1i(shaders_.getUniformLocation(prog, "uShadowsEnabled"),
                (settings_.shadowsEnabled && lighting.getShadowDepthTex() != 0) ? 1 : 0);

    int activeCount = 0;
    float plPos[MAX_POINT_LIGHTS * 3]{};
    float plCol[MAX_POINT_LIGHTS * 3]{};
    float plRad[MAX_POINT_LIGHTS]{};
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (pls[i].active && pls[i].intensity > 0.01f) {
            plPos[activeCount * 3 + 0] = pls[i].position.x;
            plPos[activeCount * 3 + 1] = pls[i].position.y;
            plPos[activeCount * 3 + 2] = pls[i].position.z;
            plCol[activeCount * 3 + 0] = pls[i].color.x * pls[i].intensity;
            plCol[activeCount * 3 + 1] = pls[i].color.y * pls[i].intensity;
            plCol[activeCount * 3 + 2] = pls[i].color.z * pls[i].intensity;
            plRad[activeCount] = pls[i].radius;
            ++activeCount;
        }
    }
    glUniform1i(shaders_.getUniformLocation(prog, "uNumPointLights"), activeCount);
    if (activeCount > 0) {
        glUniform3fv(shaders_.getUniformLocation(prog, "uPointLightPos"), activeCount, plPos);
        glUniform3fv(shaders_.getUniformLocation(prog, "uPointLightColor"), activeCount, plCol);
        glUniform1fv(shaders_.getUniformLocation(prog, "uPointLightRadius"), activeCount, plRad);
    }

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, lighting.getShadowDepthTex());
    glUniform1i(shaders_.getUniformLocation(prog, "uShadowMap"), 2);
}

void NativeRenderer::drawPrimitive(GLuint prog, const MeshPrimitive& prim, const Mat4& model, bool shadowPass) {
    if (!prim.vao || prim.indexCount == 0) return;

    glUniformMatrix4fv(shaders_.getUniformLocation(prog, "uModel"), 1, GL_FALSE, model.m);
    if (!shadowPass) {
        glUniform4f(shaders_.getUniformLocation(prog, "uBaseColor"),
                    prim.material.baseColor.x, prim.material.baseColor.y,
                    prim.material.baseColor.z, prim.material.baseColor.w);
        glUniform1f(shaders_.getUniformLocation(prog, "uMetallic"), prim.material.metallic);
        glUniform1f(shaders_.getUniformLocation(prog, "uRoughness"), prim.material.roughness);
        glUniform3f(shaders_.getUniformLocation(prog, "uEmissive"),
                    prim.material.emissive.x, prim.material.emissive.y, prim.material.emissive.z);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, prim.material.albedoTex ? prim.material.albedoTex : texMgr_.getDefaultWhite());
        glUniform1i(shaders_.getUniformLocation(prog, "uAlbedoTex"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, prim.material.normalTex ? prim.material.normalTex : texMgr_.getDefaultNormal());
        glUniform1i(shaders_.getUniformLocation(prog, "uNormalTex"), 1);
    }

    glBindVertexArray(prim.vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(prim.indexCount), GL_UNSIGNED_SHORT, nullptr);
}

void NativeRenderer::renderFrame(const CameraSystem& camera,
                                 const LevelData& level,
                                 const EnemySystem& enemies,
                                 const WeaponSystem& weapons,
                                 const PhysicsSystem& physics,
                                 ParticleSystem& particles,
                                 const LightingSystem& lighting,
                                 const LevelManager& levelMgr,
                                 float playerSpeedRatio,
                                 float damageFlash,
                                 float timeSec) {
    if (!isReady() || !glInitialized_) return;

    const Vec3& camPos = camera.getPosition();

    // 1. Directional Shadow Map Pass
    if (settings_.shadowsEnabled && lighting.getShadowFbo() != 0) {
        int smSize = lighting.getShadowMapSize();
        glBindFramebuffer(GL_FRAMEBUFFER, lighting.getShadowFbo());
        glViewport(0, 0, smSize, smSize);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        GLuint shStatic = shaders_.getProgram("shadow_static");
        glUseProgram(shStatic);
        glUniformMatrix4fv(shaders_.getUniformLocation(shStatic, "uLightSpaceMatrix"),
                           1, GL_FALSE, lighting.getLightSpaceMatrix().m);

        for (const auto& blk : level.staticBlocks) {
            if ((blk.worldBounds.center() - camPos).lengthSq() < 60.0f * 60.0f) {
                drawPrimitive(shStatic, blk.mesh, blk.transform, true);
            }
        }
        for (const auto& prop : level.props) {
            if (!prop.model) continue;
            if ((prop.worldBounds.center() - camPos).lengthSq() < 45.0f * 45.0f) {
                for (const auto& prim : prop.model->primitives) {
                    drawPrimitive(shStatic, prim, prop.transform, true);
                }
            }
        }

        GLuint shSkin = shaders_.getProgram("shadow_skinned");
        auto enemyModel = levelMgr.getEnemyModel();
        if (shSkin && enemyModel) {
            glUseProgram(shSkin);
            glUniformMatrix4fv(shaders_.getUniformLocation(shSkin, "uLightSpaceMatrix"),
                               1, GL_FALSE, lighting.getLightSpaceMatrix().m);
            for (const auto& e : enemies.getPool()) {
                if (!e.active) continue;
                Mat4 m = Mat4::translate(e.position) * Mat4::rotateY(-e.yaw);
                glUniformMatrix4fv(shaders_.getUniformLocation(shSkin, "uBoneMatrices"),
                                   MAX_SKIN_BONES, GL_FALSE, e.pose.boneMatrices[0].m);
                for (const auto& prim : enemyModel->primitives) {
                    drawPrimitive(shSkin, prim, m, true);
                }
            }
        }
        glCullFace(GL_BACK);
    }

    // 2. Main Scene Pass (into post-process FBO if enabled, else screen)
    bool usePp = settings_.postProcessEnabled && ppFbo_ != 0;
    if (usePp) {
        glBindFramebuffer(GL_FRAMEBUFFER, ppFbo_);
        glViewport(0, 0, fboWidth_, fboHeight_);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, surfaceWidth_, surfaceHeight_);
    }

    glClearColor(level.skyHorizonColor.x, level.skyHorizonColor.y, level.skyHorizonColor.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 2a. Skybox dome / atmospheric gradient pass
    GLuint skyProg = shaders_.getProgram("skybox");
    if (skyProg && quadVao_) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(skyProg);
        glUniform3f(shaders_.getUniformLocation(skyProg, "uSkyTop"),
                    level.skyTopColor.x, level.skyTopColor.y, level.skyTopColor.z);
        glUniform3f(shaders_.getUniformLocation(skyProg, "uSkyHorizon"),
                    level.skyHorizonColor.x, level.skyHorizonColor.y, level.skyHorizonColor.z);
        glUniform3f(shaders_.getUniformLocation(skyProg, "uCamForward"),
                    camera.getForward().x, camera.getForward().y, camera.getForward().z);
        glUniform3f(shaders_.getUniformLocation(skyProg, "uCamRight"),
                    camera.getRight().x, camera.getRight().y, camera.getRight().z);
        glUniform3f(shaders_.getUniformLocation(skyProg, "uCamUp"),
                    camera.getUp().x, camera.getUp().y, camera.getUp().z);
        glUniform1f(shaders_.getUniformLocation(skyProg, "uTanHalfFov"),
                    std::tan(camera.getFovDeg() * DEG2RAD * 0.5f));
        glUniform1f(shaders_.getUniformLocation(skyProg, "uAspect"), camera.getAspect());
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDepthMask(GL_TRUE);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // 2b. Static Level Architecture & Props (with Frustum Culling + Distance LOD)
    GLuint pbrStatic = shaders_.getProgram("pbr_static");
    if (pbrStatic) {
        glUseProgram(pbrStatic);
        glUniformMatrix4fv(shaders_.getUniformLocation(pbrStatic, "uViewProj"),
                           1, GL_FALSE, camera.getViewProjMatrix().m);
        bindLightingUniforms(pbrStatic, lighting, camera);

        for (const auto& blk : level.staticBlocks) {
            float distSq = (blk.worldBounds.center() - camPos).lengthSq();
            if (distSq > blk.lodCullDistance * blk.lodCullDistance) continue;
            if (!camera.isBoxVisible(blk.worldBounds)) continue;
            drawPrimitive(pbrStatic, blk.mesh, blk.transform, false);
        }

        for (const auto& prop : level.props) {
            if (!prop.model) continue;
            float distSq = (prop.worldBounds.center() - camPos).lengthSq();
            if (distSq > 95.0f * 95.0f) continue;
            if (!camera.isBoxVisible(prop.worldBounds)) continue;
            for (const auto& prim : prop.model->primitives) {
                drawPrimitive(pbrStatic, prim, prop.transform, false);
            }
        }

        // Pickups
        for (const auto& pk : level.pickups) {
            if (pk.collected) continue;
            Vec3 pos = pk.position + Vec3{0.0f, 0.35f + std::sin(pk.bobPhase) * 0.12f, 0.0f};
            Mat4 m = Mat4::translate(pos) * Mat4::rotateY(pk.bobPhase * 0.8f);
            if (pk.kind == LevelPickup::Kind::Health) {
                pickupBoxMesh_.material.baseColor = {0.15f, 0.85f, 0.35f, 1.0f};
                pickupBoxMesh_.material.emissive = {0.05f, 0.45f, 0.15f};
            } else if (pk.kind == LevelPickup::Kind::Armor) {
                pickupBoxMesh_.material.baseColor = {0.20f, 0.65f, 1.0f, 1.0f};
                pickupBoxMesh_.material.emissive = {0.05f, 0.25f, 0.55f};
            } else {
                pickupBoxMesh_.material.baseColor = {0.95f, 0.72f, 0.18f, 1.0f};
                pickupBoxMesh_.material.emissive = {0.35f, 0.22f, 0.02f};
            }
            drawPrimitive(pbrStatic, pickupBoxMesh_, m, false);
        }

        // Active Objective Beacon
        for (const auto& obj : level.objectives) {
            if (obj.completed) continue;
            if (obj.type == ObjectiveSpec::Type::ReachZone || obj.type == ObjectiveSpec::Type::HackTerminal) {
                Mat4 m = Mat4::translate(obj.targetPos + Vec3{0.0f, 1.8f, 0.0f}) * Mat4::rotateY(timeSec * 1.5f);
                beaconMesh_.material.baseColor = {0.15f, 0.85f, 1.0f, 1.0f};
                beaconMesh_.material.emissive = {0.2f, 0.85f, 1.0f};
                drawPrimitive(pbrStatic, beaconMesh_, m, false);
            }
            break; // Only render current active objective beacon
        }

        // Thrown Grenades
        auto grenadeMdl = levelMgr.getGrenadeModel();
        if (grenadeMdl) {
            for (const auto& g : physics.getGrenades()) {
                if (!g.active) continue;
                Mat4 m = Mat4::translate(g.pos) * Mat4::rotateY(timeSec * 8.0f) * Mat4::rotateX(timeSec * 6.0f);
                for (const auto& prim : grenadeMdl->primitives) {
                    drawPrimitive(pbrStatic, prim, m, false);
                }
            }
        }
    }

    // 2c. Skeletal Skinned Enemies (with Frustum Culling)
    GLuint pbrSkinned = shaders_.getProgram("pbr_skinned");
    auto enemyModel = levelMgr.getEnemyModel();
    if (pbrSkinned && enemyModel) {
        glUseProgram(pbrSkinned);
        glUniformMatrix4fv(shaders_.getUniformLocation(pbrSkinned, "uViewProj"),
                           1, GL_FALSE, camera.getViewProjMatrix().m);
        bindLightingUniforms(pbrSkinned, lighting, camera);

        for (const auto& e : enemies.getPool()) {
            if (!e.active) continue;
            AABB enemyBox{
                {e.position.x - 0.6f, e.position.y, e.position.z - 0.6f},
                {e.position.x + 0.6f, e.position.y + 2.0f, e.position.z + 0.6f}
            };
            if (!camera.isBoxVisible(enemyBox)) continue;

            Mat4 model = Mat4::translate(e.position) * Mat4::rotateY(-e.yaw);
            glUniformMatrix4fv(shaders_.getUniformLocation(pbrSkinned, "uBoneMatrices"),
                               MAX_SKIN_BONES, GL_FALSE, e.pose.boneMatrices[0].m);

            for (auto prim : enemyModel->primitives) {
                if (e.hitFlash > 0.0f) {
                    prim.material.emissive = {0.8f, 0.15f, 0.10f};
                } else if (e.type == EnemyType::Enforcer) {
                    prim.material.baseColor = {0.42f, 0.28f, 0.26f, 1.0f};
                } else if (e.type == EnemyType::Scout) {
                    prim.material.baseColor = {0.32f, 0.42f, 0.36f, 1.0f};
                }
                drawPrimitive(pbrSkinned, prim, model, false);
            }
        }
    }

    // 2d. Particles
    particles.render(shaders_.getProgram("particle"),
                     camera.getViewProjMatrix(),
                     camera.getRight(),
                     camera.getUp());

    // 2e. First-Person Weapon Viewmodel (foreground depth range)
    auto weaponModel = levelMgr.getWeaponModel(weapons.getActiveSlot());
    if (pbrStatic && weaponModel) {
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(pbrStatic);
        glUniformMatrix4fv(shaders_.getUniformLocation(pbrStatic, "uViewProj"),
                           1, GL_FALSE, camera.getViewProjMatrix().m);
        bindLightingUniforms(pbrStatic, lighting, camera);
        Mat4 vmMat = weapons.getViewmodelMatrix(camera, playerSpeedRatio);
        for (const auto& prim : weaponModel->primitives) {
            drawPrimitive(pbrStatic, prim, vmMat, false);
        }
    }

    // 3. Post-Processing Pass (ACES Tone Mapping, Vignette, Damage Flash)
    if (usePp) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, surfaceWidth_, surfaceHeight_);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        GLuint ppProg = shaders_.getProgram("postprocess");
        glUseProgram(ppProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ppColorTex_);
        glUniform1i(shaders_.getUniformLocation(ppProg, "uSceneTex"), 0);
        glUniform1f(shaders_.getUniformLocation(ppProg, "uDamageFlash"), damageFlash);
        glUniform1f(shaders_.getUniformLocation(ppProg, "uAdsFactor"), weapons.getAimFactor());
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    eglSwapBuffers(eglDisplay_, eglSurface_);
}

} // namespace ts
