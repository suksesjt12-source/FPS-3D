#include "MissionManager.h"
#include <algorithm>

namespace ts {

void MissionManager::startMission(LevelData& level, PlayerController& player, WeaponSystem& weapons) {
    state_ = MissionState::Playing;
    activeObjIdx_ = 0;
    activeCheckpointIdx_ = -1;
    respawnPos_ = level.playerSpawn;
    respawnYaw_ = level.playerSpawnYaw;
    elapsedSec_ = 0.0f;
    checkpointBannerTimer_ = 0.0f;
    nearInteractable_ = false;

    player.spawnAt(respawnPos_, respawnYaw_);
    weapons.resetLoadout();
}

void MissionManager::respawnAtCheckpoint(const LevelData& /*level*/,
                                           PlayerController& player,
                                           WeaponSystem& weapons) {
    player.spawnAt(respawnPos_, respawnYaw_);
    weapons.addAmmo(60, 80, 24, 1);
    state_ = MissionState::Playing;
}

void MissionManager::update(float dt,
                            bool interactPressed,
                            LevelData& level,
                            PlayerController& player,
                            WeaponSystem& weapons,
                            const EnemySystem& enemies,
                            AudioSystem& audio) {
    if (checkpointBannerTimer_ > 0.0f) {
        checkpointBannerTimer_ = std::max(0.0f, checkpointBannerTimer_ - dt);
    }

    if (state_ != MissionState::Playing) return;

    if (!player.isAlive()) {
        state_ = MissionState::PlayerDead;
        return;
    }

    elapsedSec_ += dt;
    const Vec3& pPos = player.getPosition();

    // Activate checkpoints
    for (auto& cp : level.checkpoints) {
        if (!cp.reached) {
            float d = (pPos - cp.position).length();
            if (d <= cp.radius) {
                cp.reached = true;
                activeCheckpointIdx_ = cp.index;
                respawnPos_ = cp.position;
                respawnYaw_ = player.getYaw();
                checkpointBannerTimer_ = 2.5f;
                audio.playSound(SoundId::ObjectiveComplete, 0.55f);
            }
        }
    }

    // Collect pickups
    for (auto& pk : level.pickups) {
        pk.bobPhase += dt * 3.0f;
        if (pk.collected) continue;
        if ((pPos - pk.position).length() <= 1.8f) {
            pk.collected = true;
            if (pk.kind == LevelPickup::Kind::Health) {
                player.heal(45.0f, 0.0f);
            } else if (pk.kind == LevelPickup::Kind::Armor) {
                player.heal(0.0f, 50.0f);
            } else {
                weapons.addAmmo(60, 80, 24, 1);
            }
            audio.playSound(SoundId::Reload, 0.7f);
        }
    }

    // Evaluate active objective
    nearInteractable_ = false;
    if (activeObjIdx_ < static_cast<int>(level.objectives.size())) {
        auto& obj = level.objectives[activeObjIdx_];
        bool done = false;
        switch (obj.type) {
            case ObjectiveSpec::Type::EliminateAll: {
                int alive = enemies.getAliveCount();
                if (alive == 0) {
                    obj.progress = 1.0f;
                    done = true;
                }
                break;
            }
            case ObjectiveSpec::Type::ReachZone: {
                float d = (pPos - obj.targetPos).length();
                if (d <= obj.radius) {
                    obj.progress = 1.0f;
                    done = true;
                }
                break;
            }
            case ObjectiveSpec::Type::HackTerminal: {
                float d = (pPos - obj.targetPos).length();
                if (d <= obj.radius) {
                    nearInteractable_ = true;
                    // Auto-progress when holding position or pressing Interact boosts speed
                    float rate = interactPressed ? 1.8f : 0.65f;
                    obj.progress = std::min(1.0f, obj.progress + (dt * rate) / std::max(0.5f, obj.holdTime));
                    if (obj.progress >= 1.0f) {
                        done = true;
                    }
                }
                break;
            }
        }

        if (done) {
            obj.completed = true;
            ++activeObjIdx_;
            audio.playSound(SoundId::ObjectiveComplete, 0.9f);
            if (activeObjIdx_ >= static_cast<int>(level.objectives.size())) {
                state_ = MissionState::MissionComplete;
            }
        }
    }
}

const std::string& MissionManager::getCurrentObjectiveText() const {
    return fallbackText_;
}

float MissionManager::getCurrentObjectiveProgress() const {
    return 0.0f;
}

} // namespace ts
