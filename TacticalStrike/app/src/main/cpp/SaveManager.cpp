#include "SaveManager.h"
#include "AssetLoader.h"
#include "JsonMini.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace ts {

void SaveManager::setStorageDir(const std::string& dir) {
    if (!dir.empty() && dir.back() == '/') {
        filePath_ = dir + "tactical_strike_save.json";
    } else {
        filePath_ = dir + "/tactical_strike_save.json";
    }
    load();
}

bool SaveManager::load() {
    if (filePath_.empty()) return false;
    std::ifstream in(filePath_);
    if (!in.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();

    JsonValue root;
    if (!JsonValue::parse(ss.str(), root) || !root.isObject()) {
        return false;
    }

    progress_.unlockedMission = std::max(0, std::min(2, root.get("unlockedMission").asInt(0)));
    progress_.lastPlayedMission = std::max(0, std::min(2, root.get("lastPlayedMission").asInt(0)));
    progress_.totalKills = std::max(0, root.get("totalKills").asInt(0));

    const auto& times = root.get("bestTimesSec").arrVal;
    for (size_t i = 0; i < 3 && i < times.size(); ++i) {
        progress_.bestTimesSec[i] = times[i].asFloat(0.0f);
    }
    const auto& done = root.get("missionCompleted").arrVal;
    for (size_t i = 0; i < 3 && i < done.size(); ++i) {
        progress_.missionCompleted[i] = done[i].asBool(false);
    }
    TS_LOGI("SaveManager: loaded progress from %s (unlocked=%d, kills=%d)",
            filePath_.c_str(), progress_.unlockedMission, progress_.totalKills);
    return true;
}

bool SaveManager::save() const {
    if (filePath_.empty()) return false;
    JsonValue::Object obj;
    obj["unlockedMission"] = JsonValue(progress_.unlockedMission);
    obj["lastPlayedMission"] = JsonValue(progress_.lastPlayedMission);
    obj["totalKills"] = JsonValue(progress_.totalKills);

    JsonValue::Array times;
    JsonValue::Array done;
    for (int i = 0; i < 3; ++i) {
        times.emplace_back(static_cast<double>(progress_.bestTimesSec[i]));
        done.emplace_back(progress_.missionCompleted[i]);
    }
    obj["bestTimesSec"] = JsonValue(std::move(times));
    obj["missionCompleted"] = JsonValue(std::move(done));

    std::ofstream out(filePath_, std::ios::trunc);
    if (!out.is_open()) {
        TS_LOGE("SaveManager: failed to open %s for writing", filePath_.c_str());
        return false;
    }
    out << JsonValue(std::move(obj)).dump();
    return out.good();
}

void SaveManager::recordMissionComplete(int missionIndex, float timeSec, int killsEarned) {
    if (missionIndex < 0 || missionIndex >= 3) return;
    progress_.missionCompleted[missionIndex] = true;
    progress_.lastPlayedMission = missionIndex;
    progress_.totalKills += std::max(0, killsEarned);
    if (progress_.bestTimesSec[missionIndex] <= 0.0f || timeSec < progress_.bestTimesSec[missionIndex]) {
        progress_.bestTimesSec[missionIndex] = timeSec;
    }
    if (missionIndex + 1 < 3) {
        progress_.unlockedMission = std::max(progress_.unlockedMission, missionIndex + 1);
    }
    save();
}

} // namespace ts
