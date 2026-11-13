#pragma once

#include "Math3D.h"
#include "ModelLoader.h"
#include <array>
#include <string>
#include <vector>

namespace ts {

struct SkeletalPose {
    std::array<Mat4, MAX_SKIN_BONES> boneMatrices;

    SkeletalPose() {
        for (auto& m : boneMatrices) m = Mat4::identity();
    }
};

class AnimationSystem {
public:
    void evaluateClip(const ModelAsset& model,
                      const std::string& clipName,
                      float timeSec,
                      bool loop,
                      SkeletalPose& outPose) const;

    void evaluateBlended(const ModelAsset& model,
                         const std::string& clipA,
                         float timeA,
                         bool loopA,
                         const std::string& clipB,
                         float timeB,
                         bool loopB,
                         float blendWeight,
                         SkeletalPose& outPose) const;
};

} // namespace ts
