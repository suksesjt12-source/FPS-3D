#pragma once

#include "AudioSystem.h"
#include "EnemySystem.h"
#include "LevelManager.h"
#include "PlayerController.h"
#include "WeaponSystem.h"
#include <string>

namespace ts {

enum class MissionState {
    Menu = 0,
    Playing,
    Paused,
    PlayerDead,
    MissionComplete
};

class MissionManager {
public:
    void startMission(LevelData& level, PlayerController& player, WeaponSystem& weapons);
    void respawnAtCheckpoint(const LevelData& level, PlayerController& player, WeaponSystem& weapons);

    void update(float dt,
                bool interactPressed,
                LevelData& level,
                PlayerController& player,
                WeaponSystem& weapons,
                const EnemySystem& enemies,
                AudioSystem& audio);

    MissionState getState() const { return state_; }
    void setState(MissionState s) { state_ = s; }

    int getCurrentObjectiveIndex() const { return activeObjIdx_; }
    int getActiveCheckpointIndex() const { return activeCheckpointIdx_; }
    float getMissionElapsedSec() const { return elapsedSec_; }
    const std::string& getCurrentObjectiveText() const;
    float getCurrentObjectiveProgress() const;
    bool isNearInteractable() const { return nearInteractable_; }
    bool didCheckpointJustActivate() const { return checkpointBannerTimer_ > 0.0f; }

private:
    MissionState state_ = MissionState::Menu;
    int activeObjIdx_ = 0;
    int activeCheckpointIdx_ = -1;
    Vec3 respawnPos_{0, 0, 0};
    float respawnYaw_ = 0.0f;
    float elapsedSec_ = 0.0f;
    float checkpointBannerTimer_ = 0.0f;
    bool nearInteractable_ = false;
    std::string fallbackText_ = "ALL OBJECTIVES COMPLETE";
};

} // namespace ts
