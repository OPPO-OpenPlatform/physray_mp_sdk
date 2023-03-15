/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : carousel.h
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

#include <ph/rt-utils.h>
#if !defined(__ANDROID__)
    #include <GLFW/glfw3.h>
#endif

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct CarouselScene : ModelViewer {
    struct Options : ModelViewer::Options {
        bool     cluster = true;
        uint32_t restirM = 32;
#if defined(__ANDROID__)
        bool outputVideo = true;
#else
        bool outputVideo = false;
#endif
        Options() {
            leftHanded = true;
#if defined(__ANDROID__)
            rpmode = RenderPackMode::FAST_PT;
            accum  = 128;
#else
            rpmode = RenderPackMode::PATH_TRACING;
            accum  = 256;
#endif
        }
    };

    // store all animations and loop through them one by one
    std::vector<std::shared_ptr<::animations::Timeline>> allAnimations;
    int                                                  i                    = 0;
    int                                                  numAnims             = 7;
    int                                                  animationIds[7]      = {1, 2, 3, 4, 6, 7, 8};
    int                                                  animatedCameraIds[7] = {2, 3, 4, 5, 7, 8, 9};
    int                                                  activeAnimation      = -1;
    bool                                                 finishedVideo        = false;
    void                                                 playAnimation() {
        if (i >= 0 && i < 7) {
            animations.clear();
            activeAnimation       = animationIds[i];
            auto & activeTimeline = allAnimations[activeAnimation];
            activeTimeline->playFromStart();
            animations.push_back(activeTimeline);
            if (activeAnimation < (int) cameras.size()) setPrimaryCamera(animatedCameraIds[i]);
        }
    }
    bool videoComplete() { return finishedVideo; }
    void updateAnimations() {
        if (!_options.outputVideo) return;
        if ((activeAnimation < 0) || (activeAnimation > (int) allAnimations.size())) {
            allAnimations = animations;
            if ((allAnimations.size() >= 1) && (numAnims >= 1)) { playAnimation(); }
        } else {
            if (allAnimations[activeAnimation]->getPlayCount() > 0) {
                if (activeAnimation == numAnims - 1) finishedVideo = true;
                i = (i + 1) % numAnims;
                playAnimation();
            }
        }
    }

    CarouselScene(SimpleApp & app, const Options & o): ModelViewer(app, o), _options(o) {
        options.usePrecompiledShaderParameters = true; // this is needed to achieve real-time runtime on mobile. on desktop, this can be toggled in the UI.
        ptConfig.initialCandidateCount         = _options.restirM;
        ptConfig.restirMode                    = _options.restirM > 0 ? PathTracerConfig::ReSTIRMode::InitialCandidates : PathTracerConfig::ReSTIRMode::Off;

        // skybox
        recordParameters.irradianceMap = {textureCache->loadFromAsset("texture/dikhololo/dikhololo_diffuse.ktx2")};
        recordParameters.reflectionMap = {textureCache->loadFromAsset("texture/dikhololo/dikhololo_reflection.ktx2")};
        // use transparent shadows to prevent lights from self-shadowing.
        // todo: cull masks to mask out lights during anyHit shader.
        recordParameters.transparencySettings.shadowSettings.tshadowAlpha = true;
        bool                useLowPoly                                    = _options.rpmode == Options::RenderPackMode::FAST_PT;
        std::string         scenePath = useLowPoly ? "model/carousel/mobile/carousel.gltf" : "model/carousel/desktop/carousel.gltf";
        Eigen::AlignedBox3f bbox      = addModelToScene({scenePath, "*", nullptr, nullptr, !useLowPoly});
        if (useLowPoly) {
            for (auto & l : lights) {
                if (l) {
                    auto ld         = l->desc();
                    ld.dimension[0] = ld.dimension[1] = 0.025f;
                    // for some reason, sphere lights come out way darker than the corresponding mesh lights.
                    // this is probably to do with the fact that we don't actually consider visibility of
                    // an arbitrary mesh when computing the pdf.
                    // we just work around this by pumping up the area light brightness by a lot.
                    ld.emission[0] = ld.emission[1] = ld.emission[2] = 150.f;
                    ld.range                                         = bbox.diagonal().norm();
                    ld.allowShadow                                   = true;
                    l->reset(ld);
                    if (debugManager) debugManager->updateDebugLight(l);
                }
            }
        }

        // Disable spatial clusters until bugs are resolved
        // setClusterMode(_options.cluster ? PathTracerConfig::ClusterMode::Enabled : PathTracerConfig::ClusterMode::Disabled);
        // setClusterSubdivisions(128);
        setupDefaultCamera(bbox);
        if ((int) cameras.size() > 1) {
            auto sceneCamNode = cameras[1].node;
            cameras[1]        = cameras[0];   // configure imported camera based on default camera settings
            cameras[1].node   = sceneCamNode; // reset scene camera to original node
            setPrimaryCamera(1);              // switch to scene camera
        }
        setupShadowRenderPack();
    }

    void drawUI() override {
#if defined(__ANDROID__)
        ImGui::SetNextWindowPos(ImVec2(20, 20));
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::Begin("ReSTIR Mode Toggle", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
        if (ImGui::Button(ptConfig.restirMode == PathTracerConfig::ReSTIRMode::InitialCandidates ? "ReSTIR" : "Temporal ReSTIR")) {
            ptConfig.restirMode = ptConfig.restirMode == PathTracerConfig::ReSTIRMode::InitialCandidates ? PathTracerConfig::ReSTIRMode::TemporalReuse
                                                                                                         : PathTracerConfig::ReSTIRMode::InitialCandidates;
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(380, 20));
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::Begin("Pause Button", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
        if (ImGui::Button(animated() ? "Pause" : "Resume")) { toggleAnimated(); }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(700, 20));
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.2f);
        ImGui::Begin("ReSTIR Off", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("ReSTIR Off");
        ImGui::End();

#else
        ModelViewer::drawUI();
#endif
    }

    void update() override {
        updateAnimations();
        ModelViewer::update();
    }

private:
    const Options _options;
};
