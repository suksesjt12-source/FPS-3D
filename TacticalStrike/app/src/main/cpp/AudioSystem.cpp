#include "AudioSystem.h"
#include "AssetLoader.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ts {

AudioSystem::~AudioSystem() {
    shutdown();
}

void SLAPIENTRY AudioSystem::bufferQueueCallback(SLAndroidSimpleBufferQueueItf /*bq*/, void* context) {
    auto* voice = static_cast<Voice*>(context);
    if (voice) {
        voice->busy = false;
    }
}

bool AudioSystem::loadWavAsset(AAssetManager* mgr, SoundId id, const std::string& path) {
    std::vector<uint8_t> raw;
    if (!AssetLoader::readBinary(mgr, path, raw) || raw.size() < 44) {
        return false;
    }
    if (std::memcmp(raw.data(), "RIFF", 4) != 0 || std::memcmp(raw.data() + 8, "WAVE", 4) != 0) {
        return false;
    }

    uint32_t sampleRate = 22050;
    uint16_t channels = 1;
    uint16_t bitsPerSample = 16;
    size_t pos = 12;
    const uint8_t* dataPtr = nullptr;
    uint32_t dataBytes = 0;

    while (pos + 8 <= raw.size()) {
        uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, raw.data() + pos + 4, 4);
        if (std::memcmp(raw.data() + pos, "fmt ", 4) == 0 && pos + 24 <= raw.size()) {
            std::memcpy(&channels, raw.data() + pos + 10, 2);
            std::memcpy(&sampleRate, raw.data() + pos + 12, 4);
            std::memcpy(&bitsPerSample, raw.data() + pos + 22, 2);
        } else if (std::memcmp(raw.data() + pos, "data", 4) == 0) {
            dataPtr = raw.data() + pos + 8;
            dataBytes = std::min<uint32_t>(chunkSize, static_cast<uint32_t>(raw.size() - (pos + 8)));
            break;
        }
        pos += 8 + chunkSize + (chunkSize & 1u);
    }

    if (!dataPtr || dataBytes < 2 || bitsPerSample != 16) return false;

    PcmClip clip;
    clip.sampleRate = sampleRate;
    size_t frameCount = dataBytes / (2 * std::max<uint16_t>(1, channels));
    clip.samples.resize(frameCount);
    const int16_t* src = reinterpret_cast<const int16_t*>(dataPtr);
    for (size_t i = 0; i < frameCount; ++i) {
        clip.samples[i] = src[i * channels];
    }
    clips_[static_cast<int>(id)] = std::move(clip);
    return true;
}

bool AudioSystem::init(AAssetManager* mgr) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    loadWavAsset(mgr, SoundId::RifleShot, "audio/rifle_shot.wav");
    loadWavAsset(mgr, SoundId::SmgShot, "audio/smg_shot.wav");
    loadWavAsset(mgr, SoundId::PistolShot, "audio/pistol_shot.wav");
    loadWavAsset(mgr, SoundId::Reload, "audio/reload.wav");
    loadWavAsset(mgr, SoundId::Explosion, "audio/explosion.wav");
    loadWavAsset(mgr, SoundId::Hitmarker, "audio/hitmarker.wav");
    loadWavAsset(mgr, SoundId::Footstep, "audio/footstep.wav");
    loadWavAsset(mgr, SoundId::DamageHit, "audio/damage_hit.wav");
    loadWavAsset(mgr, SoundId::ObjectiveComplete, "audio/objective.wav");

    SLresult res = slCreateEngine(&engineObj_, 0, nullptr, 0, nullptr, nullptr);
    if (res != SL_RESULT_SUCCESS) return false;
    if ((*engineObj_)->Realize(engineObj_, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) return false;
    if ((*engineObj_)->GetInterface(engineObj_, SL_IID_ENGINE, &engineItf_) != SL_RESULT_SUCCESS) return false;
    if ((*engineItf_)->CreateOutputMix(engineItf_, &outputMixObj_, 0, nullptr, nullptr) != SL_RESULT_SUCCESS) return false;
    if ((*outputMixObj_)->Realize(outputMixObj_, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) return false;

    ensureVoicePool();
    initialized_ = true;
    TS_LOGI("AudioSystem: OpenSL ES initialized with %zu clips and %zu voices", clips_.size(), voices_.size());
    return true;
}

void AudioSystem::ensureVoicePool() {
    constexpr int VOICE_COUNT = 8;
    voices_.resize(VOICE_COUNT);

    SLDataLocator_AndroidSimpleBufferQueue locBufq{SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM formatPcm{
        SL_DATAFORMAT_PCM,
        1,
        SL_SAMPLINGRATE_22_05,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_CENTER,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource audioSrc{&locBufq, &formatPcm};
    SLDataLocator_OutputMix locOutmix{SL_DATALOCATOR_OUTPUTMIX, outputMixObj_};
    SLDataSink audioSnk{&locOutmix, nullptr};

    const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
    const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    for (auto& v : voices_) {
        if ((*engineItf_)->CreateAudioPlayer(engineItf_, &v.playerObj, &audioSrc, &audioSnk, 2, ids, req) != SL_RESULT_SUCCESS) {
            continue;
        }
        (*v.playerObj)->Realize(v.playerObj, SL_BOOLEAN_FALSE);
        (*v.playerObj)->GetInterface(v.playerObj, SL_IID_PLAY, &v.playItf);
        (*v.playerObj)->GetInterface(v.playerObj, SL_IID_BUFFERQUEUE, &v.queueItf);
        (*v.playerObj)->GetInterface(v.playerObj, SL_IID_VOLUME, &v.volumeItf);
        (*v.queueItf)->RegisterCallback(v.queueItf, bufferQueueCallback, &v);
        (*v.playItf)->SetPlayState(v.playItf, SL_PLAYSTATE_PLAYING);
    }
}

void AudioSystem::playSound(SoundId id, float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || muted_) return;
    auto it = clips_.find(static_cast<int>(id));
    if (it == clips_.end() || it->second.samples.empty()) return;

    Voice* chosen = nullptr;
    for (auto& v : voices_) {
        if (v.queueItf && !v.busy) {
            chosen = &v;
            break;
        }
    }
    if (!chosen) return;

    chosen->busy = true;
    chosen->activeBuffer = it->second.samples;
    if (chosen->volumeItf) {
        float clamped = std::max(0.05f, std::min(1.0f, volume));
        SLmillibel mb = static_cast<SLmillibel>(2000.0f * std::log10(clamped));
        (*chosen->volumeItf)->SetVolumeLevel(chosen->volumeItf, mb);
    }
    (*chosen->queueItf)->Clear(chosen->queueItf);
    (*chosen->queueItf)->Enqueue(chosen->queueItf,
                                 chosen->activeBuffer.data(),
                                 static_cast<SLuint32>(chosen->activeBuffer.size() * sizeof(int16_t)));
}

void AudioSystem::setMuted(bool muted) {
    std::lock_guard<std::mutex> lock(mutex_);
    muted_ = muted;
    for (auto& v : voices_) {
        if (v.volumeItf) {
            (*v.volumeItf)->SetMute(v.volumeItf, muted ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE);
        }
    }
}

void AudioSystem::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& v : voices_) {
        if (v.playerObj) {
            (*v.playerObj)->Destroy(v.playerObj);
            v.playerObj = nullptr;
        }
    }
    voices_.clear();
    if (outputMixObj_) {
        (*outputMixObj_)->Destroy(outputMixObj_);
        outputMixObj_ = nullptr;
    }
    if (engineObj_) {
        (*engineObj_)->Destroy(engineObj_);
        engineObj_ = nullptr;
        engineItf_ = nullptr;
    }
    initialized_ = false;
}

} // namespace ts
