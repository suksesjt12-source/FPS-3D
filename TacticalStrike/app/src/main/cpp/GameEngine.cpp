#include "GameEngine.h"
#include "AssetLoader.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace ts {

void GameEngine::initApp(AAssetManager* assetMgr, const std::string& internalDataDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    assetMgr_ = assetMgr;
    saveMgr_.setStorageDir(internalDataDir);
    audio_.init(assetMgr_);

    physics_.setExplosionCallback([this](const Vec3& pos, float radius, float maxDmg, bool /*fromPlayer*/) {
        particles_.spawnExplosion(pos);
        lighting_.spawnTransientLight(pos + Vec3{0, 0.5f, 0}, {1.0f, 0.55f, 0.15f}, 7.0f, 14.0f, 0.35f);
        audio_.playSound(SoundId::Explosion, 1.0f);
        camera_.addShake(0.18f);
        enemies_.applyExplosionDamage(pos, radius, maxDmg);

        float playerDist = (player_.getPosition() + Vec3{0, 0.9f, 0} - pos).length();
        if (playerDist < radius) {
            float falloff = 1.0f - (playerDist / radius);
            player_.applyDamage(maxDmg * 0.55f * falloff, pos);
        }
    });
}

void GameEngine::onSurfaceCreated(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!window || !assetMgr_) return;

    if (renderer_.attachWindow(window, assetMgr_)) {
        lighting_.initShadowMap(1024);
        GLuint pAtlas = renderer_.getTextureManager().loadKtxTexture(assetMgr_, "textures/particle_atlas.ktx");
        particles_.init(pAtlas);

        int idx = saveMgr_.getProgress().lastPlayedMission;
        if (!missionLoaded_) {
            ModelLoader loader;
            if (levelMgr_.loadMission(idx, assetMgr_, renderer_.getTextureManager(),
                                      loader,
                                      collision_, enemies_, lighting_)) {
                missionLoaded_ = true;
                missionMgr_.startMission(levelMgr_.getCurrentLevel(), player_, weapons_);
                missionMgr_.setState(MissionState::Menu);
            }
        }
    }
    hasLastFrameTime_ = false;
}

void GameEngine::onSurfaceChanged(int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    renderer_.resize(width, height);
    camera_.setViewport(width, height);
}

void GameEngine::onSurfaceDestroyed() {
    std::lock_guard<std::mutex> lock(mutex_);
    particles_.destroy();
    lighting_.destroy();
    levelMgr_.clear();
    missionLoaded_ = false;
    renderer_.detachWindow();
}

void GameEngine::onPause() {
    std::lock_guard<std::mutex> lock(mutex_);
    appPaused_ = true;
    audio_.setMuted(true);
    if (missionMgr_.getState() == MissionState::Playing) {
        missionMgr_.setState(MissionState::Paused);
    }
    inputMgr_.resetAll();
}

void GameEngine::onResume() {
    std::lock_guard<std::mutex> lock(mutex_);
    appPaused_ = false;
    audio_.setMuted(false);
    hasLastFrameTime_ = false;
}

void GameEngine::setAudioMuted(bool muted) {
    audio_.setMuted(muted);
}

void GameEngine::startMission(int missionIndex) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!assetMgr_ || !renderer_.isReady()) return;
    ModelLoader loader;
    if (levelMgr_.loadMission(missionIndex, assetMgr_, renderer_.getTextureManager(),
                              loader, collision_, enemies_, lighting_)) {
        missionLoaded_ = true;
        missionRecorded_ = false;
        particles_.clear();
        physics_.clear();
        missionMgr_.startMission(levelMgr_.getCurrentLevel(), player_, weapons_);
        saveMgr_.getProgress().lastPlayedMission = missionIndex;
        saveMgr_.save();
        inputMgr_.resetAll();
    }
}

void GameEngine::restartMission() {
    int idx = levelMgr_.getCurrentLevel().missionIndex;
    startMission(idx);
}

void GameEngine::respawnAtCheckpoint() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!missionLoaded_) return;
    missionMgr_.respawnAtCheckpoint(levelMgr_.getCurrentLevel(), player_, weapons_);
    inputMgr_.resetAll();
}

void GameEngine::setPaused(bool paused) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (paused && missionMgr_.getState() == MissionState::Playing) {
        missionMgr_.setState(MissionState::Paused);
        inputMgr_.resetAll();
    } else if (!paused && missionMgr_.getState() == MissionState::Paused) {
        missionMgr_.setState(MissionState::Playing);
        hasLastFrameTime_ = false;
    }
}

void GameEngine::returnToMenu() {
    std::lock_guard<std::mutex> lock(mutex_);
    missionMgr_.setState(MissionState::Menu);
    inputMgr_.resetAll();
}

void GameEngine::setSensitivity(float touchSens, float adsMult, float gyroSens, bool gyroEnabled, bool invertY) {
    inputMgr_.setSensitivity(touchSens, adsMult, gyroSens, gyroEnabled, invertY);
}

void GameEngine::setGraphicsSettings(bool shadows, bool postProcess, float renderScale, int targetFps) {
    std::lock_guard<std::mutex> lock(mutex_);
    RenderSettings rs;
    rs.shadowsEnabled = shadows;
    rs.postProcessEnabled = postProcess;
    rs.renderScale = clampf(renderScale, 0.65f, 1.0f);
    rs.targetFps = (targetFps <= 30) ? 30 : 60;
    renderer_.setSettings(rs);
}

void GameEngine::stepAndRender() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (appPaused_ || !renderer_.isReady() || !missionLoaded_) return;

    auto now = std::chrono::steady_clock::now();
    float dt = 0.016f;
    if (hasLastFrameTime_) {
        dt = std::chrono::duration<float>(now - lastFrameTime_).count();
        dt = clampf(dt, 0.001f, 0.05f);
    }
    lastFrameTime_ = now;
    hasLastFrameTime_ = true;
    totalTimeSec_ += dt;

    fpsAccum_ += dt;
    ++fpsCounter_;
    if (fpsAccum_ >= 0.5f) {
        currentFps_ = static_cast<int>(static_cast<float>(fpsCounter_) / fpsAccum_);
        fpsAccum_ = 0.0f;
        fpsCounter_ = 0;
    }

    MissionState state = missionMgr_.getState();
    if (state == MissionState::Playing) {
        InputState inp = inputMgr_.consumeFrameInput();

        player_.update(dt, inp, weapons_.getAimFactor(), collision_, camera_, audio_);
        weapons_.update(dt, inp, player_.getSpeedRatio(), camera_, collision_,
                        enemies_, particles_, lighting_, physics_, audio_);
        physics_.update(dt, collision_);
        enemyAi_.update(dt, enemies_, player_, collision_, animSystem_,
                        levelMgr_.getEnemyModel().get(), particles_, lighting_, audio_);
        particles_.update(dt);
        lighting_.update(dt, player_.getPosition());
        missionMgr_.update(dt, inp.interactPressed, levelMgr_.getCurrentLevel(),
                           player_, weapons_, enemies_, audio_);

        if (missionMgr_.getState() == MissionState::MissionComplete && !missionRecorded_) {
            missionRecorded_ = true;
            saveMgr_.recordMissionComplete(levelMgr_.getCurrentLevel().missionIndex,
                                           missionMgr_.getMissionElapsedSec(),
                                           enemies_.getKillCount());
        }
    } else if (state == MissionState::Menu) {
        // Gentle orbital camera preview over the active mission scene
        float angle = totalTimeSec_ * 0.14f;
        Vec3 focus = levelMgr_.getCurrentLevel().playerSpawn + Vec3{0, 2.0f, -6.0f};
        camera_.setPosition(focus + Vec3{std::sin(angle) * 14.0f, 6.5f, std::cos(angle) * 14.0f});
        camera_.setAngles(angle, -0.32f);
        camera_.update(dt);
        lighting_.update(dt, focus);
    } else {
        camera_.update(dt);
        lighting_.update(dt, player_.getPosition());
    }

    renderer_.renderFrame(camera_,
                          levelMgr_.getCurrentLevel(),
                          enemies_,
                          weapons_,
                          physics_,
                          particles_,
                          lighting_,
                          levelMgr_,
                          player_.getSpeedRatio(),
                          player_.getDamageFlash(),
                          totalTimeSec_);

    // Optional frame pacing when targeting 30 FPS on mid-range thermal budgets
    if (renderer_.getSettings().targetFps <= 30) {
        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration<double, std::milli>(frameEnd - now).count();
        if (elapsedMs < 32.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(32.0 - elapsedMs)));
        }
    }
}

HudSnapshot GameEngine::getHudSnapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    HudSnapshot s;
    s.gameState = static_cast<int>(missionMgr_.getState());
    const auto& lvl = levelMgr_.getCurrentLevel();
    s.missionIndex = lvl.missionIndex;
    s.missionName = lvl.missionName;
    s.unlockedMission = saveMgr_.getProgress().unlockedMission;
    s.totalCampaignKills = saveMgr_.getProgress().totalKills;

    s.health = player_.getHealth();
    s.armor = player_.getArmor();
    s.stamina = player_.getStamina();
    s.damageFlash = player_.getDamageFlash();
    s.damageDirAngle = player_.getDamageDirAngle();
    s.playerX = player_.getPosition().x;
    s.playerZ = player_.getPosition().z;
    s.playerYaw = player_.getYaw();

    const auto& w = weapons_.getActiveWeapon();
    s.weaponSlot = weapons_.getActiveSlot();
    s.weaponName = w.spec.name;
    s.magAmmo = w.magAmmo;
    s.reserveAmmo = w.reserveAmmo;
    s.grenades = weapons_.getGrenadeCount();
    s.reloading = weapons_.isReloading();
    s.reloadProgress = weapons_.getReloadProgress();
    s.aimFactor = weapons_.getAimFactor();
    s.hitmarkerTimer = weapons_.getHitmarkerTimer();
    s.lastHitKill = weapons_.wasLastHitKill();

    s.enemiesAlive = enemies_.getAliveCount();
    s.kills = enemies_.getKillCount();
    s.elapsedSec = missionMgr_.getMissionElapsedSec();
    s.nearInteractable = missionMgr_.isNearInteractable();
    s.checkpointBanner = missionMgr_.didCheckpointJustActivate();
    s.fps = currentFps_;

    int objIdx = missionMgr_.getCurrentObjectiveIndex();
    if (objIdx >= 0 && objIdx < static_cast<int>(lvl.objectives.size())) {
        s.objectiveTitle = lvl.objectives[objIdx].title;
        s.objectiveProgress = lvl.objectives[objIdx].progress;
    } else {
        s.objectiveTitle = "MISSION COMPLETE";
        s.objectiveProgress = 1.0f;
    }
    return s;
}

} // namespace ts
