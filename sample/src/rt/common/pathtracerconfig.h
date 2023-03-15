/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : pathtracerconfig.h
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

#include <ph/rt-utils.h>
#include "ui.h"

// Configures path tracer
struct PathTracerConfig {
public:
    struct TransmissiveSSSConfig {
        // Configure transmission and subsurface scattering related material properties
        std::string materialName = "";
        float       opaque       = 1.0f;
        float       scaling      = 1.0f;
        float       albedo[3]    = {1.0f, 1.0f, 1.0f};
        float       ssColor[3]   = {1.0f, 0.0f, 0.0f};
        std::string ssMap        = "";
        bool        isThin       = false;

        void setSubsurfaceMaterial(ph::rt::Scene * scene, std::unique_ptr<TextureCache> & textureCache) {
            auto               mats     = scene->materials();
            ph::rt::Material * material = nullptr;
            for (auto mat : mats) {
                if (mat->name == materialName) {
                    material = mat;
                    break;
                }
            }

            if (material) {
                ph::rt::Material::Desc matdesc = material->desc();
                matdesc.setSSS(scaling);
                matdesc.setAlbedo(albedo[0], albedo[1], albedo[2]);
                matdesc.setEmission(ssColor[0], ssColor[1], ssColor[2]);
                if (!ssMap.empty()) matdesc.setEmissionMap(textureCache->loadFromAsset(ssMap));
                matdesc.setIor(isThin ? 0.f : 1.45f);
                matdesc.setOpaqueness(opaque);

                material->setDesc(matdesc);
            }
        }
    };

    /// used to specify the type of rendering used based on the spatial cluster system
    /// Disabled - Do not allow the use of spatial clustering in path tracer
    /// Enabled  - Allow the use of spatial clustering in the path tracer during renderng
    ///           (only used for light sampling currently)
    /// Debug    - show the scene bounds and the clusters.
    enum class ClusterMode : unsigned {
        Disabled = 0,
        Enabled,
        Debug,
    };

    /// used to specify ReSTIR mode
    /// Initial only resamples from initial M candidates
    /// Temporal adds a round of resampling between initial candidate sample and previous frame's sample
    /// Spatiotemporal does temporal + two rounds of resampling between resampling results from neighboring pixels
    enum class ReSTIRMode : unsigned {
        Off = 0,
        InitialCandidates,
        TemporalReuse,
        SpatiotemporalReuse,
    };

    int         initialCandidateCount;
    float       jitterAmount;
    float       subsurfaceChance;
    float       rmaxScalar;
    float       emissionScalar;
    float       sssamtScalar; // TODO this is the same as subsurfaceconfig::scaling; combine the two.
    float       nChance;
    float       gaussV;
    ClusterMode clusterMode;
    int         sceneSubdivisions;
    ReSTIRMode  restirMode; // ReSTIR Mode
    bool        enableRestirMap;
    uint32_t    misMode;

    // By default, full featureset is enabled
    PathTracerConfig(bool pathTracerEnabled = false) {
        initialCandidateCount = 0;
        misMode               = 0;
        if (pathTracerEnabled) {
            jitterAmount     = 0.0f; // cast primary ray; don't jitter prez camera
            subsurfaceChance = 0.5f; // uniformly sample between subsurface indirect and reflected indirect

        } else {
            jitterAmount     = 0.0f;
            subsurfaceChance = 0.0f;
        }
        rmaxScalar        = 1.f;
        emissionScalar    = 1.f;
        sssamtScalar      = 1.f;
        nChance           = 0.5f;
        gaussV            = 1.f;
        clusterMode       = ClusterMode::Disabled;
        sceneSubdivisions = 1;
        restirMode        = ReSTIRMode::InitialCandidates; // ReSTIR Mode
        enableRestirMap   = false;
    }
    ~PathTracerConfig() = default;

    void setupRp(ph::rt::RayTracingRenderPack::RecordParameters & rp) const {
        rp.initialCandidateCount = initialCandidateCount;
        rp.jitterAmount          = jitterAmount;
        rp.subsurfaceChance      = subsurfaceChance;
        rp.rmaxScalar            = rmaxScalar;
        rp.emissionScalar        = emissionScalar;
        rp.sssamtScalar          = sssamtScalar;
        rp.nChance               = nChance;
        rp.gaussV                = gaussV;
        rp.clusterMode           = static_cast<unsigned>(clusterMode);
        rp.sceneSubdivisions     = static_cast<int>(sceneSubdivisions);
        rp.restirMode            = static_cast<unsigned>(restirMode); // ReSTIR Mode
        rp.enableRestirMap       = enableRestirMap;
        rp.misMode               = misMode;
    }

    void describeImguiUI() {
        if (ImGui::TreeNode("Path Tracer Config")) {
            bool veachMisEnabled = misMode > uint32_t(0);
            if (ImGui::Checkbox("Veach MIS", &veachMisEnabled)) { misMode = veachMisEnabled ? 1 : 0; }
            ImGui::SliderFloat("Pre-Z Camera Jitter (cast primary ray iff zero)", &jitterAmount, 0.f, 3.f);
            if (ImGui::TreeNode("Subsurface Debug")) {
                ImGui::SliderFloat("Subsurface Chance", &subsurfaceChance, 0.f, 1.f);
                ImGui::SliderFloat("rmax Scalar", &rmaxScalar, 0.f, 30.f);
                ImGui::SliderFloat("Emission Scalar", &emissionScalar, 0.f, 3.f);
                ImGui::SliderFloat("SSS Amount Scalar", &sssamtScalar, 0.f, 3.f);
                ImGui::SliderFloat("Gaussian Variance", &gaussV, -2.f, 2.f);
                ImGui::SliderFloat("Chance of Casting in N direction", &nChance, 0.f, 1.f);
                ImGui::TreePop();
            }

            // Hide cluster configuration until cluster bugs are fixed.
            /*
            // store all the names of the cluster modes so imgui can use them
            static const char * clusterModeNames[] = {"Disabled", "Enabled", "Debug"};
            unsigned            clusterModeVal     = static_cast<unsigned>(clusterMode);

            // set cluster state
            if (ImGui::BeginCombo("Spatial Cluster Mode", clusterModeNames[clusterModeVal])) {
                for (unsigned i = 0; i < std::size(clusterModeNames); i++) {
                    if (ImGui::Selectable(clusterModeNames[i])) {
                        clusterMode = static_cast<ClusterMode>(i);
                        break;
                    }
                }
                ImGui::EndCombo();
            }

            // cluster values that aren't powers of two will cause flickering.
            if (clusterMode != ClusterMode::Disabled) {
                ImGui::SliderInt("Scene Subdivisions", &sceneSubdivisions, 1, 128, "%u", ImGuiSliderFlags_AlwaysClamp);
            }
            */

            if (ImGui::TreeNode("ReSTIR Config")) {
                // Set up ReSTIR Mode toggling
                static const char * restirModeNames[] = {"Off", "Initial Candidates", "Temporal Reuse", "Spatiotemporal Reuse"};
                unsigned            restirModeVal     = static_cast<unsigned>(restirMode);

                // set ReSTIR Mode State
                if (ImGui::BeginCombo("ReSTIR Mode", restirModeNames[restirModeVal])) {
                    for (unsigned i = 0; i < std::size(restirModeNames) - 1; i++) {
                        if (ImGui::Selectable(restirModeNames[i])) {
                            restirMode = static_cast<ReSTIRMode>(i);
                            break;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (restirMode > PathTracerConfig::ReSTIRMode::Off) // for ReSTIR initial candidate sampling
                    ImGui::SliderInt("Initial Candidate Count", &initialCandidateCount, 0, 64, "%i");

                // debug ReSTIR temporal reservoirs by mapping colors to image
                if (restirMode > PathTracerConfig::ReSTIRMode::InitialCandidates) ImGui::Checkbox("ReSTIR debug Map", &enableRestirMap);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
};