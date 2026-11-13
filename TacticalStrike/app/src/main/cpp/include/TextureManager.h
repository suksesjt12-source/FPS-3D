#pragma once

#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace ts {

class TextureManager {
public:
    ~TextureManager();

    void initDefaults();
    GLuint loadKtxTexture(AAssetManager* mgr, const std::string& path);
    GLuint getTexture(const std::string& key) const;
    GLuint getDefaultWhite() const { return defaultWhite_; }
    GLuint getDefaultNormal() const { return defaultNormal_; }
    void clear();

private:
    GLuint createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    std::unordered_map<std::string, GLuint> textures_;
    GLuint defaultWhite_ = 0;
    GLuint defaultNormal_ = 0;
};

} // namespace ts
