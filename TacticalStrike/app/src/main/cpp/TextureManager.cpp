#include "TextureManager.h"
#include "AssetLoader.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace ts {

namespace {
constexpr uint8_t KTX1_MAGIC[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

struct Ktx1Header {
    uint8_t identifier[12];
    uint32_t endianness;
    uint32_t glType;
    uint32_t glTypeSize;
    uint32_t glFormat;
    uint32_t glInternalFormat;
    uint32_t glBaseInternalFormat;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t numberOfArrayElements;
    uint32_t numberOfFaces;
    uint32_t numberOfMipmapLevels;
    uint32_t bytesOfKeyValueData;
};
} // namespace

TextureManager::~TextureManager() {
    clear();
}

void TextureManager::clear() {
    for (auto& kv : textures_) {
        if (kv.second != 0) {
            glDeleteTextures(1, &kv.second);
        }
    }
    textures_.clear();
    if (defaultWhite_) { glDeleteTextures(1, &defaultWhite_); defaultWhite_ = 0; }
    if (defaultNormal_) { glDeleteTextures(1, &defaultNormal_); defaultNormal_ = 0; }
}

GLuint TextureManager::createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    uint8_t pixel[4] = {r, g, b, a};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void TextureManager::initDefaults() {
    if (!defaultWhite_) defaultWhite_ = createSolidTexture(255, 255, 255, 255);
    if (!defaultNormal_) defaultNormal_ = createSolidTexture(128, 128, 255, 255);
}

GLuint TextureManager::loadKtxTexture(AAssetManager* mgr, const std::string& path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) return it->second;

    std::vector<uint8_t> data;
    if (!AssetLoader::readBinary(mgr, path, data)) {
        return defaultWhite_;
    }
    if (data.size() < sizeof(Ktx1Header)) {
        TS_LOGE("TextureManager: file too small for KTX1: %s", path.c_str());
        return defaultWhite_;
    }

    Ktx1Header hdr{};
    std::memcpy(&hdr, data.data(), sizeof(Ktx1Header));
    if (std::memcmp(hdr.identifier, KTX1_MAGIC, 12) != 0) {
        TS_LOGE("TextureManager: invalid KTX1 magic in %s", path.c_str());
        return defaultWhite_;
    }

    size_t offset = sizeof(Ktx1Header) + hdr.bytesOfKeyValueData;
    if (offset > data.size()) {
        TS_LOGE("TextureManager: corrupt KTX1 metadata offset in %s", path.c_str());
        return defaultWhite_;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    uint32_t mipCount = std::max(1u, hdr.numberOfMipmapLevels);
    uint32_t width = std::max(1u, hdr.pixelWidth);
    uint32_t height = std::max(1u, hdr.pixelHeight);
    bool isCompressed = (hdr.glType == 0 && hdr.glFormat == 0);

    uint32_t uploadedMips = 0;
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        if (offset + 4 > data.size()) break;
        uint32_t imageSize = 0;
        std::memcpy(&imageSize, data.data() + offset, 4);
        offset += 4;
        if (offset + imageSize > data.size()) break;

        if (isCompressed) {
            glCompressedTexImage2D(
                GL_TEXTURE_2D,
                static_cast<GLint>(mip),
                static_cast<GLenum>(hdr.glInternalFormat),
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height),
                0,
                static_cast<GLsizei>(imageSize),
                data.data() + offset
            );
        } else {
            glTexImage2D(
                GL_TEXTURE_2D,
                static_cast<GLint>(mip),
                static_cast<GLint>(hdr.glInternalFormat),
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height),
                0,
                static_cast<GLenum>(hdr.glFormat),
                static_cast<GLenum>(hdr.glType),
                data.data() + offset
            );
        }
        ++uploadedMips;

        // KTX1 pads each mip level to 4-byte boundary
        size_t padded = (imageSize + 3u) & ~size_t(3u);
        offset += padded;
        width = std::max(1u, width >> 1);
        height = std::max(1u, height >> 1);
    }

    if (uploadedMips == 1 && !isCompressed) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else if (uploadedMips > 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    textures_[path] = tex;
    TS_LOGI("TextureManager: loaded %s (%ux%u, mips=%u, compressed=%d)",
            path.c_str(), hdr.pixelWidth, hdr.pixelHeight, uploadedMips, isCompressed ? 1 : 0);
    return tex;
}

GLuint TextureManager::getTexture(const std::string& key) const {
    auto it = textures_.find(key);
    return it != textures_.end() ? it->second : defaultWhite_;
}

} // namespace ts
