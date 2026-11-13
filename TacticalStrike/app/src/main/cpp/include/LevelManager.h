#pragma once

#include "CollisionSystem.h"
#include "EnemySystem.h"
#include "LightingSystem.h"
#include "Math3D.h"
#include "ModelLoader.h"
#include <algorithm>
#include <android/asset_manager.h>
#include <memory>
#include <string>
#include <vector>

namespace ts {

struct LevelStaticBlock {
    MeshPrimitive mesh;
    Mat4 transform;
    AABB worldBounds;
    float lodCullDistance = 140.0f;
};

struct LevelPropInstance {
    std::shared_ptr<ModelAsset> model;
    std::shared_ptr<ModelAsset> lodModel;
    Mat4 transform;
    AABB worldBounds;
    float lodDistance = 32.0f;
};

struct LevelCheckpoint {
    int index = 0;
    Vec3 position{0, 0, 0};
    float radius = 3.2f;
    bool reached = false;
};

struct LevelPickup {
    enum class Kind { Health, Armor, Ammo };
    Kind kind = Kind::Ammo;
    Vec3 position{0, 0, 0};
    bool collected = false;
    float bobPhase = 0.0f;
};

struct ObjectiveSpec {
    enum class Type { EliminateAll, ReachZone, HackTerminal };
    Type type = Type::EliminateAll;
    std::string title;
    Vec3 targetPos{0, 0, 0};
    float radius = 3.5f;
    float holdTime = 0.0f;
    float progress = 0.0f;
    bool completed = false;
};

struct LevelData {
    int missionIndex = 0;
    std::string missionName;
    std::string briefing;
    Vec3 playerSpawn{0, 0, 0};
    float playerSpawnYaw = 0.0f;
    Vec3 skyTopColor{0.06f, 0.10f, 0.18f};
    Vec3 skyHorizonColor{0.22f, 0.28f, 0.36f};

    std::vector<LevelStaticBlock> staticBlocks;
    std::vector<LevelPropInstance> props;
    std::vector<LevelCheckpoint> checkpoints;
    std::vector<LevelPickup> pickups;
    std::vector<ObjectiveSpec> objectives;
};

class LevelManager {
public:
    bool loadMission(int missionIndex,
                     AAssetManager* mgr,
                     TextureManager& texMgr,
                     ModelLoader& modelLoader,
                     CollisionSystem& collision,
                     EnemySystem& enemies,
                     LightingSystem& lighting);

    void clear();
    void clearAll();
    LevelData& getCurrentLevel() { return level_; }
    const LevelData& getCurrentLevel() const { return level_; }
    int getMissionCount() const { return 3; }

    std::shared_ptr<ModelAsset> getEnemyModel(EnemyType type = EnemyType::Rifleman) const {
        switch (type) {
            case EnemyType::Scout: return enemyScoutModel_ ? enemyScoutModel_ : enemyModel_;
            case EnemyType::Enforcer: return enemyEnforcerModel_ ? enemyEnforcerModel_ : enemyModel_;
            case EnemyType::Rifleman:
            default: return enemyModel_;
        }
    }
    std::shared_ptr<ModelAsset> getWeaponModel(int slot) const {
        if (slot < 0 || slot >= 4) return weaponModels_[0];
        return weaponModels_[slot];
    }
    std::shared_ptr<ModelAsset> getArmsModel() const { return viewmodelArmsModel_; }
    GLuint getWeaponSkinTex(int skinIndex) const {
        int idx = std::max(0, std::min(3, skinIndex));
        return weaponSkinTex_[idx];
    }
    std::shared_ptr<ModelAsset> getGrenadeModel() const { return grenadeModel_; }

private:
    LevelData level_;
    std::shared_ptr<ModelAsset> enemyModel_;
    std::shared_ptr<ModelAsset> enemyScoutModel_;
    std::shared_ptr<ModelAsset> enemyEnforcerModel_;
    std::shared_ptr<ModelAsset> weaponModels_[4];
    std::shared_ptr<ModelAsset> viewmodelArmsModel_;
    GLuint weaponSkinTex_[4]{0, 0, 0, 0};
    std::shared_ptr<ModelAsset> grenadeModel_;
    std::shared_ptr<ModelAsset> propCrate_;
    std::shared_ptr<ModelAsset> propBarrier_;
    std::shared_ptr<ModelAsset> propTerminal_;
};

} // namespace ts
