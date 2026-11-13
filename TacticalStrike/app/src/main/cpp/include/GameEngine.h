#pragma once

#include "AnimationSystem.h"
#include "AudioSystem.h"
#include "CameraSystem.h"
#include "CollisionSystem.h"
#include "EnemyAI.h"
#include "EnemySystem.h"
#include "InputManager.h"
#include "LevelManager.h"
#include "LightingSystem.h"
#include "MissionManager.h"
#include "NativeRenderer.h"
#include "ParticleSystem.h"
#include "PhysicsSystem.h"
#include "PlayerController.h"
#include "SaveManager.h"
#include "WeaponSystem.h"
#include <android/asset_manager.h>
#include <android/native_window.h>
#include <chrono>
#include <mutex>
#include <string>

namespace ts {

struct HudSnapshot {
    int gameState = 0; // 0=Menu, 1=Playing, 2=Paused, 3=Dead, 4=MissionComplete
    int missionIndex = 0;
    int unlockedMission = 0;
    float health = 100.0f;
    float armor = 50.0f;
    float stamina = 100.0f;
    int weaponSlot = 0;
    int magAmmo = 30;
    int reserveAmmo = 150;
    int grenades = 2;
    bool reloading = false;
    float reloadProgress = 0.0f;
    float aimFactor = 0.0f;
    float hitmarkerTimer = 0.0f;
    bool lastHitKill = false;
    float damageFlash = 0.0f;
    float damageDirAngle = 0.0f;
    int enemiesAlive = 0;
    int kills = 0;
    int totalCampaignKills = 0;
    float elapsedSec = 0.0f;
    bool nearInteractable = false;
    bool checkpointBanner = false;
    float objectiveProgress = 0.0f;
    int fps = 60;
    std::string weaponName;
    std::string missionName;
    std::string objectiveTitle;
    float playerX = 0.0f;
    float playerZ = 0.0f;
    float playerYaw = 0.0f;
    bool sprinting = false;
    int weaponSkin = 0;
    bool lastHitHeadshot = false;
    float targetInfoTimer = 0.0f;
    int targetEnemyType = 0;
    float targetEnemyHpRatio = 0.0f;
    float objRelX = 9999.0f;
    float objRelZ = 9999.0f;
    int radarCount = 0;
    float radarEnemies[8 * 3]{};
};

class GameEngine {
public:
    void initApp(AAssetManager* assetMgr, const std::string& internalDataDir);
    void onSurfaceCreated(ANativeWindow* window);
    void onSurfaceChanged(int width, int height);
    void onSurfaceDestroyed();

    void onPause();
    void onResume();
    void setAudioMuted(bool muted);

    void startMission(int missionIndex);
    void restartMission();
    void respawnAtCheckpoint();
    void setPaused(bool paused);
    void returnToMenu();

    void setSensitivity(float touchSens, float adsMult, float gyroSens, bool gyroEnabled, bool invertY);
    void setGraphicsSettings(bool shadows, bool postProcess, float renderScale, int targetFps);
    void setWeaponSkin(int skinIndex);

    InputManager& getInputManager() { return inputMgr_; }
    void stepAndRender();
    HudSnapshot getHudSnapshot();

private:
    std::mutex mutex_;
    AAssetManager* assetMgr_ = nullptr;
    bool appPaused_ = false;
    bool missionLoaded_ = false;
    bool missionRecorded_ = false;
    int pendingMissionStart_ = -1;
    int weaponSkin_ = 0;

    void loadMissionOnGlThread(int missionIndex);

    NativeRenderer renderer_;
    CameraSystem camera_;
    PlayerController player_;
    WeaponSystem weapons_;
    EnemySystem enemies_;
    EnemyAI enemyAi_;
    PhysicsSystem physics_;
    CollisionSystem collision_;
    AnimationSystem animSystem_;
    AudioSystem audio_;
    ParticleSystem particles_;
    LightingSystem lighting_;
    LevelManager levelMgr_;
    MissionManager missionMgr_;
    SaveManager saveMgr_;
    InputManager inputMgr_;

    std::chrono::steady_clock::time_point lastFrameTime_;
    bool hasLastFrameTime_ = false;
    float totalTimeSec_ = 0.0f;
    float fpsAccum_ = 0.0f;
    int fpsCounter_ = 0;
    int currentFps_ = 60;
};

} // namespace ts
