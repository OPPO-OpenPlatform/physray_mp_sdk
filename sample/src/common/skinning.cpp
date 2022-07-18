#include "pch.h"
#include "skinning.h"
#include "ui.h" // for imgui

using namespace skinning;
SkinMap * SkinningManager::getSkinDataMap() {
    return &_skinnedMeshes; // always return this; even when skinning is disabled, we need to load
    // skinning data from the asset if it exists just in case we want to turn on skinning later.
}

void SkinningManager::applyCpuSkinning() const {
    for (const auto & [mesh, skinnedMesh] : _skinnedMeshes) {
        const auto jointNodes = skinnedMesh.jointMatrices;

        // Check for joint transform changes
        bool skinChange = false;
        if (jointNodes.size() > 0) {
            for (int i = 0; i < int(jointNodes.size()); i++) {
                if (jointNodes[i]->worldTransformDirty()) {
                    skinChange = true;
                    break;
                }
            }
        }

        if (skinChange) {
            const auto                   invBindMats = skinnedMesh.inverseBindMatrices;
            std::vector<Eigen::Vector3f> newPositions;
            std::vector<Eigen::Vector3f> newNormals;

            // Update position and normal based on skin weights
            // From https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_020_Skins.md
            // TODO: do this in the scene asset builder mesh view creation instead of here?
            int vtxCount = int(skinnedMesh.origPositions.size()) / 3;
            for (int j = 0; j < vtxCount; ++j) {
                Eigen::Matrix4f skinMat    = Eigen::Matrix4f::Zero();
                int             vec4offset = j * 4;
                for (int i = 0; i < 4; i++) {
                    int joint = int(skinnedMesh.joints[vec4offset + i]);
                    PH_REQUIRE(joint < int(jointNodes.size()));
                    float weight = skinnedMesh.weights[vec4offset + i];
                    skinMat += weight * (jointNodes[joint]->worldTransform() * invBindMats[joint]);
                }
                int             vec3offset = j * 3;
                Eigen::Vector3f vpos       = Eigen::Vector3f(skinnedMesh.origPositions[vec3offset], skinnedMesh.origPositions[vec3offset + 1],
                                                       skinnedMesh.origPositions[vec3offset + 2]);
                Eigen::Vector3f vnorm =
                    Eigen::Vector3f(skinnedMesh.origNormals[vec3offset], skinnedMesh.origNormals[vec3offset + 1], skinnedMesh.origNormals[vec3offset + 2]);

                Eigen::Vector4f skinnedPos = (skinMat * Eigen::Vector4f(vpos.x(), vpos.y(), vpos.z(), 1.0f));
                newPositions.push_back(Eigen::Vector3f(skinnedPos.x(), skinnedPos.y(), skinnedPos.z()));

                Eigen::Vector4f skinnedNorm = (skinMat * Eigen::Vector4f(vnorm.x(), vnorm.y(), vnorm.z(), 0.0f));
                newNormals.push_back(Eigen::Vector3f(skinnedNorm.x(), skinnedNorm.y(), skinnedNorm.z()));
            }

            mesh->morph({
                .positions = {(const float *) newPositions.data(), sizeof(newPositions[0])},
                .normals   = {(const float *) newNormals.data(), sizeof(newNormals[0])},
            });
        }
    }
}

void SkinningManager::describeImguiUI(SkinningMode & settableSkinModeOption) {
    if (ImGui::TreeNode("Skinning Mode")) {
        if (ImGui::BeginListBox("", ImVec2(0, 4 * ImGui::GetTextLineHeightWithSpacing()))) {
            if (ImGui::Selectable("Off", _sm == SkinningMode::OFF)) {
                _sm = settableSkinModeOption = SkinningMode::OFF;
                settableSkinModeOption       = SkinningMode::OFF;
            }
            if (ImGui::Selectable("CPU", _sm == SkinningMode::CPU)) { _sm = settableSkinModeOption = SkinningMode::CPU; }
            ImGui::EndListBox();
        }
        ImGui::TreePop();
    }
}

void SkinningManager::update(bool animated) const {
    if (animated && (_sm == SkinningMode::CPU)) applyCpuSkinning();
}