#include "../common/modelviewer.h"

#include <ph/rt.h>
#include <sstream>

#if !defined(__ANDROID__)
    #include <GLFW/glfw3.h> // for scene-specific keypress handling
#endif
using namespace ph;
using namespace ph::va;
using namespace ph::rt;

static const char quad_vs[] = R"glsl(
#version 420
void main() {
    const vec2 corners[] = vec2[](
        vec2(-1., -1.), // Vulkan clip space has (-1, -1) on the left-top corner of the screen.
        vec2(-1.,  3.),
        vec2( 3., -1.));
    gl_Position = vec4(corners[gl_VertexIndex % 3], 0., 1.);
}
)glsl";

// Maybe this is overkill for what is essentially a clear? it seems better than creating a buffer just to clear it though...
static const char flash_fs[] = R"glsl(
    #version 420

    layout (location = 0) out vec4 o_color;

    void main() {
        o_color = vec4(1.0, 1.0, 1.0, 1.0);
    }
)glsl";

struct PathTracerDemo : ModelViewer {
    rt::Node *mesh1 = nullptr, *mesh2 = nullptr, *mesh3 = nullptr, *light = nullptr;
    // data members to render shadow map

    Eigen::AlignedBox3f bbox;

    struct Options : ModelViewer::Options {
        float       scaling       = 1.0f;
        float       flashDuration = 2.0f;
        std::string model         = "model/ptdemo_separate/ptdemo.gltf";
        std::string center        = "5,4,-1.5";
        Options() {
#if defined(__ANDROID__)
            maxSpp = -5; // accumulate over 5 seconds on android. On desktop, use args to set.
#else
            maxSpp = -3;
#endif
            rpmode     = RenderPackMode::NOISE_FREE;
            skinMode   = skinning::SkinningMode::CPU;
            shadowMode = ShadowMode::RAY_TRACED_ALPHA;
        }
    };

    Eigen::Vector3f centerFromArg() {
        Eigen::Vector3f   result;
        std::stringstream ss = std::stringstream(_options.center);
        std::string       segment;
        float             val[3];
        int               i = 2;

        while (std::getline(ss, segment, ',')) {
            if (i < 0) break;
            val[i] = std::stof(segment);
            i--;
        }

        return Eigen::Vector3f(val[2], val[1], val[0]);
    }

    void createPipelines() {
        // Referencing Skybox::createPipelines()
        auto & vgi  = dev().vgi();
        auto   pass = mainColorPass();
        if (!pass) {
            PH_THROW("Color pass is not created yet. Are you creating the flash pipeline inside your scene's constructor?"
                     "Since flash effect depends on swapchain, the best place to call it is inside the resize() method.");
        }

        PH_REQUIRE(pass != VK_NULL_HANDLE);

        // Create basic pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutCi = ph::va::util::pipelineLayoutCreateInfo(nullptr, 0); // no descriptors needed
        PH_VA_REQUIRE(vkCreatePipelineLayout(vgi.device, &pipelineLayoutCi, vgi.allocator, &_pipelineLayout));

        /////////////////////
        // Create pipeline
        /////////////////////
        // TODO: We should probably provide a generic quad vs/API for users.
        auto quadVS  = ph::va::createGLSLShader(vgi, "flash.vert", VK_SHADER_STAGE_VERTEX_BIT, {quad_vs, std::size(quad_vs)});
        auto flashFS = ph::va::createGLSLShader(vgi, "flash.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {flash_fs, std::size(flash_fs)});
        PH_ASSERT(quadVS && flashFS);

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
        auto                                           ssci = [](auto stage, auto & shader) {
            return VkPipelineShaderStageCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, {}, {}, stage, shader.get(), "main"};
        };
        shaderStages = {
            ssci(VK_SHADER_STAGE_VERTEX_BIT, quadVS),
            ssci(VK_SHADER_STAGE_FRAGMENT_BIT, flashFS),
        };

        // Inputs to pipeline create info
        // No vertex bindings/attrs
        VkPipelineVertexInputStateCreateInfo vertexInputState {};
        vertexInputState.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputState.vertexBindingDescriptionCount   = 0;
        vertexInputState.vertexAttributeDescriptionCount = 0;

        // Viewport/scissor covers entire screen
        auto                              width    = sw().initParameters().width;
        auto                              height   = sw().initParameters().height;
        VkViewport                        viewport = {0, 0, (float) width, (float) height, 0.0f, 1.0f};
        auto                              scissor  = ph::va::util::rect2d(width, height, 0, 0);
        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        // Blend with blend constants
        // currSpp/maxSpp will be used to update blend factor each frame. Initialize blend factors to 1.0f for now
        VkPipelineColorBlendAttachmentState blendAttachmentState {};
        blendAttachmentState.colorWriteMask      = 0xf;
        blendAttachmentState.blendEnable         = VK_TRUE;
        blendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        blendAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkPipelineColorBlendStateCreateInfo colorBlendState {};
        colorBlendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments    = &blendAttachmentState;

        // Basic multisample state
        VkPipelineMultisampleStateCreateInfo multisampleState {};
        multisampleState.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Basic rasterization state
        VkPipelineRasterizationStateCreateInfo rastState {};
        rastState.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rastState.polygonMode = VK_POLYGON_MODE_FILL;
        rastState.cullMode    = VK_CULL_MODE_NONE;
        rastState.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rastState.lineWidth   = 1.0f;
        rastState.flags       = 0;

        // Ignore depth stencil
        VkPipelineDepthStencilStateCreateInfo depthStencilState {};
        depthStencilState.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthTestEnable  = VK_FALSE;
        depthStencilState.depthWriteEnable = VK_FALSE;

        // Load shaders and create pipeline.
        VkPipelineInputAssemblyStateCreateInfo iaState {};
        iaState.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        iaState.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        iaState.flags                  = 0;
        iaState.primitiveRestartEnable = VK_FALSE;

        // Dynamic state: viewport, scissor, blend constants
        VkDynamicState                   dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS};
        VkPipelineDynamicStateCreateInfo dynamicStateCi  = {.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                           .dynamicStateCount = (uint32_t) std::size(dynamicStates),
                                                           .pDynamicStates    = dynamicStates};

        VkGraphicsPipelineCreateInfo pipelineCi {};
        pipelineCi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCi.pInputAssemblyState = &iaState;
        pipelineCi.pRasterizationState = &rastState;
        pipelineCi.pColorBlendState    = &colorBlendState;
        pipelineCi.pMultisampleState   = &multisampleState;
        pipelineCi.pViewportState      = &viewportState;
        pipelineCi.pVertexInputState   = &vertexInputState;
        pipelineCi.pDepthStencilState  = &depthStencilState;
        pipelineCi.pDynamicState       = &dynamicStateCi;
        pipelineCi.stageCount          = static_cast<uint32_t>(shaderStages.size());
        pipelineCi.pStages             = shaderStages.data();
        pipelineCi.renderPass          = pass;
        pipelineCi.layout              = _pipelineLayout;

        PH_VA_REQUIRE(vkCreateGraphicsPipelines(vgi.device, VK_NULL_HANDLE, 1, &pipelineCi, vgi.allocator, &_flashPipeline));
    }

    void setFirstPersonToSceneCamera() {

        if (cameras.size() > 1) // if imported scene has a camera, switch to it
        {
            auto & desc = cameras[1]->desc();
            cameras[0]->reset(desc);

            auto sceneExtent = bbox.diagonal().norm();
            firstPersonController.setHandness(desc.handness)
                .setMinimalOrbitalRadius(sceneExtent / 100.0f)
                .setMouseMoveSensitivity(sceneExtent / 1000.0f)
                .setMoveSpeed(Eigen::Vector3f::Constant(sceneExtent / 3.0f));

            Eigen::Vector3f camPos;
            cameras[1]->node().worldTransform().decompose(&camPos, nullptr, nullptr);
            Eigen::Vector3f centerPos = bbox.center();
            Eigen::Vector3f forward   = cameras[1]->node().worldTransform().rotation().col(2).normalized();
            centerPos                 = camPos - forward * ((camPos - sceneCenter).norm());
            firstPersonController.setOrbitalCenter(&centerPos).setPosition(camPos);
        } else {
            setupDefaultCamera(bbox);
        }
    }

    PathTracerDemo(SimpleApp & app, const Options & o): ModelViewer(app, o), _options(o) {
        sceneCenter                   = centerFromArg();
        recordParameters.ambientLight = {0.2f, 0.2f, 0.1f};
        ptConfig.jitterAmount         = 1.0f;
        ptConfig.backscatterMode      = 3;
        ptConfig.reflectionMode       = 2;
        ptConfig.subsurfaceChance     = 0.5f;
        auto scaling                  = o.scaling;
        bool overrideCam              = false;
        if (!o.model.empty()) { // Load scene from path
            scene->name = o.model;
            addModelNodeToScene({o.model, "*", nullptr}, bbox);

            // Update subsurface info
            _bodySsc = {
                "bodymaterial", 1.0f, {1.0f, 0.0f, 0.0f}, "model/ptdemo_separate/textures/body_sss.png", "", false,
            };
            _bodySsc.setSubsurfaceMaterial(scene, textureCache);
            _lotusSsc = {
                "lotusleafmaterial",
                1.0f,
                {0.0f, 0.0f, 0.0f},
                "model/ptdemo_separate/textures/lotus_sss.png",
                "model/ptdemo_separate/textures/lotus_sssamt.png",
                true,
            };
            _lotusSsc.setSubsurfaceMaterial(scene, textureCache);

            setFirstPersonToSceneCamera();
            if (cameras.size() >= 1) setPrimaryCamera(1);

        } else { // Manually composite objs and create materials
            auto                baseDesc = []() { return rt::World::MaterialCreateParameters {}; };
            Eigen::AlignedBox3f unused;
            auto                bodyMat  = world->create("body", baseDesc()
                                                     .setAlbedoMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/body_basecolor.png"))
                                                     .setSSS(1.0f)
                                                     .setOpaqueness(0.1f)
                                                     .setOrmMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/body_orm.png"))
                                                     .setNormalMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/body_normal.png"))
                                                     //.setEmission(1.0f, 0.0f, 0.0f));
                                                     .setEmissionMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/body_sss.png")));
            auto                hairMat  = world->create("hair", baseDesc()
                                                     .setAlbedoMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/hair_diffuse.png"))
                                                     .setNormalMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/hair_normal.png"))
                                                     .setSSS(0.0f)
                                                     .setRoughness(0.0f)
                                                     .setMetalness(0.0f));
            auto                wingMat  = world->create("wings", baseDesc()
                                                      .setAlbedoMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/wings_base_clampedalpha.png"))
                                                      //.setAlbedo(0.95f, 0.85f, 1.0f)
                                                      .setOpaqueness(0.5f)
                                                      .setNormalMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/wings_normal.png"))
                                                      .setOrmMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/wings_orm.png"))
                                                      .setIor(1.45f)
                                                      .setSSS(0.0f));
            auto                lotusMat = world->create("lotus", baseDesc()
                                                       .setAlbedoMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/lotus_basecolor.png"))
                                                       .setNormalMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/lotus_normal.png"))
                                                       .setEmissionMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/lotus_sss.png"))
                                                       .setDepthMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/lotus_sssamt.png"))
                                                       .setOrmMap(textureCache->loadFromAsset("model/ptdemo_separate/textures/lotus_orm.png"))
                                                       .setIor(0.0f)
                                                       .setSSS(1.0f)
                                                       .setRoughness(1.0f));
            mesh1                        = addModelNodeToScene({"model/ptdemo_separate/lotus.obj", "*", lotusMat}, bbox);
            mesh2                        = addModelNodeToScene({"model/ptdemo_separate/body.obj", "*", bodyMat, mesh1}, unused);
            mesh3                        = addModelNodeToScene({"model/ptdemo_separate/hair.obj", "*", hairMat, mesh2}, unused);
            mesh3                        = addModelNodeToScene({"model/ptdemo_separate/wings.obj", "*", wingMat, mesh2}, unused);
            mesh2->setTransform(rt::NodeTransform::make(Eigen::Vector3f(-1.5f, 2.5f, 1.2f), Eigen::Quaternionf::Identity(), Eigen::Vector3f::Ones()));
            mesh1->setTransform(rt::NodeTransform::make(Eigen::Vector3f::Zero(), Eigen::Quaternionf::Identity(), Eigen::Vector3f(5.0, 5.0, 5.0)));
            mesh3->setTransform(rt::NodeTransform::make(Eigen::Vector3f(0.0f, -0.1f, -0.1f), Eigen::Quaternionf::Identity(), Eigen::Vector3f::Ones()));
            mesh1 = mesh2 = mesh3 = nullptr;
            addDirectionalLight(bbox.max(), Eigen::Vector3f(-1.0f, -0.5f, 1.0f), 1.0f);
            overrideCam = true;
            setupDefaultCamera(bbox);
        }

        addSkybox(0.0f);
        if (overrideCam) {
            auto camPos   = Eigen::Vector3f(-10.15f, 23.51f, 21.07f);
            auto camAngle = Eigen::Vector3f(-0.12f, -0.14f, 0.0f);
            firstPersonController.setOrbitalCenter(nullptr);
            firstPersonController.setAngle(camAngle);
            firstPersonController.setPosition(camPos * scaling);
        }
        setupShadowRenderPack();

        createPipelines();
    }

    ~PathTracerDemo() {
        auto & vgi = dev().vgi();
        ph::va::threadSafeDeviceWaitIdle(vgi.device);
        vgi.safeDestroy(_flashPipeline);
        vgi.safeDestroy(_pipelineLayout);
    }

    const FrameTiming & update() override {
        using namespace std::chrono_literals;
        auto & t = ModelViewer::update();

        // animate light
        /*if (animated()) {
            constexpr auto cycle = std::chrono::duration_cast<std::chrono::microseconds>(8s).count();
            auto angle  = PI * 2.0f * (float)(t.sinceBeginning.count() % cycle) / (float)cycle;
            auto radius = 0.7f * _options.scaling;
            auto lightX = std::sin(angle) * radius;
            auto lightZ = std::cos(angle) * radius;
            for (size_t i = 0; i < lights.size(); i++)
            {
                ph::rt::Node & lightNode = lights[i]->node();
                ph::rt::NodeTransform transform = lightNode.worldTransform();
                ph::rt::Light::Desc ld = lights[i]->desc();
                if (ld.type == ph::rt::Light::Type::POINT)
                {
                    transform.translation() = Eigen::Vector3f(
                        lightX,
                        transform.translation().y(),
                        lightZ
                        );
                    lightNode.setWorldTransform(transform);
                }
                else if ((ld.type == ph::rt::Light::Type::DIRECTIONAL) || (ld.type == ph::rt::Light::Type::SPOT))
                {
                    auto rotation  = PI * 2.0f * 1000 / (float)cycle;
                    auto rotate = Eigen::AngleAxis(rotation, Eigen::Vector3f::UnitY());
                    transform.rotate(rotate);
                    lightNode.setWorldTransform(transform);
                }
            }
        }*/

        // animate mesh1
        if (animated() && mesh1) {
            constexpr auto         cycle           = std::chrono::duration_cast<std::chrono::microseconds>(5s).count();
            auto                   angle           = PI * -2.0f * (float) (t.sinceBeginning.count() % cycle) / (float) cycle;
            static Eigen::Vector3f baseTranslation = mesh1->transform().translation();
            auto                   tr              = mesh1->transform();

            // Do _NOT_ use auto to declare "t" for the reason described here:
            // http://eigen.tuxfamily.org/dox-devel/TopicPitfalls.html#title3
            // If you do, the whole calcuation will result in garbage in release build.
            // In short, Eigen library is not auto friendly :(
            Eigen::Vector3f translation = baseTranslation + Eigen::Vector3f(0.f, 0.5f * _options.scaling * std::sin(angle), 0.f);

            auto r = Eigen::AngleAxisf(angle, Eigen::Vector3f::UnitY()) * Eigen::AngleAxisf(PI * 0.25f, Eigen::Vector3f(1.0f, 1.0f, 1.0f).normalized());
            tr.translation() = translation;
            tr.linear()      = r.matrix();
            mesh1->setTransform(tr);
        }

        // animate the mesh3
        if (animated() && mesh3) {
            // calculate scaling factor
            constexpr auto cycle   = std::chrono::duration_cast<std::chrono::microseconds>(1s).count();
            auto           angle   = PI * -2.0f * (float) (t.sinceBeginning.count() % cycle) / (float) cycle;
            float          scaling = std::sin(angle) * 0.25f + 0.75f; // scaling in range [0.5, 1.0];

            static auto baseTransform = mesh3->transform();
            auto        newTransform  = baseTransform;
            newTransform.scale(Eigen::Vector3f(1.0f, scaling, 1.f)); // do non-uniform scaling.
            mesh3->setTransform(newTransform);
        }

        return t;
    }

    void setSnapshot(bool b) { snapshot = b; }
    void pulseSnapshot() { setSnapshot(true); }

    void prepare(VkCommandBuffer cb) override {
        if (!scene) return;
        ph::rt::RayTracingRenderPack::RecordParameters rp = recordParameters;
        rp.scene                                          = scene;
        rp.commandBuffer                                  = cb;
        scene->prepareForRecording(cb);
        noiseFreeRenderPack->prepareForRecording(rp);
        snapshotRenderPack->prepareForRecording(rp);
    }

    void recordOffscreenPass(const PassParameters & p) override {
        if (!debugPt) {
            bool beginSnapshot = snapshot && (options.rpmode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE);

            if (snapshot) {
                // Switch from one rpmode to the other
                if (beginSnapshot) {
                    options.rpmode   = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::PATH_TRACING;
                    options.animated = false;
                    setAnimated(false);
                } else {
                    options.rpmode   = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE;
                    options.animated = true;
                    setAnimated(true);
                }

                targetMode = options.rpmode;

                recreateMainRenderPack();

                setSnapshot(false);
            }
        }
        ModelViewer::recordOffscreenPass(p);
    }
    void recordMainColorPass(const PassParameters & p) override {
        ModelViewer::recordMainColorPass(p);

        if (debugPt) return; // don't flash while debugging path tracer

        float factor = pathRayTracingRenderPack->accumulationProgress(sw().initParameters().count, pauseTime());
        if (factor <= 0.f) return; // skip flash

        // Add flash effect
        auto       width    = sw().initParameters().width;
        auto       height   = sw().initParameters().height;
        VkViewport viewport = {0, 0, (float) width, (float) height, 0.0f, 1.0f};
        auto       scissor  = ph::va::util::rect2d(width, height, 0, 0);
        vkCmdSetViewport(p.cb, 0, 1, &viewport);
        vkCmdSetScissor(p.cb, 0, 1, &scissor);
        vkCmdBindPipeline(p.cb, VK_PIPELINE_BIND_POINT_GRAPHICS, _flashPipeline);
        factor     = 1.f - factor;
        factor     = std::min(_options.flashDuration * factor * factor, 1.f);
        float f4[] = {factor, factor, factor, 1.f};
        vkCmdSetBlendConstants(p.cb, f4);
        vkCmdDraw(p.cb, 3, 1, 0, 0); // draw single triangle that covers the whole screen.
    }

    void onKeyPress(int key, bool down) override {
#if !defined(__ANDROID__)
        if (GLFW_KEY_P == key && !down) { pulseSnapshot(); }
#endif
        // Only allow camera movement/other functions in real-time mode
        if (debugPt || (options.rpmode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE)) ModelViewer::onKeyPress(key, down);
    }

protected:
    void describeImguiUI() override {
        ModelViewer::describeImguiUI();

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Path Tracer Demo")) {
            if (ImGui::Checkbox("Debug in path tracing mode", &debugPt)) {
                if (!debugPtRenderPack || !noiseFreeRenderPack) recreateRenderPacks();
                if (debugPt) {
                    setAnimated(false);
                    selectedCameraIndex      = 0; // first person camera
                    pathRayTracingRenderPack = debugPtRenderPack;
                    targetMode               = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::PATH_TRACING;
                } else {
                    setAnimated(true);
                    selectedCameraIndex      = 1;
                    pathRayTracingRenderPack = noiseFreeRenderPack;
                    targetMode               = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE;
                }
            }

            if (ImGui::SliderFloat("Subsurface Intensity", &_bodySsc.scaling, 1.0f, 100.0f)) {
                _lotusSsc.scaling = _bodySsc.scaling;
                _bodySsc.setSubsurfaceMaterial(scene, textureCache, true);
                _lotusSsc.setSubsurfaceMaterial(scene, textureCache, true);
            }

            Eigen::Vector3f center     = sceneCenter; // Eigen::Vector3f::Zero();
            float           handedness = 1.0f;
            for (size_t i = 0; i < lights.size(); ++i) {
                auto               light = lights[i];
                Eigen::Vector3f    position;
                Eigen::Quaternionf origRotation;
                light->node().worldTransform().decompose(&position, &origRotation, nullptr);

                if (origLightRotation == nullptr) {
                    origLightRotation  = new Eigen::Quaternionf();
                    *origLightRotation = origRotation;
                }

                Eigen::Vector3f toLight  = position - center;
                float           distance = toLight.norm();
                toLight.normalize();
                float rotation = std::atan2(toLight.x(), toLight.z());
                float height   = -std::asin(toLight.y());
                if (ImGui::TreeNode(formatstr("Light %zu", i))) {
                    ImGui::SliderFloat("distance", &distance, 0.01f, 1000.0f);
                    ImGui::SliderFloat("height", &height, -HALF_PI + 0.01f, HALF_PI - 0.01f);
                    ImGui::SliderFloat("rotation", &rotation, 0.f, 2.f * PI);
                    ImGui::TreePop();
                }
                // borrowed from first person controller
                float           y      = distance * -std::sin(height) * handedness;
                float           p      = distance * std::cos(height);
                float           x      = p * std::sin(rotation) * handedness;
                float           z      = p * std::cos(rotation) * handedness;
                Eigen::Vector3f newPos = center + Eigen::Vector3f(x, y, z);

                Eigen::Quaternionf r = Eigen::AngleAxisf(height, Eigen::Vector3f::UnitX()) * Eigen::AngleAxisf(rotation, Eigen::Vector3f::UnitY()) *
                                       Eigen::AngleAxisf(0, Eigen::Vector3f::UnitZ());

                r *= *origLightRotation;

                // Combine everything into view transform.
                ph::rt::NodeTransform tfm = ph::rt::NodeTransform::Identity();
                tfm.translate(newPos);
                tfm.rotate(origRotation);
                light->node().setWorldTransform(tfm);
            }

            if (false) {
                ImGui::Text("Scene center: %f, %f, %f", sceneCenter.x(), sceneCenter.y(), sceneCenter.z());
                if (ImGui::TreeNode("Cameras")) {
                    if (ImGui::TreeNode("First Person Controller")) {
                        const auto & p = firstPersonController.position();
                        const auto & a = firstPersonController.angle();
                        ImGui::Text("position: %f, %f, %f", p.x(), p.y(), p.z());
                        ImGui::Text("angle   : %f, %f, %f", a.x(), a.y(), a.z());
                        if (firstPersonController.orbiting()) {
                            const auto & c = firstPersonController.orbitalCenter();
                            ImGui::Text("orbital center: %f, %f, %f", c.x(), c.y(), c.z());
                            ImGui::Text("orbital radius: %f", firstPersonController.orbitalRadius());
                        }
                        ImGui::TreePop();
                    }
                    for (size_t i = 0; i < cameras.size(); ++i) {
                        const auto & c = cameras[i];
                        if (ImGui::TreeNode(formatstr("Camera %zu", i))) {
                            Eigen::Vector3f    p;
                            Eigen::Quaternionf r;
                            c->node().worldTransform().decompose(&p, &r, nullptr);
                            ImGui::Text("position: %f, %f, %f", p.x(), p.y(), p.z());
                            ImGui::Text("rotation: %f, %f, %f, %f", r.x(), r.y(), r.z(), r.w());
                            const auto & cd = c->desc();
                            ImGui::Text("znear: %f, zfar: %f, yfov: %f", cd.zNear, cd.zFar, cd.yFieldOfView);
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }

            if (!debugPt) {
                const char * buttonLabel = animated() ? "Snapshot" : "Resume";
                if (ImGui::SmallButton(buttonLabel)) pulseSnapshot();
            }
            ImGui::TreePop();
        }
    }

    void recreateMainRenderPack() override {
        if (pathRayTracingRenderPack == snapshotRenderPack || pathRayTracingRenderPack == noiseFreeRenderPack)
            pathRayTracingRenderPack = nullptr; // let ptdemo manage these two render packs

        // Check for resize
        // TODO: resizing code crashes. fix this after that's been fixed
        auto newW    = cp().app.sw().initParameters().width;
        auto newH    = cp().app.sw().initParameters().height;
        bool resized = (newW != _renderTargetSize.width || newH != _renderTargetSize.height);

        if (resized || !snapshotRenderPack || !noiseFreeRenderPack || !debugPtRenderPack) recreateRenderPacks();

        if (targetMode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE)
            pathRayTracingRenderPack = noiseFreeRenderPack;
        else if (targetMode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::PATH_TRACING) {
            if (debugPt)
                pathRayTracingRenderPack = debugPtRenderPack;
            else
                pathRayTracingRenderPack = snapshotRenderPack;
        } else
            ModelViewer::recreateMainRenderPack();
    }

private:
    void recreateRenderPacks() {
        auto w  = sw().initParameters().width;
        auto h  = sw().initParameters().height;
        auto cp = World::RayTracingRenderPackCreateParameters {ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE}
                      .setTarget(sw().initParameters().colorFormat, w, h, VK_IMAGE_LAYOUT_UNDEFINED)
                      .setViewport(0.f, 0.f, (float) w, (float) h)
                      .setClear(true)
                      .setTracing(options.spp, options.maxSpp, options.accum);
        if (noiseFreeRenderPack || snapshotRenderPack) {
            threadSafeDeviceWaitIdle(dev().vgi().device);
            if (noiseFreeRenderPack) world->deleteRayTracingRenderPack(noiseFreeRenderPack);
            if (snapshotRenderPack) world->deleteRayTracingRenderPack(snapshotRenderPack);
            if (debugPtRenderPack) // todo: disable in release?
                world->deleteRayTracingRenderPack(debugPtRenderPack);
        }
        PH_ASSERT(!noiseFreeRenderPack);
        PH_ASSERT(!snapshotRenderPack);

        noiseFreeRenderPack = world->createRayTracingRenderPack(cp);
        cp.mode             = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::PATH_TRACING;
        snapshotRenderPack  = world->createRayTracingRenderPack(cp);
        cp.setTracing(options.spp, 0, true);
        debugPtRenderPack = world->createRayTracingRenderPack(cp);

        targetMode               = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE;
        pathRayTracingRenderPack = noiseFreeRenderPack;
    }

    ph::rt::RayTracingRenderPack * noiseFreeRenderPack = nullptr;
    ph::rt::RayTracingRenderPack * snapshotRenderPack  = nullptr;
    ph::rt::RayTracingRenderPack * debugPtRenderPack   = nullptr;

    bool snapshot = false;
    bool debugPt  = false;

    const Options _options;

    VkPipeline       _flashPipeline {};
    VkPipelineLayout _pipelineLayout {};

    PathTracerConfig::SubsurfaceConfig _bodySsc;
    PathTracerConfig::SubsurfaceConfig _lotusSsc;

    Eigen::Vector3f      sceneCenter;
    Eigen::Quaternionf * origLightRotation = nullptr;
};
