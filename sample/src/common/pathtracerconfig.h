#include <ph/rt.h>
#include "ui.h"

// Configures path tracer
struct PathTracerConfig {
public:
    struct SubsurfaceConfig {
        std::string materialName = "";
        float       scaling      = 1.0f;
        float       ssColor[3]   = {1.0f, 0.0f, 0.0f};
        std::string ssMap        = "";
        std::string ssAmtMap     = "";
        bool        isThin       = false;

        void setSubsurfaceMaterial(ph::rt::Scene * scene, std::unique_ptr<TextureCache> & textureCache, bool forceUpdate = false) {
            std::vector<ph::rt::Material *> mats     = scene->getMaterials();
            ph::rt::Material *              material = nullptr;
            for (auto mat : mats) {
                if (mat->name == materialName) {
                    material = mat;
                    break;
                }
            }

            if (material) {
                ph::rt::Material::Desc matdesc = material->desc();
                matdesc.setSSS(scaling);
                matdesc.setEmission(ssColor[0], ssColor[1], ssColor[2]);
                if (!ssMap.empty()) matdesc.setEmissionMap(textureCache->loadFromAsset(ssMap));
                if (!ssAmtMap.empty()) matdesc.setDepthMap(textureCache->loadFromAsset(ssAmtMap));
                matdesc.setIor(isThin ? 0.f : 1.45f);

                if (forceUpdate)
                    scene->debug_updateMaterial(material, matdesc);
                else
                    material->setDesc(matdesc);
            }
        }
    };

    uint32_t reflectionMode;
    uint32_t backscatterMode;
    float    jitterAmount;
    float    subsurfaceChance;
    // By default, full featureset is enabled
    PathTracerConfig(bool pathTracerEnabled = false) {
        if (pathTracerEnabled) {
            reflectionMode   = 2;    // microfacet specular + diffuse for both direct and indirect computations
            backscatterMode  = 3;    // compute backscatter at both front and back faces
            jitterAmount     = 0.0f; // cast primary ray; don't jitter prez camera
            subsurfaceChance = 0.5f; // uniformly sample between subsurface indirect and reflected indirect
        } else {
            reflectionMode   = 0;
            backscatterMode  = 0;
            jitterAmount     = 0.0f;
            subsurfaceChance = 0.0f;
        }
    }
    ~PathTracerConfig() = default;

    void setupRp(ph::rt::RayTracingRenderPack::RecordParameters & rp) const {
        rp.reflectionMode   = reflectionMode;
        rp.backscatterMode  = backscatterMode;
        rp.jitterAmount     = jitterAmount;
        rp.subsurfaceChance = subsurfaceChance;
    }

    void describeImguiUI() {
        if (ImGui::TreeNode("Path Tracer Config")) {
            if (ImGui::TreeNode("Reflection Mode")) {
                if (ImGui::BeginListBox("", ImVec2(0, 4 * ImGui::GetTextLineHeightWithSpacing()))) {
                    if (ImGui::Selectable("GGX Specular + Diffuse Direct, Uniform Indirect", reflectionMode == 0)) { reflectionMode = 0; }
                    if (ImGui::Selectable("GGX Specular Direct, GGX Specular Indirect", reflectionMode == 1)) { reflectionMode = 1; }
                    if (ImGui::Selectable("GGX Specular + Diffuse Direct and Indirect", reflectionMode == 2)) { reflectionMode = 2; }
                    ImGui::EndListBox();
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Backscatter Mode")) {
                if (ImGui::BeginListBox("", ImVec2(0, 4 * ImGui::GetTextLineHeightWithSpacing()))) {
                    if (ImGui::Selectable("No Backscattering", backscatterMode == 0)) { backscatterMode = 0; }
                    if (ImGui::Selectable("Front Face", backscatterMode == 1)) { backscatterMode = 1; }
                    if (ImGui::Selectable("Back Face", backscatterMode == 2)) { backscatterMode = 2; }
                    if (ImGui::Selectable("Front and Back Face", backscatterMode == 3)) { backscatterMode = 3; }
                    ImGui::EndListBox();
                }
                ImGui::TreePop();
            }
            ImGui::SliderFloat("Pre-Z Camera Jitter (cast primary ray iff zero)", &jitterAmount, 0.f, 3.f);
            ImGui::SliderFloat("Subsurface Chance", &subsurfaceChance, 0.f, 1.f);
            ImGui::TreePop();
        }
    }
};