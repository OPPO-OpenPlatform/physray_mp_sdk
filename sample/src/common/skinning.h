#pragma once
#include <ph/rt.h>

namespace skinning {
enum SkinningMode {
    OFF,
    CPU,
};
struct SkinningData {
    // Indexed by joint id
    std::vector<ph::rt::Node *>  jointMatrices;
    std::vector<Eigen::Matrix4f> inverseBindMatrices;

    // Per-vertex data
    std::vector<uint32_t> joints;
    std::vector<float>    weights;
    std::vector<float>    origPositions;
    std::vector<float>    origNormals;
};

typedef std::map<ph::rt::Mesh * const, SkinningData> SkinMap;

struct SkinningManager {
private:
    SkinningMode _sm;
    SkinMap      _skinnedMeshes;
    void         applyCpuSkinning() const;

public:
    SkinningManager(const SkinningMode & sm): _sm(sm) {}
    ~SkinningManager() {}

    void      update(bool animated) const;
    void      describeImguiUI(SkinningMode & settableSkinModeOption);
    SkinMap * getSkinDataMap();
};
} // namespace skinning