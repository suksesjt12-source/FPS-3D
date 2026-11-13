#include "AnimationSystem.h"
#include <algorithm>
#include <cmath>

namespace ts {

namespace {

const AnimationClip* findClip(const ModelAsset& model, const std::string& name) {
    for (const auto& c : model.animations) {
        if (c.name == name) return &c;
    }
    return model.animations.empty() ? nullptr : &model.animations[0];
}

Vec4 sampleSampler(const AnimSampler& s, float t, bool isRotation) {
    if (s.times.empty() || s.values.empty()) return {0, 0, 0, 1};
    if (s.times.size() == 1 || t <= s.times.front()) return s.values.front();
    if (t >= s.times.back()) return s.values.back();

    size_t hi = 1;
    while (hi < s.times.size() && s.times[hi] < t) ++hi;
    size_t lo = hi - 1;
    float dt = s.times[hi] - s.times[lo];
    float alpha = dt > 1e-6f ? (t - s.times[lo]) / dt : 0.0f;

    const Vec4& a = s.values[lo];
    const Vec4& b = s.values[hi];
    if (isRotation) {
        Quat q = Quat::slerp(Quat(a.x, a.y, a.z, a.w), Quat(b.x, b.y, b.z, b.w), alpha);
        return {q.x, q.y, q.z, q.w};
    }
    return {
        lerpf(a.x, b.x, alpha),
        lerpf(a.y, b.y, alpha),
        lerpf(a.z, b.z, alpha),
        lerpf(a.w, b.w, alpha)
    };
}

void sampleClipPose(const ModelAsset& model,
                    const AnimationClip* clip,
                    float timeSec,
                    bool loop,
                    std::vector<Vec3>& trans,
                    std::vector<Quat>& rots,
                    std::vector<Vec3>& scales) {
    size_t n = model.nodes.size();
    trans.resize(n);
    rots.resize(n);
    scales.resize(n);
    for (size_t i = 0; i < n; ++i) {
        trans[i] = model.nodes[i].translation;
        rots[i] = model.nodes[i].rotation;
        scales[i] = model.nodes[i].scale;
    }
    if (!clip || clip->duration <= 1e-5f) return;

    float t = loop ? std::fmod(std::max(0.0f, timeSec), clip->duration)
                   : clampf(timeSec, 0.0f, clip->duration);

    for (const auto& ch : clip->channels) {
        if (ch.targetNode < 0 || ch.targetNode >= static_cast<int>(n)) continue;
        bool isRot = (ch.path == AnimChannel::Path::Rotation);
        Vec4 val = sampleSampler(ch.sampler, t, isRot);
        if (ch.path == AnimChannel::Path::Translation) {
            trans[ch.targetNode] = {val.x, val.y, val.z};
        } else if (ch.path == AnimChannel::Path::Rotation) {
            rots[ch.targetNode] = Quat(val.x, val.y, val.z, val.w).normalized();
        } else {
            scales[ch.targetNode] = {val.x, val.y, val.z};
        }
    }
}

void computeSkinMatrices(const ModelAsset& model,
                         const std::vector<Vec3>& trans,
                         const std::vector<Quat>& rots,
                         const std::vector<Vec3>& scales,
                         SkeletalPose& outPose) {
    size_t n = model.nodes.size();
    if (n == 0) return;
    std::vector<Mat4> globalMats(n, Mat4::identity());

    auto computeNode = [&](auto& self, int idx, const Mat4& parentMat) -> void {
        Mat4 local = Mat4::composeTRS(trans[idx], rots[idx], scales[idx]);
        globalMats[idx] = parentMat * local;
        for (int child : model.nodes[idx].children) {
            self(self, child, globalMats[idx]);
        }
    };

    for (int root : model.rootNodes) {
        computeNode(computeNode, root, Mat4::identity());
    }

    if (!model.skins.empty()) {
        const auto& skin = model.skins[0];
        size_t jCount = std::min<size_t>(skin.joints.size(), MAX_SKIN_BONES);
        for (size_t i = 0; i < jCount; ++i) {
            int nodeIdx = skin.joints[i];
            if (nodeIdx >= 0 && nodeIdx < static_cast<int>(n)) {
                outPose.boneMatrices[i] = globalMats[nodeIdx] * skin.inverseBindMatrices[i];
            }
        }
    }
}

} // namespace

void AnimationSystem::evaluateClip(const ModelAsset& model,
                                   const std::string& clipName,
                                   float timeSec,
                                   bool loop,
                                   SkeletalPose& outPose) const {
    const AnimationClip* clip = findClip(model, clipName);
    std::vector<Vec3> trans, scales;
    std::vector<Quat> rots;
    sampleClipPose(model, clip, timeSec, loop, trans, rots, scales);
    computeSkinMatrices(model, trans, rots, scales, outPose);
}

void AnimationSystem::evaluateBlended(const ModelAsset& model,
                                      const std::string& clipA,
                                      float timeA,
                                      bool loopA,
                                      const std::string& clipB,
                                      float timeB,
                                      bool loopB,
                                      float blendWeight,
                                      SkeletalPose& outPose) const {
    float w = clampf(blendWeight, 0.0f, 1.0f);
    if (w <= 0.001f) {
        evaluateClip(model, clipA, timeA, loopA, outPose);
        return;
    }
    if (w >= 0.999f) {
        evaluateClip(model, clipB, timeB, loopB, outPose);
        return;
    }

    const AnimationClip* cA = findClip(model, clipA);
    const AnimationClip* cB = findClip(model, clipB);
    std::vector<Vec3> tA, sA, tB, sB;
    std::vector<Quat> rA, rB;
    sampleClipPose(model, cA, timeA, loopA, tA, rA, sA);
    sampleClipPose(model, cB, timeB, loopB, tB, rB, sB);

    size_t n = model.nodes.size();
    for (size_t i = 0; i < n; ++i) {
        tA[i] = lerp(tA[i], tB[i], w);
        rA[i] = Quat::slerp(rA[i], rB[i], w);
        sA[i] = lerp(sA[i], sB[i], w);
    }
    computeSkinMatrices(model, tA, rA, sA, outPose);
}

} // namespace ts
