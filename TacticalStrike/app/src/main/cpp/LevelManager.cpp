#include "LevelManager.h"
#include "AssetLoader.h"
#include "JsonMini.h"
#include <algorithm>

namespace ts {

void LevelManager::clear() {
    for (auto& b : level_.staticBlocks) {
        b.mesh.destroy();
    }
    level_ = LevelData{};
}

void LevelManager::clearAll() {
    clear();
    enemyModel_.reset();
    for (auto& w : weaponModels_) w.reset();
    grenadeModel_.reset();
    propCrate_.reset();
    propBarrier_.reset();
    propTerminal_.reset();
}

bool LevelManager::loadMission(int missionIndex,
                               AAssetManager* mgr,
                               TextureManager& texMgr,
                               ModelLoader& modelLoader,
                               CollisionSystem& collision,
                               EnemySystem& enemies,
                               LightingSystem& lighting) {
    clear();
    collision.clear();
    enemies.clear();
    lighting.resetLights();

    // Preload core character, weapon, and prop glTF models once
    if (!enemyModel_) enemyModel_ = modelLoader.loadGltf(mgr, texMgr, "models/enemy_trooper.gltf");
    if (!weaponModels_[0]) weaponModels_[0] = modelLoader.loadGltf(mgr, texMgr, "models/weapon_m4a1.gltf");
    if (!weaponModels_[1]) weaponModels_[1] = modelLoader.loadGltf(mgr, texMgr, "models/weapon_mp7.gltf");
    if (!weaponModels_[2]) weaponModels_[2] = modelLoader.loadGltf(mgr, texMgr, "models/weapon_usp45.gltf");
    if (!grenadeModel_) grenadeModel_ = modelLoader.loadGltf(mgr, texMgr, "models/weapon_grenade.gltf");
    if (!propCrate_) propCrate_ = modelLoader.loadGltf(mgr, texMgr, "models/prop_crate.gltf");
    if (!propBarrier_) propBarrier_ = modelLoader.loadGltf(mgr, texMgr, "models/prop_barrier.gltf");
    if (!propTerminal_) propTerminal_ = modelLoader.loadGltf(mgr, texMgr, "models/prop_terminal.gltf");

    const char* missionFiles[3] = {
        "levels/mission_01_outpost.json",
        "levels/mission_02_reactor.json",
        "levels/mission_03_extraction.json"
    };
    int clampedIdx = std::max(0, std::min(2, missionIndex));
    std::string jsonText;
    if (!AssetLoader::readText(mgr, missionFiles[clampedIdx], jsonText)) {
        TS_LOGE("LevelManager: missing level file %s", missionFiles[clampedIdx]);
        return false;
    }

    JsonValue root;
    if (!JsonValue::parse(jsonText, root) || !root.isObject()) {
        TS_LOGE("LevelManager: invalid JSON in %s", missionFiles[clampedIdx]);
        return false;
    }

    level_.missionIndex = clampedIdx;
    level_.missionName = root.get("name").asString();
    level_.briefing = root.get("briefing").asString();

    const auto& sp = root.get("playerSpawn").arrVal;
    if (sp.size() >= 3) {
        level_.playerSpawn = {sp[0].asFloat(), sp[1].asFloat(), sp[2].asFloat()};
    }
    level_.playerSpawnYaw = root.get("playerYaw").asFloat(0.0f);

    // Sky & lighting
    const auto& skyTop = root.get("skyTop").arrVal;
    if (skyTop.size() == 3) level_.skyTopColor = {skyTop[0].asFloat(), skyTop[1].asFloat(), skyTop[2].asFloat()};
    const auto& skyHor = root.get("skyHorizon").arrVal;
    if (skyHor.size() == 3) level_.skyHorizonColor = {skyHor[0].asFloat(), skyHor[1].asFloat(), skyHor[2].asFloat()};

    DirectionalLight dl;
    const auto& sunDir = root.get("sunDirection").arrVal;
    if (sunDir.size() == 3) dl.direction = Vec3{sunDir[0].asFloat(), sunDir[1].asFloat(), sunDir[2].asFloat()}.normalized();
    const auto& sunCol = root.get("sunColor").arrVal;
    if (sunCol.size() == 3) dl.color = {sunCol[0].asFloat(), sunCol[1].asFloat(), sunCol[2].asFloat()};
    dl.intensity = root.get("sunIntensity").asFloat(2.2f);
    const auto& ambCol = root.get("ambientColor").arrVal;
    if (ambCol.size() == 3) dl.ambientColor = {ambCol[0].asFloat(), ambCol[1].asFloat(), ambCol[2].asFloat()};
    lighting.setDirectionalLight(dl);

    FogSettings fog;
    const auto& fogCol = root.get("fogColor").arrVal;
    if (fogCol.size() == 3) fog.color = {fogCol[0].asFloat(), fogCol[1].asFloat(), fogCol[2].asFloat()};
    fog.density = root.get("fogDensity").asFloat(0.014f);
    lighting.setFog(fog);

    for (const auto& jl : root.get("pointLights").arrVal) {
        const auto& pos = jl.get("pos").arrVal;
        const auto& col = jl.get("color").arrVal;
        if (pos.size() == 3 && col.size() == 3) {
            lighting.addPersistentPointLight(
                {pos[0].asFloat(), pos[1].asFloat(), pos[2].asFloat()},
                {col[0].asFloat(), col[1].asFloat(), col[2].asFloat()},
                jl.get("intensity").asFloat(3.0f),
                jl.get("radius").asFloat(10.0f)
            );
        }
    }

    // Load material textures
    GLuint concreteAlb = texMgr.loadKtxTexture(mgr, "textures/concrete_albedo.ktx");
    GLuint concreteNorm = texMgr.loadKtxTexture(mgr, "textures/concrete_normal.ktx");
    GLuint metalAlb = texMgr.loadKtxTexture(mgr, "textures/metal_albedo.ktx");
    GLuint metalNorm = texMgr.loadKtxTexture(mgr, "textures/metal_normal.ktx");
    GLuint groundAlb = texMgr.loadKtxTexture(mgr, "textures/ground_albedo.ktx");
    GLuint groundNorm = texMgr.loadKtxTexture(mgr, "textures/ground_normal.ktx");
    GLuint crateAlb = texMgr.loadKtxTexture(mgr, "textures/crate_albedo.ktx");
    GLuint crateNorm = texMgr.loadKtxTexture(mgr, "textures/crate_normal.ktx");

    // Static architecture & colliders
    for (const auto& jb : root.get("blocks").arrVal) {
        const auto& center = jb.get("center").arrVal;
        const auto& half = jb.get("half").arrVal;
        if (center.size() != 3 || half.size() != 3) continue;

        Vec3 c{center[0].asFloat(), center[1].asFloat(), center[2].asFloat()};
        Vec3 h{half[0].asFloat(), half[1].asFloat(), half[2].asFloat()};
        std::string matName = jb.get("material").asString();

        PbrMaterial mat;
        int matTag = 0;
        if (matName == "metal") {
            mat.albedoTex = metalAlb;
            mat.normalTex = metalNorm;
            mat.metallic = 0.72f;
            mat.roughness = 0.35f;
            matTag = 1;
        } else if (matName == "ground") {
            mat.albedoTex = groundAlb;
            mat.normalTex = groundNorm;
            mat.metallic = 0.05f;
            mat.roughness = 0.88f;
            matTag = 0;
        } else if (matName == "crate") {
            mat.albedoTex = crateAlb;
            mat.normalTex = crateNorm;
            mat.metallic = 0.15f;
            mat.roughness = 0.68f;
            matTag = 2;
        } else {
            mat.albedoTex = concreteAlb;
            mat.normalTex = concreteNorm;
            mat.metallic = 0.08f;
            mat.roughness = 0.82f;
            matTag = 0;
        }

        const auto& tint = jb.get("tint").arrVal;
        if (tint.size() >= 3) {
            mat.baseColor = {tint[0].asFloat(1), tint[1].asFloat(1), tint[2].asFloat(1), 1.0f};
        }

        LevelStaticBlock blk;
        blk.mesh = ModelLoader::createBoxPrimitive(h, jb.get("uvScale").asFloat(0.25f), mat);
        blk.transform = Mat4::translate(c);
        blk.worldBounds = {c - h, c + h};
        blk.lodCullDistance = jb.get("cullDist").asFloat(150.0f);
        level_.staticBlocks.push_back(std::move(blk));

        bool solid = jb.get("solid").asBool(true);
        if (solid) {
            collision.addStaticBox({c - h, c + h}, true, true, matTag);
        }
    }

    // glTF Props
    for (const auto& jp : root.get("props").arrVal) {
        std::string kind = jp.get("kind").asString();
        const auto& pos = jp.get("pos").arrVal;
        if (pos.size() != 3) continue;
        Vec3 p{pos[0].asFloat(), pos[1].asFloat(), pos[2].asFloat()};
        float rotY = jp.get("yaw").asFloat(0.0f);

        std::shared_ptr<ModelAsset> mdl = propCrate_;
        Vec3 colHalf{0.55f, 0.55f, 0.55f};
        int matTag = 2;
        if (kind == "barrier") {
            mdl = propBarrier_;
            colHalf = {1.15f, 0.55f, 0.35f};
            matTag = 0;
        } else if (kind == "terminal") {
            mdl = propTerminal_;
            colHalf = {0.45f, 0.85f, 0.45f};
            matTag = 1;
        }
        if (!mdl) continue;

        LevelPropInstance inst;
        inst.model = mdl;
        inst.transform = Mat4::translate(p) * Mat4::rotateY(rotY);
        inst.worldBounds = {
            {p.x - colHalf.x, p.y, p.z - colHalf.z},
            {p.x + colHalf.x, p.y + colHalf.y * 2.0f, p.z + colHalf.z}
        };
        level_.props.push_back(inst);
        collision.addStaticBox(inst.worldBounds, true, true, matTag);
    }

    // Checkpoints
    int cpIdx = 0;
    for (const auto& jc : root.get("checkpoints").arrVal) {
        const auto& pos = jc.get("pos").arrVal;
        if (pos.size() == 3) {
            level_.checkpoints.push_back({
                cpIdx++,
                {pos[0].asFloat(), pos[1].asFloat(), pos[2].asFloat()},
                jc.get("radius").asFloat(3.5f),
                false
            });
        }
    }

    // Pickups
    for (const auto& jpk : root.get("pickups").arrVal) {
        const auto& pos = jpk.get("pos").arrVal;
        if (pos.size() != 3) continue;
        std::string kind = jpk.get("kind").asString();
        LevelPickup pk;
        pk.position = {pos[0].asFloat(), pos[1].asFloat(), pos[2].asFloat()};
        if (kind == "health") pk.kind = LevelPickup::Kind::Health;
        else if (kind == "armor") pk.kind = LevelPickup::Kind::Armor;
        else pk.kind = LevelPickup::Kind::Ammo;
        level_.pickups.push_back(pk);
    }

    // Objectives
    for (const auto& jo : root.get("objectives").arrVal) {
        ObjectiveSpec obj;
        obj.title = jo.get("title").asString();
        std::string typeStr = jo.get("type").asString();
        if (typeStr == "reach") obj.type = ObjectiveSpec::Type::ReachZone;
        else if (typeStr == "hack") obj.type = ObjectiveSpec::Type::HackTerminal;
        else obj.type = ObjectiveSpec::Type::EliminateAll;

        const auto& pos = jo.get("pos").arrVal;
        if (pos.size() == 3) {
            obj.targetPos = {pos[0].asFloat(), pos[1].asFloat(), pos[2].asFloat()};
        }
        obj.radius = jo.get("radius").asFloat(3.5f);
        obj.holdTime = jo.get("holdTime").asFloat(3.0f);
        level_.objectives.push_back(std::move(obj));
    }

    // Enemies
    for (const auto& je : root.get("enemies").arrVal) {
        const auto& pos = je.get("pos").arrVal;
        if (pos.size() != 3) continue;
        std::string typeStr = je.get("type").asString();
        EnemyType et = EnemyType::Rifleman;
        if (typeStr == "scout") et = EnemyType::Scout;
        else if (typeStr == "enforcer") et = EnemyType::Enforcer;

        std::vector<Vec3> patrol;
        for (const auto& wp : je.get("patrol").arrVal) {
            if (wp.arrVal.size() == 3) {
                patrol.push_back({wp.arrVal[0].asFloat(), wp.arrVal[1].asFloat(), wp.arrVal[2].asFloat()});
            }
        }
        enemies.spawnEnemy(et,
                           {pos[0].asFloat(), pos[1].asFloat(), pos[2].asFloat()},
                           je.get("yaw").asFloat(0.0f),
                           patrol);
    }

    TS_LOGI("LevelManager: loaded mission %d '%s' (%zu blocks, %zu props, %d enemies)",
            clampedIdx, level_.missionName.c_str(),
            level_.staticBlocks.size(), level_.props.size(), enemies.getAliveCount());
    return true;
}

} // namespace ts
