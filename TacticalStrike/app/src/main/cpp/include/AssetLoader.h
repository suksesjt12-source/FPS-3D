#pragma once

#include <android/asset_manager.h>
#include <android/log.h>
#include <cstdint>
#include <string>
#include <vector>

#define TS_LOG_TAG "TacticalStrikeNative"
#define TS_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TS_LOG_TAG, __VA_ARGS__)
#define TS_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TS_LOG_TAG, __VA_ARGS__)
#define TS_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TS_LOG_TAG, __VA_ARGS__)

namespace ts {

class AssetLoader {
public:
    static bool readText(AAssetManager* mgr, const std::string& path, std::string& out) {
        if (!mgr) return false;
        AAsset* asset = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_BUFFER);
        if (!asset) {
            TS_LOGE("AssetLoader: failed to open text asset: %s", path.c_str());
            return false;
        }
        off_t len = AAsset_getLength(asset);
        out.resize(static_cast<size_t>(len));
        if (len > 0) {
            AAsset_read(asset, &out[0], static_cast<size_t>(len));
        }
        AAsset_close(asset);
        return true;
    }

    static bool readBinary(AAssetManager* mgr, const std::string& path, std::vector<uint8_t>& out) {
        if (!mgr) return false;
        AAsset* asset = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_BUFFER);
        if (!asset) {
            TS_LOGE("AssetLoader: failed to open binary asset: %s", path.c_str());
            return false;
        }
        off_t len = AAsset_getLength(asset);
        out.resize(static_cast<size_t>(len));
        if (len > 0) {
            AAsset_read(asset, out.data(), static_cast<size_t>(len));
        }
        AAsset_close(asset);
        return true;
    }
};

} // namespace ts
