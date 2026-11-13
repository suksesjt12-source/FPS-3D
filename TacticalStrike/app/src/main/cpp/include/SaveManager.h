#pragma once

#include <array>
#include <string>

namespace ts {

struct CampaignProgress {
    int unlockedMission = 0; // 0..2
    int lastPlayedMission = 0;
    int totalKills = 0;
    std::array<float, 3> bestTimesSec{0.0f, 0.0f, 0.0f};
    std::array<bool, 3> missionCompleted{false, false, false};
};

class SaveManager {
public:
    void setStorageDir(const std::string& dir);
    bool load();
    bool save() const;

    CampaignProgress& getProgress() { return progress_; }
    const CampaignProgress& getProgress() const { return progress_; }

    void recordMissionComplete(int missionIndex, float timeSec, int killsEarned);

private:
    std::string filePath_;
    CampaignProgress progress_;
};

} // namespace ts
