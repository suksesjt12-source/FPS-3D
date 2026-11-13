#include "ModelLoader.h"
#include "AssetLoader.h"
#include "JsonMini.h"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace ts {

namespace {

std::vector<uint8_t> decodeBase64(const std::string& in) {
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string parentDir(const std::string& path) {
    size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? "" : path.substr(0, pos + 1);
}

int numComponentsForType(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT4") return 16;
    return 0;
}

int componentByteSize(int compType) {
    switch (compType) {
        case 5120: // BYTE
        case 5121: // UNSIGNED_BYTE
            return 1;
        case 5122: // SHORT
        case 5123: // UNSIGNED_SHORT
            return 2;
        case 5125: // UNSIGNED_INT
        case 5126: // FLOAT
            return 4;
        default:
            return 0;
    }
}

struct AccessorView {
    const uint8_t* data = nullptr;
    size_t stride = 0;
    size_t count = 0;
    int compType = 0;
    int numComps = 0;
};

bool resolveAccessor(const JsonValue& root,
                     const std::vector<std::vector<uint8_t>>& buffers,
                     int accessorIdx,
                     AccessorView& out) {
    const auto& accessors = root.get("accessors").arrVal;
    const auto& bufferViews = root.get("bufferViews").arrVal;
    if (accessorIdx < 0 || accessorIdx >= static_cast<int>(accessors.size())) return false;
    const auto& acc = accessors[accessorIdx];
    int bvIdx = acc.get("bufferView").asInt(-1);
    if (bvIdx < 0 || bvIdx >= static_cast<int>(bufferViews.size())) return false;
    const auto& bv = bufferViews[bvIdx];
    int bufIdx = bv.get("buffer").asInt(-1);
    if (bufIdx < 0 || bufIdx >= static_cast<int>(buffers.size())) return false;

    out.compType = acc.get("componentType").asInt(0);
    out.numComps = numComponentsForType(acc.get("type").asString());
    out.count = static_cast<size_t>(acc.get("count").asInt(0));
    size_t bvOffset = static_cast<size_t>(bv.get("byteOffset").asInt(0));
    size_t accOffset = static_cast<size_t>(acc.get("byteOffset").asInt(0));
    size_t defaultStride = static_cast<size_t>(out.numComps * componentByteSize(out.compType));
    out.stride = static_cast<size_t>(bv.get("byteStride").asInt(static_cast<int>(defaultStride)));
    if (out.stride == 0) out.stride = defaultStride;

    size_t start = bvOffset + accOffset;
    if (start >= buffers[bufIdx].size()) return false;
    out.data = buffers[bufIdx].data() + start;
    return true;
}

float readScalarAsFloat(const uint8_t* ptr, int compType) {
    switch (compType) {
        case 5126: {
            float f = 0.0f;
            std::memcpy(&f, ptr, 4);
            return f;
        }
        case 5121: return static_cast<float>(*ptr) / 255.0f;
        case 5123: {
            uint16_t u = 0;
            std::memcpy(&u, ptr, 2);
            return static_cast<float>(u) / 65535.0f;
        }
        default: return 0.0f;
    }
}

uint32_t readIndex(const uint8_t* ptr, int compType) {
    if (compType == 5123) {
        uint16_t u = 0;
        std::memcpy(&u, ptr, 2);
        return u;
    }
    if (compType == 5125) {
        uint32_t u = 0;
        std::memcpy(&u, ptr, 4);
        return u;
    }
    if (compType == 5121) {
        return *ptr;
    }
    return 0;
}

uint8_t readJointIndex(const uint8_t* ptr, int compType) {
    if (compType == 5121) return *ptr;
    if (compType == 5123) {
        uint16_t u = 0;
        std::memcpy(&u, ptr, 2);
        return static_cast<uint8_t>(std::min<uint16_t>(u, MAX_SKIN_BONES - 1));
    }
    return 0;
}

void computeTangents(std::vector<SkinnedVertex>& verts, const std::vector<uint16_t>& indices) {
    std::vector<Vec3> tan1(verts.size(), {0, 0, 0});
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint16_t i0 = indices[i];
        uint16_t i1 = indices[i + 1];
        uint16_t i2 = indices[i + 2];
        if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) continue;

        const auto& v0 = verts[i0];
        const auto& v1 = verts[i1];
        const auto& v2 = verts[i2];

        Vec3 p0{v0.px, v0.py, v0.pz};
        Vec3 p1{v1.px, v1.py, v1.pz};
        Vec3 p2{v2.px, v2.py, v2.pz};
        Vec3 e1 = p1 - p0;
        Vec3 e2 = p2 - p0;
        float du1 = v1.u - v0.u;
        float dv1 = v1.v - v0.v;
        float du2 = v2.u - v0.u;
        float dv2 = v2.v - v0.v;

        float denom = du1 * dv2 - du2 * dv1;
        float r = std::fabs(denom) > 1e-6f ? 1.0f / denom : 0.0f;
        Vec3 sdir = (e1 * dv2 - e2 * dv1) * r;
        tan1[i0] += sdir;
        tan1[i1] += sdir;
        tan1[i2] += sdir;
    }

    for (size_t i = 0; i < verts.size(); ++i) {
        Vec3 n{verts[i].nx, verts[i].ny, verts[i].nz};
        Vec3 t = tan1[i];
        Vec3 ortho = (t - n * n.dot(t)).normalized();
        if (ortho.lengthSq() < 1e-5f) {
            Vec3 up = std::fabs(n.y) < 0.9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
            ortho = up.cross(n).normalized();
        }
        verts[i].tx = ortho.x;
        verts[i].ty = ortho.y;
        verts[i].tz = ortho.z;
        verts[i].tw = 1.0f;
    }
}

} // namespace

void MeshPrimitive::destroy() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ibo) { glDeleteBuffers(1, &ibo); ibo = 0; }
    indexCount = 0;
}

void MeshPrimitive::upload(const std::vector<SkinnedVertex>& vertices,
                           const std::vector<uint16_t>& indices) {
    destroy();
    indexCount = static_cast<uint32_t>(indices.size());
    if (vertices.empty() || indices.empty()) return;

    localBounds.min = {1e9f, 1e9f, 1e9f};
    localBounds.max = {-1e9f, -1e9f, -1e9f};
    for (const auto& v : vertices) {
        localBounds.min = minVec3(localBounds.min, {v.px, v.py, v.pz});
        localBounds.max = maxVec3(localBounds.max, {v.px, v.py, v.pz});
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(SkinnedVertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint16_t)),
                 indices.data(),
                 GL_STATIC_DRAW);

    constexpr GLsizei stride = sizeof(SkinnedVertex);
    // 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, px)));
    // 1: normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, nx)));
    // 2: uv (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, u)));
    // 3: tangent (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, tx)));
    // 4: joints (uvec4 / ivec4)
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_BYTE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, j0)));
    // 5: weights (vec4)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SkinnedVertex, w0)));

    glBindVertexArray(0);
}

ModelAsset::~ModelAsset() {
    for (auto& prim : primitives) {
        prim.destroy();
    }
}

std::shared_ptr<ModelAsset> ModelLoader::loadGltf(AAssetManager* mgr,
                                                TextureManager& texMgr,
                                                const std::string& gltfPath) {
    std::string text;
    if (!AssetLoader::readText(mgr, gltfPath, text)) return nullptr;

    JsonValue root;
    if (!JsonValue::parse(text, root) || !root.isObject()) {
        TS_LOGE("ModelLoader: failed to parse glTF JSON: %s", gltfPath.c_str());
        return nullptr;
    }

    auto asset = std::make_shared<ModelAsset>();
    asset->path = gltfPath;
    std::string baseDir = parentDir(gltfPath);

    // Load binary buffers
    std::vector<std::vector<uint8_t>> buffers;
    const auto& bufArr = root.get("buffers").arrVal;
    buffers.resize(bufArr.size());
    for (size_t i = 0; i < bufArr.size(); ++i) {
        const std::string& uri = bufArr[i].get("uri").asString();
        if (uri.rfind("data:", 0) == 0) {
            size_t comma = uri.find(',');
            if (comma != std::string::npos) {
                buffers[i] = decodeBase64(uri.substr(comma + 1));
            }
        } else if (!uri.empty()) {
            AssetLoader::readBinary(mgr, baseDir + uri, buffers[i]);
        }
    }

    // Load materials
    std::vector<PbrMaterial> materials;
    const auto& images = root.get("images").arrVal;
    const auto& textures = root.get("textures").arrVal;
    const auto& mats = root.get("materials").arrVal;

    auto resolveTex = [&](int texIdx, GLuint fallback) -> GLuint {
        if (texIdx < 0 || texIdx >= static_cast<int>(textures.size())) return fallback;
        int imgIdx = textures[texIdx].get("source").asInt(-1);
        if (imgIdx < 0 || imgIdx >= static_cast<int>(images.size())) return fallback;
        std::string uri = images[imgIdx].get("uri").asString();
        if (uri.empty()) return fallback;
        // Support relative paths from model dir or assets root
        if (uri.rfind("textures/", 0) == 0) {
            return texMgr.loadKtxTexture(mgr, uri);
        }
        return texMgr.loadKtxTexture(mgr, baseDir + uri);
    };

    for (const auto& jm : mats) {
        PbrMaterial mat;
        mat.albedoTex = texMgr.getDefaultWhite();
        mat.normalTex = texMgr.getDefaultNormal();

        const auto& pbr = jm.get("pbrMetallicRoughness");
        if (pbr.isObject()) {
            const auto& bc = pbr.get("baseColorFactor").arrVal;
            if (bc.size() == 4) {
                mat.baseColor = {bc[0].asFloat(1), bc[1].asFloat(1), bc[2].asFloat(1), bc[3].asFloat(1)};
            }
            mat.metallic = pbr.get("metallicFactor").asFloat(0.2f);
            mat.roughness = pbr.get("roughnessFactor").asFloat(0.65f);
            if (pbr.has("baseColorTexture")) {
                int idx = pbr.get("baseColorTexture").get("index").asInt(-1);
                mat.albedoTex = resolveTex(idx, texMgr.getDefaultWhite());
            }
        }
        if (jm.has("normalTexture")) {
            int idx = jm.get("normalTexture").get("index").asInt(-1);
            mat.normalTex = resolveTex(idx, texMgr.getDefaultNormal());
        }
        const auto& em = jm.get("emissiveFactor").arrVal;
        if (em.size() == 3) {
            mat.emissive = {em[0].asFloat(0), em[1].asFloat(0), em[2].asFloat(0)};
        }
        materials.push_back(mat);
    }

    // Load nodes
    const auto& jnodes = root.get("nodes").arrVal;
    asset->nodes.resize(jnodes.size());
    for (size_t i = 0; i < jnodes.size(); ++i) {
        const auto& jn = jnodes[i];
        auto& n = asset->nodes[i];
        n.name = jn.get("name").asString();
        n.meshIndex = jn.get("mesh").asInt(-1);
        n.skinIndex = jn.get("skin").asInt(-1);
        const auto& t = jn.get("translation").arrVal;
        if (t.size() == 3) n.translation = {t[0].asFloat(), t[1].asFloat(), t[2].asFloat()};
        const auto& r = jn.get("rotation").arrVal;
        if (r.size() == 4) n.rotation = Quat(r[0].asFloat(), r[1].asFloat(), r[2].asFloat(), r[3].asFloat()).normalized();
        const auto& s = jn.get("scale").arrVal;
        if (s.size() == 3) n.scale = {s[0].asFloat(1), s[1].asFloat(1), s[2].asFloat(1)};
        for (const auto& ch : jn.get("children").arrVal) {
            int childIdx = ch.asInt(-1);
            if (childIdx >= 0 && childIdx < static_cast<int>(jnodes.size())) {
                n.children.push_back(childIdx);
                asset->nodes[childIdx].parent = static_cast<int>(i);
            }
        }
    }
    for (size_t i = 0; i < asset->nodes.size(); ++i) {
        if (asset->nodes[i].parent == -1) {
            asset->rootNodes.push_back(static_cast<int>(i));
        }
    }

    // Load skins
    const auto& jskins = root.get("skins").arrVal;
    for (const auto& js : jskins) {
        SkinData skin;
        for (const auto& jnt : js.get("joints").arrVal) {
            skin.joints.push_back(jnt.asInt(-1));
        }
        int ibmIdx = js.get("inverseBindMatrices").asInt(-1);
        AccessorView ibmView;
        if (ibmIdx >= 0 && resolveAccessor(root, buffers, ibmIdx, ibmView)) {
            skin.inverseBindMatrices.resize(ibmView.count);
            for (size_t k = 0; k < ibmView.count; ++k) {
                std::memcpy(skin.inverseBindMatrices[k].m, ibmView.data + k * ibmView.stride, 64);
            }
        } else {
            skin.inverseBindMatrices.assign(skin.joints.size(), Mat4::identity());
        }
        asset->skins.push_back(std::move(skin));
        asset->hasSkin = true;
    }

    // Load meshes
    const auto& jmeshes = root.get("meshes").arrVal;
    asset->bounds.min = {1e9f, 1e9f, 1e9f};
    asset->bounds.max = {-1e9f, -1e9f, -1e9f};

    for (size_t mIdx = 0; mIdx < jmeshes.size(); ++mIdx) {
        int ownerNode = -1;
        for (size_t ni = 0; ni < asset->nodes.size(); ++ni) {
            if (asset->nodes[ni].meshIndex == static_cast<int>(mIdx)) {
                ownerNode = static_cast<int>(ni);
                break;
            }
        }
        for (const auto& jp : jmeshes[mIdx].get("primitives").arrVal) {
            const auto& attrs = jp.get("attributes");
            AccessorView posV, normV, uvV, tanV, jntV, wgtV, idxV;
            if (!resolveAccessor(root, buffers, attrs.get("POSITION").asInt(-1), posV)) continue;
            bool hasNorm = resolveAccessor(root, buffers, attrs.get("NORMAL").asInt(-1), normV);
            bool hasUv = resolveAccessor(root, buffers, attrs.get("TEXCOORD_0").asInt(-1), uvV);
            bool hasTan = resolveAccessor(root, buffers, attrs.get("TANGENT").asInt(-1), tanV);
            bool hasJnt = resolveAccessor(root, buffers, attrs.get("JOINTS_0").asInt(-1), jntV);
            bool hasWgt = resolveAccessor(root, buffers, attrs.get("WEIGHTS_0").asInt(-1), wgtV);
            bool hasIdx = resolveAccessor(root, buffers, jp.get("indices").asInt(-1), idxV);

            std::vector<SkinnedVertex> verts(posV.count);
            for (size_t v = 0; v < posV.count; ++v) {
                SkinnedVertex sv{};
                const float* p = reinterpret_cast<const float*>(posV.data + v * posV.stride);
                sv.px = p[0]; sv.py = p[1]; sv.pz = p[2];

                if (hasNorm) {
                    const float* n = reinterpret_cast<const float*>(normV.data + v * normV.stride);
                    sv.nx = n[0]; sv.ny = n[1]; sv.nz = n[2];
                } else {
                    sv.ny = 1.0f;
                }
                if (hasUv) {
                    const float* uv = reinterpret_cast<const float*>(uvV.data + v * uvV.stride);
                    sv.u = uv[0]; sv.v = uv[1];
                }
                if (hasTan) {
                    const float* t = reinterpret_cast<const float*>(tanV.data + v * tanV.stride);
                    sv.tx = t[0]; sv.ty = t[1]; sv.tz = t[2]; sv.tw = t[3];
                }
                if (hasJnt && hasWgt) {
                    const uint8_t* jpPtr = jntV.data + v * jntV.stride;
                    int jSz = componentByteSize(jntV.compType);
                    sv.j0 = readJointIndex(jpPtr + 0 * jSz, jntV.compType);
                    sv.j1 = readJointIndex(jpPtr + 1 * jSz, jntV.compType);
                    sv.j2 = readJointIndex(jpPtr + 2 * jSz, jntV.compType);
                    sv.j3 = readJointIndex(jpPtr + 3 * jSz, jntV.compType);

                    const uint8_t* wpPtr = wgtV.data + v * wgtV.stride;
                    int wSz = componentByteSize(wgtV.compType);
                    sv.w0 = readScalarAsFloat(wpPtr + 0 * wSz, wgtV.compType);
                    sv.w1 = readScalarAsFloat(wpPtr + 1 * wSz, wgtV.compType);
                    sv.w2 = readScalarAsFloat(wpPtr + 2 * wSz, wgtV.compType);
                    sv.w3 = readScalarAsFloat(wpPtr + 3 * wSz, wgtV.compType);
                } else {
                    sv.w0 = 1.0f;
                }
                verts[v] = sv;
            }

            std::vector<uint16_t> indices;
            if (hasIdx) {
                indices.resize(idxV.count);
                for (size_t k = 0; k < idxV.count; ++k) {
                    indices[k] = static_cast<uint16_t>(readIndex(idxV.data + k * idxV.stride, idxV.compType));
                }
            } else {
                indices.resize(verts.size());
                for (size_t k = 0; k < verts.size(); ++k) indices[k] = static_cast<uint16_t>(k);
            }

            if (!hasTan) {
                computeTangents(verts, indices);
            }

            MeshPrimitive prim;
            int matIdx = jp.get("material").asInt(-1);
            if (matIdx >= 0 && matIdx < static_cast<int>(materials.size())) {
                prim.material = materials[matIdx];
            } else {
                prim.material.albedoTex = texMgr.getDefaultWhite();
                prim.material.normalTex = texMgr.getDefaultNormal();
            }
            prim.isSkinned = hasJnt && hasWgt;
            prim.upload(verts, indices);

            asset->bounds.min = minVec3(asset->bounds.min, prim.localBounds.min);
            asset->bounds.max = maxVec3(asset->bounds.max, prim.localBounds.max);
            asset->primitives.push_back(std::move(prim));
            asset->primitiveNodeIndex.push_back(ownerNode);
        }
    }

    // Load animations
    for (const auto& ja : root.get("animations").arrVal) {
        AnimationClip clip;
        clip.name = ja.get("name").asString();
        const auto& jsamplers = ja.get("samplers").arrVal;
        for (const auto& jc : ja.get("channels").arrVal) {
            int sIdx = jc.get("sampler").asInt(-1);
            const auto& tgt = jc.get("target");
            int nodeIdx = tgt.get("node").asInt(-1);
            const std::string& pathStr = tgt.get("path").asString();
            if (sIdx < 0 || sIdx >= static_cast<int>(jsamplers.size()) || nodeIdx < 0) continue;

            const auto& jsamp = jsamplers[sIdx];
            AccessorView inV, outV;
            if (!resolveAccessor(root, buffers, jsamp.get("input").asInt(-1), inV)) continue;
            if (!resolveAccessor(root, buffers, jsamp.get("output").asInt(-1), outV)) continue;

            AnimChannel ch;
            ch.targetNode = nodeIdx;
            if (pathStr == "rotation") ch.path = AnimChannel::Path::Rotation;
            else if (pathStr == "scale") ch.path = AnimChannel::Path::Scale;
            else ch.path = AnimChannel::Path::Translation;

            ch.sampler.times.resize(inV.count);
            ch.sampler.values.resize(inV.count);
            for (size_t k = 0; k < inV.count; ++k) {
                float t = 0.0f;
                std::memcpy(&t, inV.data + k * inV.stride, 4);
                ch.sampler.times[k] = t;
                clip.duration = std::max(clip.duration, t);

                const uint8_t* op = outV.data + k * outV.stride;
                Vec4 val{0, 0, 0, 1};
                if (outV.numComps >= 3) {
                    std::memcpy(&val.x, op, 12);
                }
                if (outV.numComps == 4) {
                    std::memcpy(&val.w, op + 12, 4);
                }
                ch.sampler.values[k] = val;
            }
            clip.channels.push_back(std::move(ch));
        }
        if (!clip.channels.empty()) {
            asset->animations.push_back(std::move(clip));
        }
    }

    TS_LOGI("ModelLoader: loaded %s (prims=%zu, nodes=%zu, skins=%zu, anims=%zu)",
            gltfPath.c_str(), asset->primitives.size(), asset->nodes.size(),
            asset->skins.size(), asset->animations.size());
    return asset;
}

MeshPrimitive ModelLoader::createBoxPrimitive(const Vec3& h, float uvScale, const PbrMaterial& mat) {
    struct FaceDef {
        Vec3 n;
        Vec3 t;
        Vec3 p0, p1, p2, p3;
        float w, h;
    };
    const float sx = h.x, sy = h.y, sz = h.z;
    const FaceDef faces[6] = {
        {{0, 0, 1},  {1, 0, 0},  {-sx, -sy,  sz}, { sx, -sy,  sz}, { sx,  sy,  sz}, {-sx,  sy,  sz}, sx * 2, sy * 2},
        {{0, 0, -1}, {-1, 0, 0}, { sx, -sy, -sz}, {-sx, -sy, -sz}, {-sx,  sy, -sz}, { sx,  sy, -sz}, sx * 2, sy * 2},
        {{1, 0, 0},  {0, 0, -1}, { sx, -sy,  sz}, { sx, -sy, -sz}, { sx,  sy, -sz}, { sx,  sy,  sz}, sz * 2, sy * 2},
        {{-1, 0, 0}, {0, 0, 1},  {-sx, -sy, -sz}, {-sx, -sy,  sz}, {-sx,  sy,  sz}, {-sx,  sy, -sz}, sz * 2, sy * 2},
        {{0, 1, 0},  {1, 0, 0},  {-sx,  sy,  sz}, { sx,  sy,  sz}, { sx,  sy, -sz}, {-sx,  sy, -sz}, sx * 2, sz * 2},
        {{0, -1, 0}, {1, 0, 0},  {-sx, -sy, -sz}, { sx, -sy, -sz}, { sx, -sy,  sz}, {-sx, -sy,  sz}, sx * 2, sz * 2}
    };

    std::vector<SkinnedVertex> verts;
    std::vector<uint16_t> indices;
    verts.reserve(24);
    indices.reserve(36);

    for (int f = 0; f < 6; ++f) {
        const auto& fd = faces[f];
        uint16_t base = static_cast<uint16_t>(verts.size());
        float uMax = fd.w * uvScale;
        float vMax = fd.h * uvScale;
        Vec3 pts[4] = {fd.p0, fd.p1, fd.p2, fd.p3};
        Vec2 uvs[4] = {{0, 0}, {uMax, 0}, {uMax, vMax}, {0, vMax}};
        for (int k = 0; k < 4; ++k) {
            SkinnedVertex sv{};
            sv.px = pts[k].x; sv.py = pts[k].y; sv.pz = pts[k].z;
            sv.nx = fd.n.x; sv.ny = fd.n.y; sv.nz = fd.n.z;
            sv.u = uvs[k].x; sv.v = uvs[k].y;
            sv.tx = fd.t.x; sv.ty = fd.t.y; sv.tz = fd.t.z; sv.tw = 1.0f;
            sv.w0 = 1.0f;
            verts.push_back(sv);
        }
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    }

    MeshPrimitive prim;
    prim.material = mat;
    prim.upload(verts, indices);
    return prim;
}

} // namespace ts
