/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "../common/modelviewer.h"
#include "../common/ui.h"
#include "culling.h"
#include <filesystem>

#if PH_ANDROID
    #include <sys/system_properties.h>
#endif

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct WarScene : ModelViewer {
    struct Options : ModelViewer::Options {
        std::string model;
        std::string animation;
        Options() {
            rpmode                       = RenderPackMode::NOISE_FREE;
            shadowMode                   = ShadowMode::RAY_TRACED;
            flythroughCamera             = true;
            showFrameTimes               = true;
            reflectionMapAsset           = "model/war-scenario/skybox-reflection.ktx2";
            irradianceMapAsset           = "model/war-scenario/skybox-irradiance.ktx2";
            refractionAndRoughReflection = false;
        }
    };

    WarScene(SimpleApp & app, const Options & o): ModelViewer(app, o) {
        // determine path to the model
        auto modelPath = std::filesystem::path(o.model);
        if (modelPath.empty()) {
            modelPath = "model/war-scenario/livedemo.gltf";
        } else if (std::filesystem::is_directory(modelPath)) {
            auto gltf = searchForGLTF(modelPath);
            if (gltf.empty()) PH_THROW("No GLTF/GLB model found in folder: %s", modelPath.string().c_str());
            modelPath = gltf;
        }

        // preload all files in the model's folder
        auto folder = std::filesystem::path(modelPath).parent_path().string();
        assetSys->preloadFolder(folder.c_str());

        // load the model
        scene->name = modelPath.string();
        _bbox       = addModelToScene({modelPath.string()});

        std::vector<ph::rt::Material *> materialList  = world->materials();
        size_t                          materialCount = materialList.size();

        for (size_t i = 0; i < materialCount; i++) {
            Material *     material = materialList[i];
            Material::Desc mcp      = material->desc();
            mcp.setIor(0.0f);
            material->setDesc(mcp);
        }

        // setup camera
        setCamera(o.animated);

        // setup culling parameters
        _cullingManager.setActiveAlgorithm(3); // set to war-zone's special culling algorithm.
        _cullingManager.cullingDistance() = 0.75f;

        // Setup light bounding box (The directional light shadow map rendering code needs it to calculate light projection matrix)
        if (lights.size() > 0) {
            auto l = lights[0]->light();
            auto d = l->desc();
            if (d.type == Light::DIRECTIONAL) {
                d.directional.setBBox(_bbox.min(), _bbox.max());
                l->reset(d);
            }
            l->shadowMapBias      = 0.004f;
            l->shadowMapSlopeBias = 0.001f;
        }

        // reset skybox
        addSkybox(0.0f);
    }

    void resized() override {
        ModelViewer::resized();

        // set default record parameters
        recordParameters.ambientLight                                        = {20.f / 255.f, 20.f / 255.f, 17.f / 255.f};
        recordParameters.transparencySettings.shadowSettings.tshadowAlpha    = true;
        recordParameters.transparencySettings.shadowSettings.tshadowTextured = true;
        recordParameters.transparencySettings.alphaCutoff                    = 0.99f;
        recordParameters.transparencySettings.alphaMaxHit                    = 2;
        recordParameters.skyboxLighting                                      = 0;
        recordParameters.maxSpecularBounces                                  = 1;
        recordParameters.skyboxRotation = 2.105f; // this is to align the sun direction in skybox texture to the directional light.
        setupShadowRenderPack();
    }

    void update() override {
        ModelViewer::update();
        if (_cullingManager.activeAlgorithm() != 0) {
            _cullingManager.setCamera(&cameras[selectedCameraIndex], ((float) sw().initParameters().width), ((float) sw().initParameters().height));
            _cullingManager.setGraph(graph);
            _cullingManager.update();
        }
    }

    void describeImguiUI() override {
        ModelViewer::describeImguiUI();
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("WarZone")) {
            size_t n = _cullingManager.numAlgorithms();
            if (ImGui::BeginListBox("", ImVec2(0, (float) n * ImGui::GetTextLineHeightWithSpacing()))) {
                for (size_t i = 0; i < n; ++i) {
                    auto & a = _cullingManager.algorithm(i);
                    if (ImGui::Selectable(a.name.c_str(), _cullingManager.activeAlgorithm() == i)) {
                        _cullingManager.setActiveAlgorithm(i);
                        if (0 == i) {
                            // A hack to force update to set all nodes visible when culling is disabled.
                            _cullingManager.update();
                        }
                    }
                }
                ImGui::EndListBox();
            }
            ImGui::SliderFloat("Distance Cutoff", &_cullingManager.cullingDistance(), 0.1f, 4.0f);
            ImGui::SliderFloat("camera Zfar", &cameras[0].zFar, 0.1f, 4.0f);
            if (ImGui::BeginTable("", (int) ph::rt::render::NoiseFreeRenderPack::ShadowMode::NUM_SHADOW_MODES)) {
                ImGui::TableNextColumn();
                if (ImGui::Button("POI #0")) teleportToPOI(0);
                ImGui::TableNextColumn();
                if (ImGui::Button("POI #1")) teleportToPOI(1);
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    void teleportToPOI(int index) {
        switch (index) {
        case 0:
            firstPersonController.setPosition({0.1512f, 0.06f, 0.2251f});
            firstPersonController.setAngle({-0.044f, -1.2127f, 0.f});
            break;
        case 1:
            firstPersonController.setPosition({-0.57f, 0.06f, -1.982f});
            firstPersonController.setAngle({-0.4426f, 1.1246f, 0.f});
            break;
        }
    }

    void setCamera(bool cameraAnimation) {
        setupDefaultCamera(_bbox);
        cameras[0].zNear = .01f;
        cameras[0].zFar  = 4.0f;
        // limit camera at ground level.
        firstPersonController.setFlythroughPositionBoundary({
            Eigen::Vector3f(-6.f, 0.06f, -6.f),
            Eigen::Vector3f(6.f, 0.06f, 6.f),
        });
        firstPersonController.setMoveSpeed(_bbox.diagonal().norm() / 15.0f);

        // if imported scene has a camera, switch to it
        if (cameraAnimation && cameras.size() > 1) { setPrimaryCamera(1); }

        teleportToPOI(0);
    }

private:
    Eigen::AlignedBox3f _bbox; ///< bounding box of the scene
    CullingManager      _cullingManager;
};
