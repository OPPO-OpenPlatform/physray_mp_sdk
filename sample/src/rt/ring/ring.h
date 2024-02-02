/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "../common/modelviewer.h"
#include "../common/ui.h"
#include <filesystem>

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct OppoRingScene : ModelViewer {
    struct Options : ModelViewer::Options {
        Options() {
            rpmode             = RenderPackMode::NOISE_FREE;
            animated           = true;
            flythroughCamera   = false;
            showFrameTimes     = false;
            specBounces        = 2;
            irradianceMapAsset = "texture/skybox2/irradiance-astc-12x12.ktx2";
            reflectionMapAsset = "texture/skybox2/prefiltered-reflection-astc-4x4.ktx2";
        }
    };

    OppoRingScene(SimpleApp & app, const Options & o): ModelViewer(app, o) {
        auto bbox = addModelToScene({"model/oppo-ring.glb"});
        setupDefaultCamera(bbox);
        addSkybox(0.f);
        // move camera closer to the scene.
        firstPersonController.setOrbitalRadius(bbox.diagonal().norm() / 1.5f);
        // Rotate camera to a certain angle to avoid animated rings overlapping on top of the "OPPO" text.
        firstPersonController.setAngle({-.2f, -.8f, 0.f});
        // use animation camera by default.
        setPrimaryCamera(1);
    }

    void describeImguiUI() override {
        ModelViewer::describeImguiUI();
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Oppo Ring")) {
            bool animated_ = animated();
            if (ImGui::Checkbox("animated", &animated_)) { setAnimated(animated_); }

            bool cameraAnimation = 1 == selectedCameraIndex;
            if (ImGui::Checkbox("camera animation", &cameraAnimation)) { setPrimaryCamera(cameraAnimation ? 1 : 0); }

            ImGui::SliderInt("Max Specular Bounces", (int *) &recordParameters.maxSpecularBounces, 0, 4);

            ImGui::Checkbox("Show heat view", &recordParameters.enableHeatMap);

            ImGui::TreePop();
        }
    }
};
