#pragma once

#include "Math3D.h"
#include "TextureManager.h"
#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ts {

constexpr int MAX_SKIN_BONES = 32;

struct SkinnedVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float tx, ty, tz, tw;
    uint8_t j0, j1, j2, j3;
    float w0, w1, w2, w3;
};

struct PbrMaterial {
    Vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.1f;
    float roughness = 0.7f;
    Vec3 emissive{0.0f, 0.0f, 0.0f};
    GLuint albedoTex = 0;
    GLuint normalTex = 0;
};

struct MeshPrimitive {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    uint32_t indexCount = 0;
    PbrMaterial material;
    AABB localBounds;
    bool isSkinned = false;

    void destroy();
    void upload(const std::vector<SkinnedVertex>& vertices,
                const std::vector<uint16_t>& indices);
};

struct GltfNode {
    std::string name;
    int parent = -1;
    std::vector<int> children;
    Vec3 translation{0, 0, 0};
    Quat rotation{0, 0, 0, 1};
    Vec3 scale{1, 1, 1};
    int meshIndex = -1;
    int skinIndex = -1;
};

struct AnimSampler {
    std::vector<float> times;
    std::vector<Vec4> values; // vec3 stored in xyz, quat in xyzw
};

struct AnimChannel {
    enum class Path { Translation, Rotation, Scale };
    int targetNode = -1;
    Path path = Path::Translation;
    AnimSampler sampler;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimChannel> channels;
};

struct SkinData {
    std::vector<int> joints;
    std::vector<Mat4> inverseBindMatrices;
};

struct ModelAsset {
    std::string path;
    std::vector<MeshPrimitive> primitives;
    std::vector<int> primitiveNodeIndex;
    std::vector<GltfNode> nodes;
    std::vector<int> rootNodes;
    std::vector<SkinData> skins;
    std::vector<AnimationClip> animations;
    AABB bounds;
    bool hasSkin = false;

    ~ModelAsset();
};

class ModelLoader {
public:
    std::shared_ptr<ModelAsset> loadGltf(AAssetManager* mgr,
                                         TextureManager& texMgr,
                                         const std::string& gltfPath);

    static MeshPrimitive createBoxPrimitive(const Vec3& halfExtents,
                                            float uvScale,
                                            const PbrMaterial& mat);
};

} // namespace ts
