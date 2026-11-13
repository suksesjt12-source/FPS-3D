#pragma once

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ts {

enum class SoundId {
    RifleShot = 0,
    SmgShot,
    PistolShot,
    Reload,
    Explosion,
    Hitmarker,
    Footstep,
    DamageHit,
    ObjectiveComplete
};

struct PcmClip {
    std::vector<int16_t> samples;
    uint32_t sampleRate = 22050;
};

class AudioSystem {
public:
    ~AudioSystem();

    bool init(AAssetManager* mgr);
    void shutdown();
    void playSound(SoundId id, float volume = 1.0f);
    void setMuted(bool muted);

private:
    bool loadWavAsset(AAssetManager* mgr, SoundId id, const std::string& path);
    void ensureVoicePool();

    struct Voice {
        SLObjectItf playerObj = nullptr;
        SLPlayItf playItf = nullptr;
        SLAndroidSimpleBufferQueueItf queueItf = nullptr;
        SLVolumeItf volumeItf = nullptr;
        std::vector<int16_t> activeBuffer;
        bool busy = false;
    };

    SLObjectItf engineObj_ = nullptr;
    SLEngineItf engineItf_ = nullptr;
    SLObjectItf outputMixObj_ = nullptr;
    std::vector<Voice> voices_;
    std::unordered_map<int, PcmClip> clips_;
    std::mutex mutex_;
    bool initialized_ = false;
    bool muted_ = false;

    static void SLAPIENTRY bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void* context);
};

} // namespace ts
