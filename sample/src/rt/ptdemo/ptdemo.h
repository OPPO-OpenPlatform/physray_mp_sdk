/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : ptdemo.h
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
#include <sstream>
#include <filesystem>
#include <ph/base.h>

#if !defined(__ANDROID__)
    #include <GLFW/glfw3.h>
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

    /////////////////////
    // Day/night textures
    Material::TextureHandle     dayReflMap = Material::TextureHandle::EMPTY_CUBE();
    Material::TextureHandle     dayDiffMap = Material::TextureHandle::EMPTY_CUBE();
    Material::TextureHandle     ngtReflMap = Material::TextureHandle::EMPTY_CUBE();
    Material::TextureHandle     ngtDiffMap = Material::TextureHandle::EMPTY_CUBE();
    Material::TextureHandle     dynReflMap = Material::TextureHandle::EMPTY_CUBE();
    Material::TextureHandle     dynDiffMap = Material::TextureHandle::EMPTY_CUBE();
    const ph::va::ImageObject * dayReflImg;
    const ph::va::ImageObject * dayDiffImg;
    const ph::va::ImageObject * ngtReflImg;
    const ph::va::ImageObject * ngtDiffImg;
    ph::va::ImageObject         dynReflImg;
    ph::va::ImageObject         dynDiffImg;
    bool                        isDay        = true;
    bool                        skyboxIsDay  = false;
    rt::Node *                  fireflyNode  = nullptr;
    rt::Node *                  light0Node   = nullptr;
    rt::Light *                 fireflyLight = nullptr;
    rt::Light *                 mainLight    = nullptr;
    Eigen::Vector3f             fireflyScaling;
    rt::Node *                  bodyNode    = nullptr;
    rt::Node *                  dropletNode = nullptr;
    Eigen::Vector3f             dropletScaling;
    rt::Material *              lakeMat = nullptr;

    // Save out snapshot
    ph::va::ImageObject _accumulatedImage;
    ///////////
    /// Procedural headturn
    struct LookAtParams {
        rt::Node *      joint;
        NodeTransform   origParentToWorld;
        NodeTransform   origLocalToParent;
        Eigen::Vector3f thetaAxis; // Represents the direction of the visual mesh Y axis relative to the joint Y axis
        float           thetaOffset;
        float           phiOffset;
        float           phiScalar; // For some reason, head and right eye use -dot(toCam, localY) while left eye uses positive
        bool            isNeck;    // head requires angle clamping and motion lerping
    };
    LookAtParams neckParams, leftEyeParams, rightEyeParams;

    // Procedural headturn debug vars
    float           targetTheta;
    float           targetPhi;
    float           theta     = 0.f;
    float           phi       = 0.f;
    float           lookDelay = 0.3f;
    float           lerpT     = 0.f;
    Eigen::Vector3f localToCam;
    float           rTheta, rPhi;
    float           lTheta, lPhi;
    float           debugThetaOffset;
    float           debugPhiOffset;
    Eigen::Vector3f rLocalToCam;
    Eigen::Vector3f lLocalToCam;
    Eigen::Vector3f lThetaAxis, rThetaAxis;
    float           mouseMoveSensitivity = 0.001f;

#if defined(__ANDROID__)
    float mouseWheelSensitivity = 0.0001f;
#else
    float mouseWheelSensitivity = 0.01f;
#endif
    bool                  saveSnapshotWhenReady = false;
    std::filesystem::path imageSavePath;

    // User camera constraints
    float maxOrbitalRadius = 2.0f;
    float minOrbitalRadius = 0.33f;
    float maxCameraPhi     = 0.2f;  // constrains orbiting to below the scene
    float minCameraPhi     = -0.3f; // constrains orbiting to above the scene
    float minCameraTheta   = -HALF_PI;
    float maxCameraTheta   = HALF_PI;

    // data members to render shadow map
    Eigen::AlignedBox3f bbox;

    bool idleEnabled       = false;
    bool userCameraEnabled = false;
    struct TrackedAnimation {
        std::string                             name;
        std::shared_ptr<::animations::Timeline> timeline;
    };
    enum TrackedAnimations {
        CAMERA,
        SAYHI,
        LEAF1_SAYHI,
        LEAF2_SAYHI,
        DROP1_SAYHI,
        DROP2_SAYHI,
        ELF_FIREFLY,
        FIREFLY_OUT,
        IDLE,
        LEAF1_IDLE,
        LEAF2_IDLE,
        DROP1_IDLE,
        DROP2_IDLE,
        FIREFLY_IDLE,
        CAM_FAIRY,
        CAM_LEAVES,
        CAM_LEAVES_NIGHT,
        CAM_LOTUS,
        CAM_LOTUS_2,
        CAM_REFLECTION,
        CAM_WINGS,
        CAM_REFLECTION2,
        ANIMCOUNT
    };
    std::string                             trackedAnimationNames[TrackedAnimations::ANIMCOUNT] = {"act-camera-walk",
                                                                       "elf-sayhi",
                                                                       "hy1-sayhi",
                                                                       "hy2-sayhi",
                                                                       "waterdrop1-sayhi",
                                                                       "waterdrop2-sayhi",
                                                                       "elf-found-firefly",
                                                                       "firefly-out",
                                                                       "elf-idle",
                                                                       "hy1-idle",
                                                                       "hy2-idle",
                                                                       "waterdrop1-idle",
                                                                       "waterdrop2-idle",
                                                                       "firefly-idle",
                                                                       "Fairy",
                                                                       "Leaves",
                                                                       "Cam Leaves NightAction",
                                                                       "Lotus New",
                                                                       "Lotus New2",
                                                                       "Reflection",
                                                                       "Wings",
                                                                       "Reflection 2"};
    std::shared_ptr<::animations::Timeline> trackedAnimations[TrackedAnimations::ANIMCOUNT];

    struct SnapshotView {
        std::string                             name;
        bool                                    restartActiveAnims;
        int                                     cameraIndex;
        TrackedAnimations                       cameraAnimation;
        std::shared_ptr<::animations::Timeline> cameraTimeline;
    };

    enum SnapshotViews { BLOOM, BLOOM2, LEAVES, REFLECTIONS, CHARACTER, WINGS, FACE, COUNT };
    SnapshotView      views[SnapshotViews::COUNT];
    int               activeView = -1;
    TrackedAnimations leavesAnimation() { return isDay ? TrackedAnimations::CAM_LEAVES : TrackedAnimations::CAM_LEAVES_NIGHT; }
    int               leavesCamera() { return isDay ? 4 : 5; }

    bool snapshotInProgress = false;

    std::chrono::microseconds snapshotMicroseconds;
    float                     snapshotDelay = 1.5f;

    struct AutomaticSnapshot {
        std::chrono::duration<uint64_t, std::nano> time;
        TrackedAnimations                          animation;
        std::shared_ptr<::animations::Timeline>    timeline;
        uint32_t                                   playCount;
        bool                                       enabled;
        bool                                       delaySnap; // padding between idle animation snapshots
    };
    AutomaticSnapshot snapshots[SnapshotViews::COUNT];
    AutomaticSnapshot ngtsnapshots[SnapshotViews::COUNT];
    AutomaticSnapshot currSnapshots[SnapshotViews::COUNT];
    bool              finishedVideo = false;

    // Can only be fast_pt or path_tracing. Allows ptdemo to run with full pt if specified via existing args.
    ModelViewer::Options::RenderPackMode ptMode;
    struct Options : ModelViewer::Options {
        float       flashDuration   = 2.0f;
        std::string center          = "0.04, 0.83, 0.04";
        bool        day             = true;
        bool        debug           = true;
        bool        enableIdle      = false;
        float       roughnessCutoff = 0.4f;
        int         outputVideo     = 0;
        int         cameraAnimation = -1;
        bool        skipCamAnim     = false;
        uint32_t    restirM         = 0;
        Options() {
            accum               = -5; // accumulate over 5 seconds on android. On desktop, use args to set.
            enableDebugGeometry = true;
#if defined(__ANDROID__)
            rpmode     = RenderPackMode::FAST_PT;
            shadowMode = ph::rt::RayTracingRenderPack::ShadowMode::RAY_TRACED;
#else
            rpmode     = RenderPackMode::PATH_TRACING;
            shadowMode = ph::rt::RayTracingRenderPack::ShadowMode::RAY_TRACED;
#endif
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

    bool videoComplete() { return finishedVideo; }
    bool frameComplete() { return recordParameters.accum == ph::rt::RayTracingRenderPack::Accumulation::RETAIN; }
    void toggleRpMode() { setRpMode((options.rpmode != ptMode) ? ptMode : Options::RenderPackMode::NOISE_FREE); }

    void createPipelines() {

        // create render packs during loading
        recreateMainRenderPack();

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

    void getRenderedImage(const ph::rt::RayTracingRenderPack::RecordParameters & rp, VkImage & copyTo) {
        ph::va::setImageLayout(rp.commandBuffer, rp.targetImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        ph::va::setImageLayout(rp.commandBuffer, copyTo, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        VkImageCopy copyRegion = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                  {0, 0, 0},
                                  {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                  {0, 0, 0},
                                  {sw().initParameters().width, sw().initParameters().height, 1}};
        vkCmdCopyImage(rp.commandBuffer, rp.targetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copyTo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }

    void setupAnimations(std::vector<std::shared_ptr<::animations::Timeline>> & animVector) {
        // These are the animations for day-active
        if (!idleEnabled && isDay) {
            for (int i = TrackedAnimations::SAYHI; i <= TrackedAnimations::DROP2_SAYHI; i++) {
                auto & timeline = trackedAnimations[i];
                timeline->playFromStart();
                animVector.push_back(timeline);
            }
            // Unhide droplet
            if (dropletNode) {
                ph::rt::NodeTransform tfm = dropletNode->worldTransform();
                tfm.setScaling(dropletScaling);
                dropletNode->setWorldTransform(tfm);
            }
        } else {
            // These are the animations in common between day-idle, night-idle, and day-active
            bool activatingNight = !idleEnabled && !isDay;
            int  beginIdx        = activatingNight ? TrackedAnimations::LEAF1_IDLE : TrackedAnimations::IDLE;
            for (int i = beginIdx; i < TrackedAnimations::FIREFLY_IDLE; i++) {
                auto & timeline = trackedAnimations[i];
                timeline->playFromStart();
                timeline->setStart(std::chrono::duration<uint64_t, std::nano>(14320000000)); // idle start time is equal to duration of day active animation
                animVector.push_back(timeline);
            }
            if (activatingNight) {
                auto timeline = trackedAnimations[TrackedAnimations::ELF_FIREFLY];
                timeline->setStart(std::chrono::duration<uint64_t, std::nano>(16300000000));
                timeline->playFromStart();
                animVector.push_back(timeline);
                timeline = trackedAnimations[TrackedAnimations::FIREFLY_OUT];
                timeline->playFromStart();
                animVector.push_back(timeline);
            } else if (!isDay) {
                // Animations specific to night-idle
                auto & timeline = trackedAnimations[TrackedAnimations::FIREFLY_IDLE];
                timeline->playFromStart();
                animVector.push_back(timeline);
            }
        }

        // Preserve any camera animations that are currently playing
        animVector.push_back(views[activeView].cameraTimeline);
    }

    void playCameraAnimation(SnapshotViews svIdx) {
        PH_REQUIRE(svIdx < SnapshotViews::COUNT);

        auto & sv = views[svIdx];
        // PH_REQUIRE(sv.cameraIndex < (int) cameras.size());
        // PH_REQUIRE(sv.cameraTimeline);
        activeView = (int) svIdx;
        if (svIdx < SnapshotViews::CHARACTER)
            recordParameters.minRayLength = 0.0001f;
        else
            recordParameters.minRayLength = 0.00001f; // character rendering needs smaller min ray length for transparents
        if (svIdx == SnapshotViews::LEAVES) {         // todo clean this up
            sv.cameraIndex     = leavesCamera();
            sv.cameraAnimation = leavesAnimation();
            sv.cameraTimeline  = trackedAnimations[sv.cameraAnimation];
        }
        int cameraIdx       = std::min(sv.cameraIndex, (int) cameras.size() - 1);
        selectedCameraIndex = cameraIdx;
        if (sv.cameraTimeline) {
            sv.cameraTimeline->playFromStart();
            idleEnabled = !sv.restartActiveAnims;
            std::vector<std::shared_ptr<::animations::Timeline>> animVec;
            setupAnimations(animVec); // setupAnimations will now add the timeline associated with activeView
            animations        = animVec;
            userCameraEnabled = false;
        }

        // leaves anim has some funky horizon angles, cutoff needs to change
        // TODO re-add after merging noise-free fresnel cutoff changes
        /*if(svIdx == SnapshotViews::LEAVES)
            recordParameters.noiseFreeTransmitanceCutoff = 0.49f;
        else if (svIdx == SnapshotViews::REFLECTIONS)
            recordParameters.noiseFreeTransmitanceCutoff = .6f;
        else if (svIdx == SnapshotViews::BLOOM)
            recordParameters.noiseFreeTransmitanceCutoff = 0.15f;
        else
            //everything else works fine at this cutoff
            recordParameters.noiseFreeTransmitanceCutoff = 0.3f;*/

        theta = phi = 0; // reset headturn interpolation
    }
    Eigen::Vector3f    camPos;
    Eigen::Quaternionf camRot;

    void setCameraToSnapshotView(int svIndex) {
        PH_REQUIRE(svIndex < SnapshotViews::COUNT);
        activeView = svIndex;

        if (svIndex == SnapshotViews::CHARACTER) {
            maxCameraPhi              = 0.2f;
            minCameraPhi              = -0.3f;
            maxOrbitalRadius          = 0.9f;
            minOrbitalRadius          = 0.33f;
            Eigen::Vector3f forward   = camRot.toRotationMatrix().col(2).normalized();
            Eigen::Vector3f position  = camPos;
            Eigen::Vector3f centerPos = position - forward * ((position - sceneCenter).norm());
            sceneCenter               = centerPos;
            firstPersonController.setOrbitalCenter(&sceneCenter).setPosition(position);
        }
    }

    void setupHeadTurnParams() {
        // Set up procedural head turn params
        PH_REQUIRE(neckParams.joint);
        PH_REQUIRE(leftEyeParams.joint);
        PH_REQUIRE(rightEyeParams.joint);
        // Hardcoded based on matrix values at facing-forward position
        // recorded by debugging joint matrices at idle pose rest position
        // FINDINGS: Eigen storage data array stores its data row major
        // (copying from index 0 through size will fill in rows first, then columns)
        // FINDINGS: decomposing to position, rotation, scale at 4 significant decimals
        // will not reconstruct the same matrix using NodeTransform::make
        // so I found it faster to just debug and copy the values from vscode's
        // watch window (shift-select all array values)
        Eigen::Matrix<float, 3, 4> m;
        m.col(0)                     = Eigen::Vector3f(0.828733504f, 0.559949577f, -0.00744789513f);
        m.col(1)                     = Eigen::Vector3f(-0.560289621f, 0.829124928f, -0.0084116878f);
        m.col(2)                     = Eigen::Vector3f(0.00146407052f, 0.0111360801f, 1.000144f);
        m.col(3)                     = Eigen::Vector3f(-9.82425022f, 25.1158524f, 0.0105733182f);
        neckParams.origLocalToParent = m;

        m.col(0)                     = Eigen::Vector3f(7.05340062e-06f, -0.000122427795f, -0.000992359361f);
        m.col(1)                     = Eigen::Vector3f(0.000161797681f, 0.000979229459f, -0.000121353718f);
        m.col(2)                     = Eigen::Vector3f(0.000987036037f, -0.000159808551f, 2.67052092e-05f);
        m.col(3)                     = Eigen::Vector3f(0.00333261117f, 0.77639693f, 0.0398084447f);
        neckParams.origParentToWorld = m;

        m.col(0)                         = Eigen::Vector3f(0.184991077f, -0.136775672f, -0.973175347f);
        m.col(1)                         = Eigen::Vector3f(-0.601181388f, 0.767609179f, -0.222162902f);
        m.col(2)                         = Eigen::Vector3f(0.777405083f, 0.62615329f, 0.0597739033f);
        m.col(3)                         = Eigen::Vector3f(-9.32097721f, 11.908287f, -3.44445992f);
        rightEyeParams.origLocalToParent = m;
        m.col(0)                         = Eigen::Vector3f(0.184990898f, 0.136775583f, 0.973175108f);
        m.col(1)                         = Eigen::Vector3f(0.601181328f, 0.767609417f, -0.222162813f);
        m.col(2)                         = Eigen::Vector3f(-0.777405381f, 0.626153111f, 0.0597738512f);
        m.col(3)                         = Eigen::Vector3f(9.32889938f, 11.9021864f, -3.44945431f);
        leftEyeParams.origLocalToParent  = m;

        theta = phi = 0.f;
    }

    void enterIdleIfNeeded() {
        if (!idleEnabled) {
            int    refTimelineIdx = isDay ? int(TrackedAnimations::SAYHI) : int(TrackedAnimations::FIREFLY_OUT);
            auto & refTimeline    = trackedAnimations[refTimelineIdx];
            if (refTimeline->getPlayCount() > 0) {
                idleEnabled = true;
                std::vector<std::shared_ptr<::animations::Timeline>> noActive;
                setupAnimations(noActive);
                animations = noActive;
                setupHeadTurnParams();
            }
        }
    }

    void setupLights() {
        if (isDay) {
            if (fireflyLight) { // use firefly light as fill light during daytime
                auto fld = fireflyLight->desc();
                if (options.rpmode == Options::RenderPackMode::NOISE_FREE) {
                    fld.emission[0] = fld.emission[1] = fld.emission[2] = 0.4f;
                    fld.emission[2]                                     = 0.2f;
                } else {
                    fld.emission[0] = fld.emission[1] = fld.emission[2] = 3.0f;
                }

                fld.dimension[0] = -0.02f;
                fld.dimension[1] = -0.02f;
                fld.range        = 1.f;
                fld.allowShadow  = false;
                fld.setPoint({});
                fireflyLight->reset(fld);
                fireflyNode->detachComponent(fireflyLight);
                // Hide firefly mesh
                ph::rt::NodeTransform tfm = fireflyNode->worldTransform();
                tfm.setScaling(Eigen::Vector3f::Zero());
                fireflyNode->setWorldTransform(tfm);
                light0Node->attachComponent(fireflyLight);
                light0Node->setWorldTransform(ph::rt::NodeTransform::make(Eigen::Vector3f(0.05f, 0.84f, 0.19f),
                                                                          Eigen::Quaternionf(0.0f, 0.9956f, 0.0f, 0.0941f), Eigen::Vector3f::Identity()));
                if (debugManager) debugManager->updateDebugLight(fireflyLight);
            }
            if (mainLight) {
                // Setup lights
                // daylight

                rt::Node *      lightNode = mainLight->nodes()[0];
                Eigen::Vector3f lightScaling;
                ph::rt::NodeTransform(lightNode->worldTransform()).decompose(nullptr, nullptr, &lightScaling);
                rt::NodeTransform lightTfm = rt::NodeTransform::make(Eigen::Vector3f(24.f, 11.2f, -36.9f), Eigen::Quaternionf(0.094f, 0.005f, 0.994f, -0.056f),
                                                                     lightScaling); //
                lightNode->setWorldTransform(lightTfm);

                auto lightDesc         = mainLight->desc();
                lightDesc.type         = Light::DIRECTIONAL;
                lightDesc.dimension[0] = -0.5f; // directional for now
                lightDesc.dimension[1] = -0.5f;

                if (options.rpmode == Options::RenderPackMode::NOISE_FREE) {
                    lightDesc.emission[0] = 1.f;
                    lightDesc.emission[1] = 0.9f;
                    lightDesc.emission[2] = 0.6f;
                } else {
                    lightDesc.emission[0] = 56.f;
                    lightDesc.emission[1] = 54.f;
                    lightDesc.emission[2] = 51.f;
                }
                lightDesc.range           = 60.f;
                Eigen::Vector3f direction = Eigen::Vector3f(0.2f, -0.5f, -1.f).normalized();
                lightDesc.directional.setDir(direction.x(), direction.y(), direction.z());
                lightDesc.allowShadow = true;
                mainLight->reset(lightDesc);
                if (debugManager) debugManager->updateDebugLight(mainLight);
            }
        } else {
            if (fireflyLight) {
                auto fld         = fireflyLight->desc();
                fld.range        = 5.f;
                fld.dimension[0] = -0.01f;
                fld.dimension[1] = -0.01f;
                fld.allowShadow  = true;
                fld.setPoint({});

                if (options.rpmode == Options::RenderPackMode::NOISE_FREE) {
                    fld.emission[0] = 1.f;
                    fld.emission[1] = .6f;
                    fld.emission[2] = .336f;
                } else {
                    fld.emission[0] = 3.f;
                    fld.emission[1] = 2.0f;
                    fld.emission[2] = 1.0f;
                }

                fireflyLight->reset(fld);
                light0Node->detachComponent(fireflyLight);
                fireflyNode->attachComponent(fireflyLight);
                // Unhide firefly mesh
                ph::rt::NodeTransform tfm = fireflyNode->worldTransform();
                tfm.setScaling(fireflyScaling);
                fireflyNode->setWorldTransform(tfm);
                if (debugManager) debugManager->updateDebugLight(fireflyLight);
            }
            if (mainLight) {
                // Setup lights
                // moonlight
                // position: -15.43, 2.05, -0.22
                // rotation: 0.094, 0.005, 0.994, -0.056
                // in direction (-1, 0, 1)
                rt::Node *      lightNode = mainLight->nodes()[0];
                Eigen::Vector3f lightScaling;
                ph::rt::NodeTransform(lightNode->worldTransform()).decompose(nullptr, nullptr, &lightScaling);
                rt::NodeTransform lightTfm =
                    rt::NodeTransform::make(Eigen::Vector3f(-84.484f, 8.621f, -3.020f), Eigen::Quaternionf(-0.056f, 0.094f, 0.005f, 0.994f), lightScaling);
                lightNode->setWorldTransform(lightTfm);

                auto ld        = mainLight->desc();
                ld.allowShadow = false;
                if (options.rpmode == Options::RenderPackMode::NOISE_FREE) {
                    ld.emission[0] = .454f * 1.5f;
                    ld.emission[1] = .444f * 1.5f;
                    ld.emission[2] = .431f * 1.5f;

                    ld.type  = Light::POINT;
                    ld.range = 1000;

                    // move directional to emulate the
                    // ld.directional.setDir(Eigen::Vector3f(-1.f, 0.615f, 0.f).normalized());

                } else {
                    ld.emission[0] = ld.emission[1] = ld.emission[2] = 15.f;
                    ld.dimension[0]                                  = -1.5f;
                    ld.dimension[1]                                  = -1.5f;
                    ld.type                                          = Light::DIRECTIONAL;
                    Eigen::Vector3f direction                        = Eigen::Vector3f(-1.f, 0.f, 0.f).normalized();
                    ld.directional.setDir(direction.x(), direction.y(), direction.z());

                    ld.range = 140.f;
                }
                mainLight->reset(ld);
                if (debugManager) debugManager->updateDebugLight(mainLight);
            }
        }
    }

    void activateNight() {
        isDay       = false;
        idleEnabled = false;
        std::vector<std::shared_ptr<::animations::Timeline>> nightActiveAnimations;
        setupAnimations(nightActiveAnimations);
        animations = nightActiveAnimations;
        trackedAnimations[TrackedAnimations::IDLE]->playFromStart(); // reset idle animation so that camera snaps cue correctly
    }

    void activateDay() {
        isDay       = true;
        idleEnabled = false;
        std::vector<std::shared_ptr<::animations::Timeline>> dayIdleAnimations;
        setupAnimations(dayIdleAnimations);
        animations = dayIdleAnimations;
    }

    void toggleDayNight() {
        if (isDay)
            activateNight();
        else
            activateDay();
        setupLights();
    }

    void setFirstPersonToSceneCamera(bool switchToFp = true) {
        PH_REQUIRE(activeView >= 0);
        PH_REQUIRE(activeView < SnapshotViews::COUNT);
        auto & v           = views[activeView];
        auto & cameraIndex = v.cameraIndex;
        if ((int) cameras.size() > cameraIndex) // if imported scene has a camera, switch to it
        {
            auto sceneExtent = bbox.diagonal().norm();
            auto fpNode      = cameras[0].node;
            cameras[0]       = cameras[cameraIndex];
            cameras[0].node  = fpNode;
            cameras[0].zNear = 0.1f;
            cameras[0].zFar  = 100.f;

            firstPersonController.setHandness(cameras[cameraIndex].handness)
                .setMinimalOrbitalRadius(sceneExtent / 1000.0f)
                .setMouseMoveSensitivity(mouseMoveSensitivity)
                .setMouseWheelSensitivity(mouseWheelSensitivity)
                .setMoveSpeed(Eigen::Vector3f::Constant(sceneExtent / 100.0f))
                .setRotateSpeed(PI / 64.f);
            cameras[cameraIndex].worldTransform().decompose(&camPos, &camRot, nullptr);
            setCameraToSnapshotView(activeView);
        } else {
            setupDefaultCamera(bbox);
        }

        if (switchToFp && (activeView == SnapshotViews::CHARACTER)) selectedCameraIndex = 0;
    }

    bool isIdleAnimation(const std::string & name) {
        const std::string suffix = "-idle";
        if (name.size() < suffix.size()) return false;
        // rbegin gives a reverse iterator starting at the last character of the string
        // equal compares values over a given range
        return std::equal(suffix.rbegin(), suffix.rend(), name.rbegin());
    }

    bool isNightAnimation(const std::string & name) {
        return ((name == trackedAnimationNames[TrackedAnimations::ELF_FIREFLY]) || (name == trackedAnimationNames[TrackedAnimations::FIREFLY_OUT]) ||
                (name == trackedAnimationNames[TrackedAnimations::FIREFLY_IDLE]));
    }

    void initDayNightSkybox(const std::filesystem::path modelFolder) {
        // determine sky texture path.
        auto dayReflMapAsset = modelFolder / "skybox/day-reflection.ktx2";
        auto dayDiffMapAsset = modelFolder / "skybox/day-irradiance.ktx2";
        auto ngtReflMapAsset = modelFolder / "skybox/night-reflection.ktx2";
        auto ngtDiffMapAsset = modelFolder / "skybox/night-irradiance.ktx2";

        // Dynamic skybox setup
        VkImageUsageFlagBits usage = VkImageUsageFlagBits(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        dayReflMap                 = textureCache->loadFromAsset(dayReflMapAsset.string(), usage);
        dayDiffMap                 = textureCache->loadFromAsset(dayDiffMapAsset.string(), usage);
        ngtReflMap                 = textureCache->loadFromAsset(ngtReflMapAsset.string(), usage);
        ngtDiffMap                 = textureCache->loadFromAsset(ngtDiffMapAsset.string(), usage);
        const auto & texMap        = textureCache->textures();
        dayReflImg                 = &(texMap.find(dayReflMapAsset.string())->second);
        dayDiffImg                 = &(texMap.find(dayDiffMapAsset.string())->second);
        ngtReflImg                 = &(texMap.find(ngtReflMapAsset.string())->second);
        ngtDiffImg                 = &(texMap.find(ngtDiffMapAsset.string())->second);
        PH_REQUIRE(dayReflImg->ci.extent.width == ngtReflImg->ci.extent.width);
        PH_REQUIRE(dayReflImg->ci.extent.height == ngtReflImg->ci.extent.height);
        PH_REQUIRE(dayReflImg->ci.extent.depth == ngtReflImg->ci.extent.depth);
        PH_REQUIRE(dayDiffImg->ci.extent.width == ngtDiffImg->ci.extent.width);
        PH_REQUIRE(dayDiffImg->ci.extent.height == ngtDiffImg->ci.extent.height);
        PH_REQUIRE(dayDiffImg->ci.extent.depth == ngtDiffImg->ci.extent.depth);
        PH_REQUIRE(dayReflImg->ci.format == ngtReflImg->ci.format);
        PH_REQUIRE(dayDiffImg->ci.format == ngtDiffImg->ci.format);
        PH_REQUIRE(dayReflImg->ci.arrayLayers == ngtReflImg->ci.arrayLayers);
        PH_REQUIRE(dayDiffImg->ci.arrayLayers == ngtDiffImg->ci.arrayLayers);
        PH_REQUIRE(dayReflImg->ci.mipLevels == ngtReflImg->ci.mipLevels);
        PH_REQUIRE(dayDiffImg->ci.mipLevels == ngtDiffImg->ci.mipLevels);

        usage = VkImageUsageFlagBits(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        dynReflImg.create("ptdemo reflection map", dev().vgi(),
                          ImageObject::CreateInfo {}
                              .setCube(dayReflImg->ci.extent.width)
                              .setFormat(dayReflImg->ci.format)
                              .setLayers(dayReflImg->ci.arrayLayers)
                              .setLevels(dayReflImg->ci.mipLevels)
                              .setUsage(usage));
        dynDiffImg.create("ptdemo irradiance map", dev().vgi(),
                          ImageObject::CreateInfo {}
                              .setCube(dayDiffImg->ci.extent.width)
                              .setFormat(dayDiffImg->ci.format)
                              .setLayers(dayDiffImg->ci.arrayLayers)
                              .setLevels(dayDiffImg->ci.mipLevels)
                              .setUsage(usage));
        dynReflMap = Material::TextureHandle(dynReflImg);
        dynDiffMap = Material::TextureHandle(dynDiffImg);
    }

    void copySkybox(VkCommandBuffer cb = VK_NULL_HANDLE) {
        if (skyboxIsDay != isDay) {
            skyboxIsDay = isDay;
            SingleUseCommandPool                pool(dev().graphicsQ());
            SingleUseCommandPool::CommandBuffer singleUseCb;
            bool                                execNow = false;

            if (cb == VK_NULL_HANDLE) {
                singleUseCb = pool.create();
                cb          = singleUseCb;
                execNow     = true;
            }
            const ph::va::ImageObject * srcReflImg = isDay ? dayReflImg : ngtReflImg;
            const ph::va::ImageObject * srcDiffImg = isDay ? dayDiffImg : ngtDiffImg;
            VkImageSubresourceRange     FULL_SUBRESOURCE_RANGE =
                VkImageSubresourceRange {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};

            setImageLayout(cb, dynReflImg.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FULL_SUBRESOURCE_RANGE);
            setImageLayout(cb, srcReflImg->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, FULL_SUBRESOURCE_RANGE);
            setImageLayout(cb, dynDiffImg.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FULL_SUBRESOURCE_RANGE);
            setImageLayout(cb, srcDiffImg->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, FULL_SUBRESOURCE_RANGE);

            uint32_t                 reflNumMips   = srcReflImg->ci.mipLevels;
            uint32_t                 reflNumLayers = srcReflImg->ci.arrayLayers;
            uint32_t                 diffNumMips   = srcDiffImg->ci.mipLevels;
            uint32_t                 diffNumLayers = srcDiffImg->ci.arrayLayers;
            VkExtent3D               reflExtent    = srcReflImg->ci.extent;
            VkExtent3D               diffExtent    = srcDiffImg->ci.extent;
            std::vector<VkImageCopy> reflCopyRegions;
            std::vector<VkImageCopy> diffCopyRegions;
            VkImageCopy              reflInit = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, reflNumLayers},
                                    {0, 0, 0},
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, reflNumLayers},
                                    {0, 0, 0},
                                    reflExtent};
            VkImageCopy              diffInit = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, diffNumLayers},
                                    {0, 0, 0},
                                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, diffNumLayers},
                                    {0, 0, 0},
                                    diffExtent};
            for (uint32_t i = 0; i < reflNumMips; i++) {
                VkImageCopy & rvic           = reflInit;
                rvic.srcSubresource.mipLevel = i;
                rvic.dstSubresource.mipLevel = i;
                rvic.extent.width = rvic.extent.height = reflNumMips == 1 ? reflExtent.height : uint32_t(std::pow(2, reflNumMips - i));
                reflCopyRegions.push_back(rvic);
            }
            for (uint32_t i = 0; i < diffNumMips; i++) {
                VkImageCopy & dvic           = diffInit;
                dvic.srcSubresource.mipLevel = i;
                dvic.dstSubresource.mipLevel = i;
                dvic.extent.width = dvic.extent.height = diffNumMips == 1 ? diffExtent.height : uint32_t(std::pow(2, diffNumMips - i));
                diffCopyRegions.push_back(dvic);
            }

            vkCmdCopyImage(cb, srcReflImg->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dynReflImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reflNumMips,
                           reflCopyRegions.data());
            vkCmdCopyImage(cb, srcDiffImg->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dynDiffImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, diffNumMips,
                           diffCopyRegions.data());

            setImageLayout(cb, dynReflImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, FULL_SUBRESOURCE_RANGE);
            setImageLayout(cb, dynDiffImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, FULL_SUBRESOURCE_RANGE);

            if (execNow) pool.finish(singleUseCb);
        }
    }

    PathTracerDemo(SimpleApp & app, const Options & o): ModelViewer(app, o), _options(o) {
        std::string model;
        if (o.rpmode == Options::RenderPackMode::PATH_TRACING)
            model = "model/ptdemo/desktop/scene.gltf";
        else
            model = "model/ptdemo/mobile/scene.gltf";

        ptConfig.initialCandidateCount = _options.restirM;
        ptConfig.restirMode            = _options.restirM > 0 ? PathTracerConfig::ReSTIRMode::InitialCandidates : PathTracerConfig::ReSTIRMode::Off;

        if (_options.outputVideo)
            ptMode = Options::RenderPackMode::PATH_TRACING;
        else {
            ptMode = o.rpmode == Options::RenderPackMode::PATH_TRACING ? Options::RenderPackMode::PATH_TRACING : Options::RenderPackMode::FAST_PT;
        }

        // determine model path
        auto modelPath = std::filesystem::path(model);
        if (std::filesystem::is_directory(modelPath)) {
            auto gltf = searchForGLTF(modelPath);
            if (gltf.empty()) PH_THROW("No GLTF/GLB model found in folder: %s", modelPath.string().c_str());
            modelPath = gltf;
        }

        // get the model folder
        auto modelFolder = modelPath.parent_path();

        // preload all files in the model folder.
        assetSys->preloadFolder(modelFolder.string().c_str());

        // load scene and get tracked nodes and joints
        scene->name     = modelPath.string();
        auto sceneAsset = loadGltf({modelPath.string(), "*", nullptr});
        bbox            = sceneAsset->getBounds();

        const auto & nodes = sceneAsset->getNodes();
        for (auto & node : nodes) {
            auto & name = node->name;

            if (name == "Neck_M")
                neckParams.joint = node;
            else if (name == "EyeJoint_L")
                leftEyeParams.joint = node;
            else if (name == "EyeJoint_R")
                rightEyeParams.joint = node;
            else if (name == "firefly") {
                fireflyNode = node;
            } else if (name == "shuidi_01_md") // bounce-away water drop
            {
                dropletNode = node;
            }
        }

        if (fireflyNode) {
            const ph::rt::NodeTransform & tfm = fireflyNode->worldTransform();
            tfm.decompose(nullptr, nullptr, &fireflyScaling);

            for (auto c : fireflyNode->components())
                if (c->type() == ph::rt::NodeComponent::Type::LIGHT) fireflyLight = (ph::rt::Light *) c;

            if (fireflyLight == nullptr) {
                if (lights.size() > 0) {
                    fireflyLight = lights[0];
                } else {
                    fireflyLight = addPointLight(Eigen::Vector3f::Zero(), 2.f, Eigen::Vector3f::Zero(), 0.f, 0.f);
                }
                light0Node = *(fireflyLight->nodes().begin());
            } else {
                // disable light 0 since we are using the generated mesh light
                if (lights.size() > 0) {
                    lights[0]->reset(ph::rt::Light::Desc().setType(ph::rt::Light::Type::OFF).setEmission(0.f, 0.f, 0.f));
                    light0Node = *(lights[0]->nodes().begin());
                } else {
                    // create a separate node to separately transform the fill light
                    light0Node       = scene->createNode({});
                    light0Node->name = "fillLightNode0";
                }
            }
        }

        if (lights.size() > 1) {
            mainLight = lights[1];
            // convert mainlight to directional
            mainLight->reset(ph::rt::Light::Desc().setDirectional(ph::rt::Light::Directional().setDir(0.f, 0.f, 1.f)));
            mainLight->shadowMap = textureCache->createShadowMap2D("ptdemo main light");
        }

        for (auto l : lights) { // disable all other lights
            if ((l != fireflyLight) && (l != mainLight)) {
                auto ld = l->desc();
                ld.setType(ph::rt::Light::Type::OFF);
                ld.emission[0] = ld.emission[1] = ld.emission[2] = 0.f;
                ld.range                                         = 0.f;
                l->reset(ld);
                if (debugManager) debugManager->updateDebugLight(l);
            }
        }

        if (dropletNode) {
            const ph::rt::NodeTransform & tfm = dropletNode->worldTransform();
            tfm.decompose(nullptr, nullptr, &dropletScaling);
        }

        setupLights();

// determine image output path
#if defined(__ANDROID__)
        // requires the WRITE_EXTERNAL_STORAGE permission in AndroidManifest.xml
        imageSavePath = "/sdcard/DCIM";
#else
    // For PC, use the temp directory if available, otherwise just dump them in the current directory
    #if defined(SDK_ROOT_DIR)
        std::filesystem::path sdk_root_dir = SDK_ROOT_DIR;
        auto                  temp_dir     = sdk_root_dir / "temp";
        std::filesystem::create_directories(temp_dir);
        imageSavePath = temp_dir;
    #else
        imageSavePath = getExecutableFolder();
    #endif
#endif

        initDayNightSkybox(modelFolder);
        copySkybox();

        // Create image used to save out snapshots
        const auto & ip = sw().initParameters();
        _accumulatedImage.create("ptdemo snapshot image", dev().vgi(),
                                 ImageObject::CreateInfo {}
                                     .set2D(ip.width, ip.height)
                                     .setFormat(ip.colorFormat)
                                     .setAspect(VK_IMAGE_ASPECT_COLOR_BIT)
                                     .setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                                     .setInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED));

        // Setup procedural headturn data
        neckParams.thetaAxis   = Eigen::Vector3f(-0.5f, 1.0f, 0.f);
        neckParams.thetaOffset = 0.f;
        neckParams.phiOffset   = std::asin(0.5f);
        neckParams.phiScalar   = -1.f;
        neckParams.isNeck      = true;

        rightEyeParams.thetaAxis = rThetaAxis = Eigen::Vector3f(0.3f, 0.5f, -1.f);
        rightEyeParams.thetaOffset = debugThetaOffset = -0.66f;
        rightEyeParams.phiOffset = debugPhiOffset = 0.66f;
        rightEyeParams.phiScalar                  = -1.f;
        rightEyeParams.isNeck                     = false;

        leftEyeParams.thetaAxis = lThetaAxis = Eigen::Vector3f(0.f, 0.f, 1.f);
        leftEyeParams.thetaOffset            = -0.66f;
        leftEyeParams.phiOffset              = -0.66f;
        leftEyeParams.phiScalar              = 1.f;
        leftEyeParams.isNeck                 = false;

        // Setup snapshot views
        views[SnapshotViews::BLOOM]       = {"Lotus Bloom",
                                       false, // restart active animations
                                       8, TrackedAnimations::CAM_LOTUS, nullptr};
        views[SnapshotViews::BLOOM2]      = {"Lotus Bloom 2",
                                        false, // restart active animations
                                        9, TrackedAnimations::CAM_LOTUS_2, nullptr};
        views[SnapshotViews::REFLECTIONS] = {"Reflections",
                                             false, // restart active animations
                                             11, TrackedAnimations::CAM_REFLECTION, nullptr};
        views[SnapshotViews::LEAVES]      = {"Leaves",
                                        false, // restart active animations
                                        4, TrackedAnimations::CAM_LEAVES, nullptr};
        views[SnapshotViews::CHARACTER]   = {"Fairy",
                                           true, // restart active animations
                                           1, TrackedAnimations::CAMERA, nullptr};

        views[SnapshotViews::WINGS] = {"Wings", false, 13, TrackedAnimations::CAM_WINGS, nullptr};
        views[SnapshotViews::FACE]  = {"Face",
                                      true, // restart active animations
                                      2, TrackedAnimations::CAM_FAIRY, nullptr};

        bool recordingSnapshots = _options.outputVideo == 1;
        auto enableSnapshot     = [this, recordingSnapshots](SnapshotViews vIdx, bool recordingSnapshotsValue, bool noSnapshotsValue) {
            return recordingSnapshots ? recordingSnapshotsValue
                                          : ((noSnapshotsValue) && ((_options.cameraAnimation < 0) || (_options.cameraAnimation == (int) vIdx)));
        };
        snapshots[SnapshotViews::BLOOM] = {
            std::chrono::duration<uint64_t, std::nano>::zero(), TrackedAnimations::IDLE, nullptr, 3, enableSnapshot(SnapshotViews::BLOOM, true, true), true};
        snapshots[SnapshotViews::BLOOM2] = {
            std::chrono::duration<uint64_t, std::nano>::zero(), TrackedAnimations::IDLE, nullptr, 4, enableSnapshot(SnapshotViews::BLOOM2, true, true), true};
        snapshots[SnapshotViews::REFLECTIONS] = {std::chrono::duration<uint64_t, std::nano>::zero(),
                                                 TrackedAnimations::IDLE,
                                                 nullptr,
                                                 5,
                                                 enableSnapshot(SnapshotViews::REFLECTIONS, true, true),
                                                 true};
        snapshots[SnapshotViews::LEAVES]      = {
            std::chrono::duration<uint64_t, std::nano>::zero(), TrackedAnimations::IDLE, nullptr, 6, enableSnapshot(SnapshotViews::LEAVES, true, true), true};
        snapshots[SnapshotViews::CHARACTER] = {std::chrono::duration<uint64_t, std::nano>(9900000000), TrackedAnimations::SAYHI,         nullptr, 0,
                                               enableSnapshot(SnapshotViews::CHARACTER, true, true),   recordingSnapshots ? false : true};
        snapshots[SnapshotViews::WINGS]     = {
            std::chrono::duration<uint64_t, std::nano>::zero(), TrackedAnimations::IDLE, nullptr, 1, enableSnapshot(SnapshotViews::WINGS, true, true), true};
        snapshots[SnapshotViews::FACE] = {
            std::chrono::duration<uint64_t, std::nano>::zero(), TrackedAnimations::IDLE, nullptr, 2, enableSnapshot(SnapshotViews::FACE, true, true), true};

        sceneCenter                                = centerFromArg();
        recordParameters.ambientLight              = {0.2f, 0.2f, 0.1f};
        recordParameters.reflectionRoughnessCutoff = o.roughnessCutoff;
        recordParameters.minRayLength =
            0.00001f; // This is the min ray length required to minimize black artifacts that appear in transmissives at the scale of the scene

        options.usePrecompiledShaderParameters                = true; // this is needed to achieve real-time runtime on mobile. comment this out to debug.
        recordParameters.transparencySettings.backscatterMode = 3;
        recordParameters.transparencySettings.shadowSettings.tshadowAlpha = true;
        recordParameters.transparencySettings.shadowSettings.tshadowColor = true;
        ptConfig.jitterAmount                                             = 0.0f;
        ptConfig.subsurfaceChance                                         = 0.5f;

        // Populate timeline map and disable repeats on non-idle animations
        for (auto & timeline : animations) {
            for (int i = (int) TrackedAnimations::CAMERA; i < (int) TrackedAnimations::ANIMCOUNT; i++) {
                if (trackedAnimationNames[i] == timeline->name) {
                    if (!isIdleAnimation(timeline->name)) {
                        timeline->setRepeatCount(1);
                        if (_options.skipCamAnim) timeline->setStart(timeline->getDuration() - std::chrono::duration<uint64_t, std::nano>(1));
                    }
                    trackedAnimations[i] = timeline;
                }
            }
        }

        // Setup timeline pointers for snapshots
        for (int i = 0; i < SnapshotViews::COUNT; i++) {
            auto & sv         = views[i];
            sv.cameraTimeline = trackedAnimations[sv.cameraAnimation];
            if (_options.outputVideo) // add reference timelines for automatic snapshot triggering
            {
                auto & snapshot  = snapshots[i];
                currSnapshots[i] = snapshots[i];
                if (!snapshot.enabled) continue;
                snapshot.timeline = trackedAnimations[snapshot.animation];
                currSnapshots[i]  = snapshots[i];
            }
        }

        // Update subsurface info
        if (ptMode == Options::RenderPackMode::FAST_PT) {
            ptConfig.gaussV         = 0.02f;
            ptConfig.emissionScalar = 0.9f;
            ptConfig.sssamtScalar   = 1.1f;
            ptConfig.nChance        = 1.0f;
        } else {
            ptConfig.gaussV         = -0.001f;
            ptConfig.emissionScalar = 0.1f;
            ptConfig.sssamtScalar   = 6.f;
            ptConfig.nChance        = 0.8f;
        }

        // Wing is not subsurface but we can set up IOR/SSS the same way
        auto        textureFolder = modelFolder / "scene-texture";
        std::string texExt        = (ptMode == Options::RenderPackMode::FAST_PT) ? "ktx2" : "tga";
        _wingSsc                  = {"Elf_01_wing", 0.0f, 0.0f, {0.51f, 0.52f, 0.94f}, {0.0f, 0.0f, 0.0f}, "", false};
        _wingSsc.setSubsurfaceMaterial(scene, textureCache);
        _bodySsc = {
            "Elf_01", 1.0f, 1.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, (textureFolder / "Elf_01_SSS.").string() + texExt, false,
        };
        _bodySsc.setSubsurfaceMaterial(scene, textureCache);
        _lotusSsc = {
            "hh_material", 1.0f, 1.0f, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, (textureFolder / "Lotus_SSSColor.").string() + texExt, true,
        };
        _lotusSsc.setSubsurfaceMaterial(scene, textureCache);
        _leafSsc = {
            "hy_material", 1.0f, 1.0f, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, (textureFolder / "leaf_SSSColor.").string() + texExt, true,
        };
        _leafSsc.setSubsurfaceMaterial(scene, textureCache);

        // Grab lake material to set up uv animation later
        ph::rt::Blob<ph::rt::Material *> mats = scene->materials();
        for (auto mat : mats) {
            if (mat->name == "lake") {
                lakeMat = mat;
                break;
            }
        }

        // Setup animations and play initial camera animation
        if ((_options.outputVideo == 2) && (_options.cameraAnimation >= 0)) {
            // When outputting video scene-by-scene,
            // skip animations and go straight to playing camera animations.
            idleEnabled       = true;
            userCameraEnabled = true;
            activeView        = _options.cameraAnimation;
        } else
            playCameraAnimation(SnapshotViews::CHARACTER);
        setupHeadTurnParams();
        setFirstPersonToSceneCamera(false); // setup fpcontroller at beginning for debug purposes
        // set the bounds for the scene
        setBounds(bbox);
        createPipelines();
        setupShadowRenderPack();
        addSkybox(0.0f);
    }

    ~PathTracerDemo() {
        auto & vgi = dev().vgi();
        ph::va::threadSafeDeviceWaitIdle(vgi.device);
        vgi.safeDestroy(_flashPipeline);
        vgi.safeDestroy(_pipelineLayout);
    }

private:
    float lerpAngle(float prevAngle, float targetAngle) {
        float distance = targetAngle - prevAngle;
        float t        = 1.f;
        float fTime    = ((float) app().gameTime().sinceLastUpdate.count()) / 1000000.0f;
        if (std::abs(distance) > 0.01f) t = fTime * 1.0f / lookDelay;
        lerpT = t;
        return prevAngle + t * distance;
    }

    void jointLookAt(LookAtParams & laParams, Eigen::Vector3f camPos, bool isSetup = false) {
        rt::NodeTransform localToWorld = (laParams.origParentToWorld * laParams.origLocalToParent);
        Eigen::Matrix4f   worldToLocal = localToWorld.matrix4f().inverse();
        Eigen::Vector4f   camPosLocal  = (worldToLocal * Eigen::Vector4f(camPos.x(), camPos.y(), camPos.z(), 1.0f));
        Eigen::Vector3f   lookAtLocal(camPosLocal.x(), camPosLocal.y(), camPosLocal.z());
        lookAtLocal.normalize();

        // The local Z axis captures the horizontal displacement of the camera from the local forward vector.
        // arcsin converts this displacement into an angle of rotation from the forward vector.
        // we're using z as a stand-in for the projection of lookAtLocal onto the z axis.
        float calculatedTheta = std::asin(lookAtLocal.z()) + laParams.thetaOffset;
        // The local X axis is negative in front of the face and positive behind the face.

        bool camIsBehind = lookAtLocal.x() > 0.f;
        if (laParams.isNeck) {
            if (camIsBehind) {
                if (calculatedTheta < 0.f) // slightly less than PI / 2 to help lerps
                    calculatedTheta = -PI / 2.2f;
                else
                    calculatedTheta = PI / 2.2f;
            }
            if (isSetup) {
                theta = targetTheta = calculatedTheta;
            } else {
                targetTheta     = calculatedTheta;
                theta           = lerpAngle(theta, targetTheta);
                calculatedTheta = theta;
            }
        }

        rt::NodeTransform  localTfm = laParams.origLocalToParent;
        Eigen::Quaternionf localRotation;
        localTfm.decompose(nullptr, &localRotation, nullptr);
        Eigen::Vector3f    localAxis           = laParams.thetaAxis.normalized();
        Eigen::Quaternionf incrementalRotation = Eigen::Quaternionf(Eigen::AngleAxisf(calculatedTheta, localAxis));
        localRotation                          = incrementalRotation * localRotation;

        // Recalculate local space after horizontal rotation
        localTfm.setRotation(localRotation);

        // Apply vertical rotation
        localToWorld = laParams.origParentToWorld * localTfm;
        worldToLocal = localToWorld.matrix4f().inverse();
        camPosLocal  = (worldToLocal * Eigen::Vector4f(camPos.x(), camPos.y(), camPos.z(), 1.0f));
        lookAtLocal  = Eigen::Vector3f(camPosLocal.x(), camPosLocal.y(), camPosLocal.z()).normalized();

        // The local Y axis captures the displacement of the camera from the local forward vector.
        // The face is rotated up from the forward vector by default, so the offset corrects for this.
        float calculatedPhi = std::asin(laParams.phiScalar * lookAtLocal.y());
        if (laParams.isNeck) {
            if (camIsBehind)
                calculatedPhi += laParams.phiOffset * std::max(1.f - abs(lookAtLocal.x()), 0.f);
            else
                calculatedPhi += laParams.phiOffset;
            if (isSetup) {
                phi = targetPhi = calculatedPhi;
            } else {
                targetPhi     = calculatedPhi;
                phi           = lerpAngle(phi, targetPhi);
                calculatedPhi = phi;
            }
        } else {
            calculatedPhi += laParams.phiOffset;
        }

        localAxis           = localAxis.cross(lookAtLocal).normalized();
        incrementalRotation = Eigen::Quaternionf(Eigen::AngleAxisf(calculatedPhi, localAxis));
        localRotation       = incrementalRotation * localRotation;

        localTfm.setRotation(localRotation);
        laParams.joint->setTransform(localTfm);
    }

protected:
    void overrideAnimations() override {
        if (idleEnabled && !_options.enableIdle) {
            PH_REQUIRE(neckParams.joint);
            PH_REQUIRE(leftEyeParams.joint);
            PH_REQUIRE(rightEyeParams.joint);
            Eigen::Vector3f camPos;
            ph::rt::NodeTransform(cameras[selectedCameraIndex].node->worldTransform()).decompose(&camPos, nullptr, nullptr);
            jointLookAt(neckParams, camPos);

            // Eyes require updated transform from neck turn
            // right
            rightEyeParams.origParentToWorld = rightEyeParams.joint->parent()->worldTransform();
            jointLookAt(rightEyeParams, camPos);

            // left
            leftEyeParams.origParentToWorld = leftEyeParams.joint->parent()->worldTransform();
            jointLookAt(leftEyeParams, camPos);
        }

        if (idleEnabled && dropletNode) { // hide second droplet
            ph::rt::NodeTransform tfm = dropletNode->worldTransform();
            tfm.setScaling(Eigen::Vector3f::Zero());
            dropletNode->setWorldTransform(tfm);
        }
    }

public:
    void update() override {
        ModelViewer::update();

        rightEyeParams.thetaOffset = debugThetaOffset;
        rightEyeParams.phiOffset   = debugPhiOffset;
        rightEyeParams.thetaAxis   = rThetaAxis;
        leftEyeParams.thetaAxis    = lThetaAxis;

        using namespace std::chrono_literals;
        // do this at the beginning of update
        // so that the snapshot save-out doesn't happen until the frame after
        // the accumulated frame is copied back.
        saveSnapshotAccumComplete();

        enterIdleIfNeeded();

        if (!userCameraEnabled) {
            if (activeView < SnapshotViews::COUNT && views[activeView].cameraTimeline->getPlayCount() > 0) {
                userCameraEnabled = true;
                setFirstPersonToSceneCamera(!((bool) _options.outputVideo));
            }
        }

        // If camera thresholds are exceeded, disable keypresses for fpcontroller
        if (!debugPt) {
            firstPersonController.setMinimalOrbitalRadius(minOrbitalRadius);
            firstPersonController.setMaximalOrbitalRadius(maxOrbitalRadius);
            firstPersonController.setRollLimits(Eigen::Vector2f(minCameraPhi, maxCameraPhi));
            firstPersonController.setPitchLimits(Eigen::Vector2f(minCameraTheta, maxCameraTheta));
        } else {
            firstPersonController.setMinimalOrbitalRadius(0.f);
            firstPersonController.setMaximalOrbitalRadius(FLT_MAX);
            firstPersonController.setRollLimits(Eigen::Vector2f(-HALF_PI, HALF_PI));
            firstPersonController.setPitchLimits(Eigen::Vector2f(-HALF_PI, HALF_PI));
        }
    }

    void setSnapshot(bool b) { snapshot = b; }
    void pulseSnapshot() {
        setSnapshot(true);
        snapshotMicroseconds = std::chrono::microseconds::zero();
    }

    void saveSnapshotAccumComplete() {
        if (saveSnapshotWhenReady && (recordParameters.accum == RayTracingRenderPack::Accumulation::RETAIN)) {
            threadSafeDeviceWaitIdle(dev().vgi().device);
            saveSnapshotWhenReady = false;
            ph::RawImage snapshot = readBaseImagePixels(dev().graphicsQ(), _accumulatedImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                        sw().initParameters().colorFormat, sw().initParameters().width, sw().initParameters().height);
            std::time_t  now      = std::time(0);
            std::tm      localnow;
#if PH_MSWIN
            if (localtime_s(&localnow, &now)) {
#else
            if (localtime_r(&now, &localnow)) {
#endif
                char nameBuffer[80];
                std::strftime(nameBuffer, 80, "ptdemo_%Y_%B_%d_%H_%M_%S.jpg", &localnow);
                std::string outputImageName = (imageSavePath / std::string(nameBuffer)).string();
                snapshot.save(outputImageName, 0, 0);
            }
        }
    }

    void saveSnapshot() {
        if (recordParameters.accum != RayTracingRenderPack::Accumulation::OFF) saveSnapshotWhenReady = true;
    }

    void recordOffscreenPass(const PassParameters & p) override {
        copySkybox();
        if (!debugPt) {
            bool beginSnapshot = snapshot && (options.rpmode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE);

            if (snapshot) {
                // Switch from one rpmode to the other
                if (beginSnapshot) {
                    options.rpmode   = ptMode;
                    options.animated = false;
                    setAnimated(false);

                    // Reset accumulation
                    recordParameters.accum = RayTracingRenderPack::Accumulation::OFF;
                    _lastCameraPosition    = Eigen::Vector3f::Constant(Eigen::Infinity);
                    _lastCameraRotation    = Eigen::Vector3f::Constant(Eigen::Infinity);
                } else {
                    options.rpmode   = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE;
                    options.animated = true;
                    setAnimated(true);
                }

                _renderPackDirty = true;

                recreateMainRenderPack();

                setSnapshot(false);
            }
        }
        recordParameters.commandBuffer = p.cb;
        recordParameters.scene         = scene;

        ModelViewer::recordOffscreenPass(p);

        // constantly update pipelines for required renderpacks.
        // this ensures that all packs are up to date and there will be no stutters when switching
        noiseFreeRenderPack->preloadPipelines(recordParameters);
        snapshotRenderPack->preloadPipelines(recordParameters);

        // This needs to happen after modelviewer's accumulation updates
        // TODO: clean this nasty code up
        if (_options.outputVideo == 1) {
            if (snapshotInProgress) {
                snapshotMicroseconds += app().gameTime().sinceLastUpdate;
                if (recordParameters.accum == RayTracingRenderPack::Accumulation::RETAIN) {
                    float snapshotSeconds = (float) snapshotMicroseconds.count() / 1000000.0f;
                    if (snapshotSeconds >= snapshotDelay - options.accum) {
                        pulseSnapshot();
                        snapshotInProgress = false;
                    }
                } else if (recordParameters.accum == RayTracingRenderPack::Accumulation::OFF) {
                    float snapshotSeconds = (float) snapshotMicroseconds.count() / 1000000.0f;
                    if (snapshotSeconds >= snapshotDelay) // 1s delay before triggering snapshot
                    {
                        pulseSnapshot();
                    }
                }
            } else {
                bool isAnyEnabled = false;
                for (int i = 0; i < SnapshotViews::COUNT; i++) {
                    auto & snapshot = currSnapshots[i];
                    if (snapshot.enabled) {
                        isAnyEnabled = true;
                        if ((snapshot.timeline->getTime() >= snapshot.time) && (snapshot.timeline->getPlayCount() >= snapshot.playCount)) {
                            snapshot.enabled = false;
                            if (i == (int) SnapshotViews::CHARACTER)
                                setCameraToSnapshotView(i);
                            else
                                playCameraAnimation((SnapshotViews) i);
                            snapshotInProgress = true;
                            // TODO cleanup accumulation updating.
                            // Currently since camera is reset on the first frame of snapshot,
                            // accum is off for one frame before switching to on.
                            // This causes ptdemo to re-trigger snapshots over and over.
                            // We're working around it by resetting the snapshot delay for delayed snapshots,
                            // but there's probably a better way.
                            if (snapshot.delaySnap)
                                snapshotMicroseconds = std::chrono::microseconds::zero();
                            else
                                pulseSnapshot();
                            break;
                        }
                    }
                }
                if (!isAnyEnabled) {
                    if (isDay) {
                        setCameraToSnapshotView(SnapshotViews::CHARACTER);
                        toggleDayNight();
                        for (int i = 0; i < SnapshotViews::COUNT; i++) currSnapshots[i] = snapshots[i]; // reset to original snapshot list
                        auto & elfAnim    = currSnapshots[SnapshotViews::CHARACTER];                    // switch to firefly animation for night
                        elfAnim.animation = TrackedAnimations::ELF_FIREFLY;
                        elfAnim.timeline  = trackedAnimations[elfAnim.animation];
                        elfAnim.time      = std::chrono::duration<uint64_t, std::nano>(18300000000);
                        elfAnim.delaySnap = true;
                    } else {
                        finishedVideo = true;
                    }
                }
            }
        } else if (_options.outputVideo == 2) {
            if (snapshotInProgress) {
                if (userCameraEnabled && idleEnabled) {
                    if (animated()) snapshotMicroseconds += app().gameTime().sinceLastUpdate;
                    float snapshotSeconds = (float) snapshotMicroseconds.count() / 1000000.0f;
                    if (snapshotSeconds >= snapshotDelay) {
                        // play next animation
                        snapshotInProgress = false;
                    }
                }
            } else if (userCameraEnabled && idleEnabled) {
                bool isAnyEnabled = false;
                for (int i = 0; i < SnapshotViews::COUNT; i++) {
                    auto & snapshot = currSnapshots[i];
                    if (snapshot.enabled) {
                        isAnyEnabled = true;
                        // skip walk animation at night
                        if (!((i == SnapshotViews::CHARACTER) && !isDay)) playCameraAnimation((SnapshotViews) i);
                        snapshot.enabled     = false;
                        snapshotMicroseconds = std::chrono::microseconds::zero();
                        snapshotInProgress   = true;
                        break;
                    }
                }
                if (!isAnyEnabled) {
                    if (isDay) {
                        toggleDayNight();
                        if (_options.cameraAnimation < 0) {
                            activeView = SnapshotViews::CHARACTER;
                            setFirstPersonToSceneCamera(true);
                        } else
                            idleEnabled = !views[activeView].restartActiveAnims; // skip waiting for animations if not needed
                        // playCameraAnimation(SnapshotViews::CHARACTER);
                        for (int i = 0; i < SnapshotViews::COUNT; i++) currSnapshots[i] = snapshots[i]; // reset to original snapshot list
                    } else
                        finishedVideo = true;
                }
            }
        }
    }
    void recordMainColorPass(const PassParameters & p) override {
        ModelViewer::recordMainColorPass(p);

        if (debugPt || (_options.outputVideo > 1) || (_options.skipCamAnim)) return; // don't flash while debugging path tracer or generating full-pt video

        // add flashing effect only when the path tracer is accumulating.
        if (!(ph::rt::World::RayTracingRenderPackCreateParameters::isStochastic(options.rpmode)) ||
            (recordParameters.accum == RayTracingRenderPack::Accumulation::RETAIN))
            return;

        // Add flash effect
        auto       width    = sw().initParameters().width;
        auto       height   = sw().initParameters().height;
        VkViewport viewport = {0, 0, (float) width, (float) height, 0.0f, 1.0f};
        auto       scissor  = ph::va::util::rect2d(width, height, 0, 0);
        vkCmdSetViewport(p.cb, 0, 1, &viewport);
        vkCmdSetScissor(p.cb, 0, 1, &scissor);
        vkCmdBindPipeline(p.cb, VK_PIPELINE_BIND_POINT_GRAPHICS, _flashPipeline);
        auto factor = 1.f - _accumProgress;
        factor      = std::min(_options.flashDuration * factor * factor, 1.f);
        float f4[]  = {factor, factor, factor, 1.f};
        vkCmdSetBlendConstants(p.cb, f4);
        vkCmdDraw(p.cb, 3, 1, 0, 0); // draw single triangle that covers the whole screen.
    }

    void onKeyPress(int key, bool down) override {
        // Only allow camera movement/other functions in real-time mode
        if (debugPt) {
            ModelViewer::onKeyPress(key, down);
        } else if (options.rpmode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE) {
            auto & io = ImGui::GetIO();
            if (io.WantCaptureMouse) return;

// Pared-down set of keypresses to allow only zooming and orbiting
#if !defined(__ANDROID__)
            auto k = FirstPersonController::INVALID_KEY;
            // Constraints must be prepared with checks in update() to reset appropriate keys in fpcontroller
            // even when a keypress is not registered.
            switch (key) {
            case GLFW_KEY_S:
                k = FirstPersonController::MOVE_B;
                break;
            case GLFW_KEY_W:
                k = FirstPersonController::MOVE_F;
                break;
            case GLFW_KEY_LEFT:
                k = FirstPersonController::TURN_L;
                break;
            case GLFW_KEY_RIGHT:
                k = FirstPersonController::TURN_R;
                break;
            case GLFW_KEY_UP:
                k = FirstPersonController::TURN_D;
                break;
            case GLFW_KEY_DOWN:
                k = FirstPersonController::TURN_U;
                break;
            case GLFW_KEY_A:
                k = FirstPersonController::TURN_L;
                break;
            case GLFW_KEY_D:
                k = FirstPersonController::TURN_R;
                break;
            }
            firstPersonController.onKeyPress(k, down);

            // Update scene controls.
            if (!down) {
                switch (key) {
                    break;
                case GLFW_KEY_SPACE:
                    toggleAnimated();
                    break;
                case GLFW_KEY_C:
                    togglePrimaryCamera();
                    break;
                case GLFW_KEY_P:
                    pulseSnapshot();
                    break;
                case GLFW_KEY_N:
                    toggleDayNight();
                    break;
                };
            }
#else
            // Use inputs to avoid warning/errors
            key = (int) down;
#endif
        }

#if !defined(__ANDROID__)
        else if (!debugPt && (options.rpmode == ptMode)) {
            if (!down && (key == GLFW_KEY_P)) { pulseSnapshot(); }
            if (!down && (key == GLFW_KEY_N)) { toggleDayNight(); }
            // todo add save image control
        }
#endif
    }

    void addSkybox(float lodBias = 0.0f) override {
        if (!mainColorPass()) {
            PH_THROW("Color pass is not created yet. Are you try calling addSkybox() in you scene's constructor?"
                     "Since skybox depends on swapchain, the best place to call it is inside the resize() method.");
        }
        if (dynReflMap && dynDiffMap) {
            // Need to update environment map parameters too.
            recordParameters.irradianceMap = dynDiffMap;
            recordParameters.reflectionMap = dynReflMap;
            recordParameters.ambientLight  = {0.f, 0.f, 0.f};
        } else {
            recordParameters.irradianceMap = {};
            recordParameters.reflectionMap = {};
            recordParameters.ambientLight  = {0.2f, 0.2f, 0.2f};
        }

        Skybox::ConstructParameters cp = {loop(), *assetSys};
        cp.width                       = sw().initParameters().width;
        cp.height                      = sw().initParameters().height;
        cp.pass                        = mainColorPass();
        cp.skymap                      = recordParameters.reflectionMap;
        cp.skymapType                  = Skybox::SkyMapType::CUBE;
        skybox.reset(new Skybox(cp));
        skyboxLodBias = lodBias;
    }

protected:
    void doAccumulationComplete(VkCommandBuffer cb) override {
        ph::va::setImageLayout(cb, recordParameters.targetImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        ph::va::setImageLayout(cb, _accumulatedImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        VkImageCopy copyRegion = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                  {0, 0, 0},
                                  {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                  {0, 0, 0},
                                  {sw().initParameters().width, sw().initParameters().height, 1}};
        vkCmdCopyImage(cb, recordParameters.targetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _accumulatedImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                       &copyRegion);
        ph::va::setImageLayout(cb, recordParameters.targetImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    }

    void drawUI() override {
#if defined(__ANDROID__)
        // values for 480p: buttonDim = 34, right-side window pos x = 385
        // values for 720p: buttonDim = 44, right-side window pos x = 650
        // values for 601P: buttonDim = 36, right-side window pos x = 520
        // must match dimensions hardcoded in hub/app/src/main/cpp/app.h
        bool   alwaysOpen = true;
        ImVec2 margins    = ImVec2(5, 5);
        int    buttonDim  = 44;
        ImVec2 buttonDims = ImVec2((float) 100, (float) buttonDim);
        // custom UI for side branch only
        // Left-side buttons
        ImGui::SetNextWindowPos(margins);
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.f);
        if (ImGui::Begin("Camera Views", &alwaysOpen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
            for (int i = 0; i < SnapshotViews::COUNT; i++)
                if (ImGui::Button(views[i].name.c_str(), buttonDims)) { playCameraAnimation((SnapshotViews) i); }
        }
        ImGui::End();

        // Right-side buttons
        // for some reason imgui width seems to be 480....?
        ImGui::SetNextWindowPos(ImVec2(650.f, 2.f)); // ImVec2((float)w - (buttonDims[0] + margins[0]), margins[1]));
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.f);
        if (ImGui::Begin("Features", &alwaysOpen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
            bool         isAnimating      = animated();
            const char * snapshotLabel    = isAnimating ? "Snapshot" : "Save";
            const char * dayNightLabel    = isDay ? "Night" : "Day";
            const char * pauseResumeLabel = isAnimating ? "Pause" : "Resume";
            if (ImGui::Button(snapshotLabel, ImVec2((float) 100, buttonDim * 2.f))) {
                if (isAnimating)
                    pulseSnapshot();
                else
                    saveSnapshot();
            }
            if (ImGui::Button(dayNightLabel, buttonDims)) {
                if (isAnimating) toggleDayNight();
            }
            if (ImGui::Button(pauseResumeLabel, buttonDims)) {
                if (isAnimating || (options.rpmode == ModelViewer::Options::RenderPackMode::NOISE_FREE))
                    toggleAnimated();
                else {
                    // the proper way of resuming from snapshot is via pulseSnapshot()
                    pulseSnapshot();
                }
            }
        }
        ImGui::End();
#else
        ModelViewer::drawUI();
#endif
    }

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
                    options.rpmode           = ptMode;
                    _renderPackDirty         = true;
                } else {
                    setAnimated(true);
                    pathRayTracingRenderPack = noiseFreeRenderPack;
                    options.rpmode           = ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE;
                    _renderPackDirty         = true;
                }
            }

            if (ImGui::BeginTable("Snapshot Views", SnapshotViews::COUNT)) {
                for (int i = 0; i < SnapshotViews::COUNT; i++) {
                    auto & v = views[i];
                    ImGui::TableNextColumn();
                    if (ImGui::RadioButton(v.name.c_str(), activeView == i)) { playCameraAnimation((SnapshotViews) i); }
                }
                ImGui::EndTable();
            }

            if (ImGui::SliderFloat("Subsurface Intensity", &_bodySsc.scaling, 0.001f, 100.0f)) {
                _lotusSsc.scaling = _bodySsc.scaling;
                _bodySsc.setSubsurfaceMaterial(scene, textureCache);
                _lotusSsc.setSubsurfaceMaterial(scene, textureCache);
            }

            //////////////////////////////////////////////
            // Debug UI for used during demo development
            if (false) {
                if (ImGui::TreeNode("Procedural Head Turn Debug")) {
                    ImGui::SliderFloat("Look Delay", &lookDelay, 0.01f, 3.f);
                    ImGui::SliderFloat("Debug Offset (theta): %f", &debugThetaOffset, -2.f, 2.f);
                    ImGui::SliderFloat("Debug Offset (phi): %f", &debugPhiOffset, -2.f, 2.f);
                    if (ImGui::TreeNode("Neck")) {
                        ImGui::Text("theta, targetTheta, toCam.z: %f, %f, %f", theta, targetTheta, localToCam.z());
                        ImGui::Text("phi, targetTheta, toCam.y: %f, %f, %f", phi, targetPhi, localToCam.y());
                        ImGui::Text("toCam.x, t: %f, %f", localToCam.x(), lerpT);
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Left Eye")) {
                        ImGui::Text("theta, toCam.z: %f, %f", lTheta, lLocalToCam.z());
                        ImGui::Text("phi, toCam.y: %f, %f", lPhi, lLocalToCam.y());
                        ImGui::Text("toCam.x: %f", lLocalToCam.x());
                        float yx = lThetaAxis.x();
                        float yy = lThetaAxis.y();
                        float yz = lThetaAxis.z();
                        if (ImGui::SliderFloat("Theta Axis X", &yx, -1.f, 1.f)) { lThetaAxis = Eigen::Vector3f(yx, yy, yz); }
                        if (ImGui::SliderFloat("Theta Axis Y", &yy, -1.f, 1.f)) { lThetaAxis = Eigen::Vector3f(yx, yy, yz); }
                        if (ImGui::SliderFloat("Theta Axis Z", &yz, -1.f, 1.f)) { lThetaAxis = Eigen::Vector3f(yx, yy, yz); }
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Right Eye")) {
                        ImGui::Text("theta, toCam.z: %f, %f", rTheta, rLocalToCam.z());
                        ImGui::Text("phi, toCam.y: %f, %f", rPhi, rLocalToCam.y());
                        ImGui::Text("toCam.x: %f", rLocalToCam.x());
                        float yx = rThetaAxis.x();
                        float yy = rThetaAxis.y();
                        float yz = rThetaAxis.z();
                        if (ImGui::SliderFloat("Theta Axis X", &yx, -1.f, 1.f)) { rThetaAxis = Eigen::Vector3f(yx, yy, yz); }
                        if (ImGui::SliderFloat("Theta Axis Y", &yy, -1.f, 1.f)) { rThetaAxis = Eigen::Vector3f(yx, yy, yz); }
                        if (ImGui::SliderFloat("Theta Axis Z", &yz, -1.f, 1.f)) { rThetaAxis = Eigen::Vector3f(yx, yy, yz); }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
            }

            // Debug lights
            if (false) {
                Eigen::Vector3f center     = sceneCenter;
                float           handedness = 1.0f;
                for (size_t i = 0; i < lights.size(); ++i) {
                    auto               light = lights[i];
                    Eigen::Vector3f    position;
                    Eigen::Quaternionf origRotation;
                    ph::rt::NodeTransform(light->nodes()[0]->worldTransform()).decompose(&position, &origRotation, nullptr);
                    ImGui::Text("position: %f, %f, %f", position.x(), position.y(), position.z());
                    ImGui::Text("rotation: %f, %f, %f, %f", origRotation.x(), origRotation.y(), origRotation.z(), origRotation.w());

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
                        ImGui::SliderFloat("phi", &height, -HALF_PI + 0.01f, HALF_PI - 0.01f);
                        ImGui::SliderFloat("theta", &rotation, 0.f, 2.f * PI);
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
                    light->nodes()[0]->setWorldTransform(tfm);
                }
            }

            // Debug cameras
            if (false) {
                ImGui::Text("Scene center: %f, %f, %f", sceneCenter.x(), sceneCenter.y(), sceneCenter.z());
                ImGui::Text("Active Camera: %d", (int) selectedCameraIndex);
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
                        if (ImGui::SliderFloat("mouse move sensitivity", &mouseMoveSensitivity, 0.0001f, 0.3f))
                            firstPersonController.setMouseMoveSensitivity(mouseMoveSensitivity);
                        if (ImGui::SliderFloat("mouse wheel sensitivity", &mouseWheelSensitivity, 0.0001f, 1.0f))
                            firstPersonController.setMouseWheelSensitivity(mouseWheelSensitivity);
                        ImGui::TreePop();
                    }
                    for (size_t i = 0; i < cameras.size(); ++i) {
                        auto & c = cameras[i];
                        if (ImGui::TreeNode(formatstr("Camera %zu", i))) {
                            Eigen::Vector3f    p;
                            Eigen::Quaternionf r;
                            ph::rt::NodeTransform(c.node->worldTransform()).decompose(&p, &r, nullptr);
                            ImGui::Text("position: %f, %f, %f", p.x(), p.y(), p.z());
                            ImGui::Text("rotation: %f, %f, %f, %f", r.x(), r.y(), r.z(), r.w());
                            ImGui::SliderFloat("znear", &c.zNear, 0.00001f, 0.1f);
                            ImGui::Text("znear: %f, zfar: %f, yfov: %f", c.zNear, c.zFar, c.yFieldOfView);
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }

            // End debug demo UI
            /////////////////////

            bool         isAnimating = animated();
            const char * buttonLabel = isAnimating ? "Snapshot" : "Save";
            if (ImGui::SmallButton(buttonLabel)) {
                if (isAnimating)
                    pulseSnapshot();
                else
                    saveSnapshot();
            }
            const char * dayNightLabel = isDay ? "Night" : "Day";
            if (ImGui::SmallButton(dayNightLabel)) {
                if (isAnimating) toggleDayNight();
            }
            const char * pauseResumeLabel = isAnimating ? "Pause" : "Resume";
            if (ImGui::SmallButton(pauseResumeLabel)) {
                if (isAnimating || (options.rpmode == ModelViewer::Options::RenderPackMode::NOISE_FREE))
                    toggleAnimated();
                else {
                    // the proper way of resuming from snapshot is via pulseSnapshot()
                    pulseSnapshot();
                }
            }
            ImGui::TreePop();
        }
    }

    void recreateMainRenderPack() override {
        _renderPackDirty = false;
        if (pathRayTracingRenderPack == snapshotRenderPack || pathRayTracingRenderPack == noiseFreeRenderPack)
            pathRayTracingRenderPack = nullptr; // let ptdemo manage these two render packs

        // Check for resize
        // TODO: resizing code crashes. fix this after that's been fixed
        auto newW    = app().sw().initParameters().width;
        auto newH    = app().sw().initParameters().height;
        bool resized = (newW != _renderTargetSize.width || newH != _renderTargetSize.height);

        if (resized || !snapshotRenderPack || !noiseFreeRenderPack || !debugPtRenderPack || _renderPackDirty) recreateRenderPacks();

        if (options.rpmode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE)
            pathRayTracingRenderPack = noiseFreeRenderPack;
        else if (options.rpmode == ptMode) {
            if (debugPt)
                pathRayTracingRenderPack = debugPtRenderPack;
            else
                pathRayTracingRenderPack = snapshotRenderPack;
        } else {
            pathRayTracingRenderPack = nullptr; // don't delete a render pack that we're using
            ModelViewer::recreateMainRenderPack();
        }
        setupLights();
    }

private:
    void recreateRenderPacks() {
        auto w  = sw().initParameters().width;
        auto h  = sw().initParameters().height;
        auto cp = World::RayTracingRenderPackCreateParameters {ptMode}
                      .setTarget(sw().initParameters().colorFormat, w, h, VK_IMAGE_LAYOUT_UNDEFINED)
                      .setViewport(0.f, 0.f, (float) w, (float) h)
                      .setClear(true);
        if (noiseFreeRenderPack || snapshotRenderPack) {
            threadSafeDeviceWaitIdle(dev().vgi().device);
            if (noiseFreeRenderPack) world->deleteRayTracingRenderPack(noiseFreeRenderPack);
            if (snapshotRenderPack) world->deleteRayTracingRenderPack(snapshotRenderPack);
            if (debugPtRenderPack) // todo: disable in release?
                world->deleteRayTracingRenderPack(debugPtRenderPack);
        }
        PH_ASSERT(!noiseFreeRenderPack);
        PH_ASSERT(!snapshotRenderPack);

        cp.targetIsSRGB     = true;
        snapshotRenderPack  = world->createRayTracingRenderPack(cp);
        debugPtRenderPack   = world->createRayTracingRenderPack(cp);
        cp.mode             = {ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE};
        noiseFreeRenderPack = world->createRayTracingRenderPack(cp);

        options.rpmode           = (_options.outputVideo == 2) ? ptMode : ph::rt::World::RayTracingRenderPackCreateParameters::Mode::NOISE_FREE;
        pathRayTracingRenderPack = (_options.outputVideo == 2) ? snapshotRenderPack : noiseFreeRenderPack;

        recordParameters.spp                = options.spp;
        recordParameters.maxDiffuseBounces  = options.diffBounces;
        recordParameters.maxSpecularBounces = options.specBounces;
        recordParameters.spp                = options.spp;
        ptConfig.setupRp(recordParameters);
    }

    ph::rt::RayTracingRenderPack * noiseFreeRenderPack = nullptr;
    ph::rt::RayTracingRenderPack * snapshotRenderPack  = nullptr;
    ph::rt::RayTracingRenderPack * debugPtRenderPack   = nullptr;

    bool snapshot = false;
    bool debugPt  = false;

    const Options _options;

    VkPipeline       _flashPipeline {};
    VkPipelineLayout _pipelineLayout {};

    PathTracerConfig::TransmissiveSSSConfig _bodySsc;
    PathTracerConfig::TransmissiveSSSConfig _wingSsc;
    PathTracerConfig::TransmissiveSSSConfig _lotusSsc;
    PathTracerConfig::TransmissiveSSSConfig _leafSsc;

    Eigen::Vector3f      sceneCenter;
    Eigen::Quaternionf * origLightRotation = nullptr;
};
