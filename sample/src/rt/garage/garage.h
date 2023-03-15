/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : garage.h
 *
 * Version: 2.0
 *
 * Date : Feb 2023
 *
 * Author: Computing & Graphics Research Institute
 *
 * ------------------ Revision History: ---------------------
 *  <version>  <date>  <author>  <desc>
 *
 *******************************************************************************/

#include "../common/modelviewer.h"
#include "../common/ui.h"
#include <filesystem>

#if PH_ANDROID
    #include <sys/system_properties.h>
#endif

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct GarageScene : ModelViewer {
    struct Options : ModelViewer::Options {
        std::string model;
        Options() {
            rpmode           = RenderPackMode::NOISE_FREE;
            shadowMode       = ShadowMode::REFINED;
            animated         = true;
            flythroughCamera = true;
            showFrameTimes   = true;
        }
    };

    GarageScene(SimpleApp & app, const Options & o): ModelViewer(app, o) {
        // setup recording parameters
        recordParameters.irradianceMap             = {};
        recordParameters.reflectionMap             = {};
        recordParameters.ambientLight              = {.05f, .05f, .05f};
        recordParameters.saturation                = 1.1f;
        recordParameters.reflectionRoughnessCutoff = 0.2f;

        // determine path to the model
        auto path = o.model;
        if (path.empty()) { path = "model/garage/4.0a/Garage_Enviorment.gltf"; }

        // preload all files in the garage asset folder.
        auto folder = std::filesystem::path(path).parent_path().string();
        assetSys->preloadFolder(folder.c_str());

        // load the model
        scene->name = path;
        auto bbox   = addModelToScene({path});

        // setup camera
        setupDefaultCamera(bbox);
        firstPersonController.setFlythroughPositionBoundary({
            Eigen::Vector3f(-130.f, 10.f, -480.f),
            Eigen::Vector3f(390.f, 280.f, 600.f),
        });
        firstPersonController.setPosition({-26.4f, 167.f, 600.f});
        firstPersonController.setAngle({-0.221f, -0.0258f, 0.f});

        // setup the ceiling light
        _light0 = addPointLight(Eigen::Vector3f(125.5f, 177.4f, 58.2f), 750.0f, Eigen::Vector3f(211.f / 255.f, 233.f / 255.f, 255.f / 255.f) * 5.0);
        //_light0 = addDirectionalLight(Eigen::Vector3f(125.5f, bbox.max()[1], 58.2f), Eigen::Vector3f(0.f, -1.f, 0.f));

#if 0
        // Add a 2nd light. But keep it disabled by default.
        float sunColor[3]            = { 255.f/255.f, 215.f/255.f, 155.f/255.f };
        float sunBrightness          = 58.7f;
        float sunRange               = 448.0f;
        _light1 = addPointLight(Eigen::Vector3f(25.f, 500.f, 830.f), sunRange, sunBrightness);
        auto desc = _light1->desc();
        desc.type = ph::rt::Light::OFF;
        _light1->reset(desc);
#endif

        // set default record parameters
        recordParameters.ambientLight              = {28.f / 255.f, 26.f / 255.f, 23.f / 255.f};
        recordParameters.reflectionRoughnessCutoff = 0.196f;

        setupShadowRenderPack();

        // reset skybox
        skybox.reset(); // release old instance
        Skybox::ConstructParameters cp = {loop(), *assetSys};
        cp.width                       = sw().initParameters().width;
        cp.height                      = sw().initParameters().height;
        cp.pass                        = mainColorPass();
        cp.skymap                      = textureCache->loadFromAsset("model/garage/4.0a/skybox-ibl-reflection-astc-12x12.ktx2");
        cp.skymapType                  = Skybox::SkyMapType::CUBE;
        skybox.reset(new Skybox(cp));
    }

    void describeImguiUI() override {
        ModelViewer::describeImguiUI();
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Garage")) {
            bool animated_ = animated();
            if (ImGui::Checkbox("animated", &animated_)) { setAnimated(animated_); }
            if (_light1) {
                bool secondLight = _light1->desc().type > 0;
                if (ImGui::Checkbox("Second Light", &secondLight)) {
                    auto desc = _light1->desc();
                    desc.type = secondLight ? ph::rt::Light::POINT : ph::rt::Light::OFF;
                    _light1->reset(desc);
                }
            }

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::TreeNode("Refl Roughness")) {
                ImGui::SliderFloat("Cutoff", &recordParameters.reflectionRoughnessCutoff, 0.0f, 1.0f);
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }

private:
    ImFont *        _font   = nullptr;
    ph::rt::Light * _light0 = nullptr;
    ph::rt::Light * _light1 = nullptr;
};
