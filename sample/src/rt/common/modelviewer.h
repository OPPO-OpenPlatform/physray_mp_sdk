/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/rt-utils.h>
#include "animations/timeline.h"
#include "first-person-controller.h"
#include "scene-asset.h"
#include "scene-utils.h"
#include "skybox.h"
#include "texture-cache.h"
#include "simpleApp.h"
#include "skinning.h"
#include "morphtargets.h"
#include "pathtracerconfig.h"
#include "debug-scene-data.h"
#include "sbb.h"

#include <chrono>
#include <filesystem>

struct ModelViewer : SimpleScene {
    struct Options {
        uint32_t spp                            = 1; ///< samples per pixel per frame
        uint32_t diffBounces                    = 3;
        uint32_t specBounces                    = 5;
        uint32_t activeCamera                   = 0;     ///< index of the active camera
        int      accum                          = 2000;  ///< accumulative rendering. 0: disabled, >0: number of frames, <0: number of seconds.
        int      animated                       = 1;     ///< enable animation when app starts.
        bool     showUI                         = true;  ///< show ImGUI based in-game GUI when set to true.
        bool     showFrameTimes                 = true;  ///< show detail frame time break down in the in-game GUI.
        bool     usePrecompiledShaderParameters = false; ///< use precompiled defines instead of slower but debuggable uniforms
        bool     refractionAndRoughReflection   = true;  ///< handle refractive and rough reflective materials
        bool     clearColorOnMainPass           = false;
        bool     clearDepthOnMainPass           = false;
        bool     renderToSRGB                   = true; ///< set to false to render in linear color space.

        // Set to true to create a set of debug geometries.
        bool enableDebugGeometry = false;

        enum class RenderPackMode {
            RAST,       // rasterizer
            PT,         // path tracer
            NOISE_FREE, // noise free tracing
            SHADOW,     // ray traced shadow
            FAST_PT,    // fast path tracing
        };

        RenderPackMode rpmode = RenderPackMode::PT;

        using ShadowMode      = ph::rt::render::NoiseFreeRenderPack::ShadowMode;
        ShadowMode shadowMode = ShadowMode::RAY_TRACED;

        // Assets
        std::vector<std::string> additionalAssetFolders;
        std::string              irradianceMapAsset = "texture/skybox1/irradiance-astc.ktx2";
        std::string              reflectionMapAsset = "texture/skybox1/prefiltered-reflection-astc.ktx2";

        /// Set to true to enable left handed mode. Right handed by default.
        bool leftHanded = false;

        /// Set to true to use flythrough camera. Orbital camera is used by default.
        bool flythroughCamera = false;

        bool isPathTraced() const { return RenderPackMode::PT == rpmode || RenderPackMode::FAST_PT == rpmode; }
    };

    ModelViewer(SimpleApp & app, const Options & o);

    ~ModelViewer() {
        ph::safeDelete(graph);
        ph::safeDelete(pathTracingRenderPack);
        ph::safeDelete(noiseFreeRenderPack);
        ph::safeDelete(shadowRenderPack);
        ph::safeDelete(world);
    }

    Options                       options;
    skinning::SkinnedMeshManager  skinningManager;
    PathTracerConfig              ptConfig;
    MorphTargetManager            morphTargetManager;
    ph::AssetSystem *             assetSys   = nullptr;
    ph::rt::World *               world      = nullptr;
    ph::rt::Scene *               scene      = nullptr;
    sg::Graph *                   graph      = nullptr;
    ph::rt::Material *            lambertian = nullptr;
    ph::rt::Material *            glossy     = nullptr;
    ph::rt::Mesh *                sphereMesh = nullptr;
    ph::rt::Mesh *                circleMesh = nullptr;
    ph::rt::Mesh *                quadMesh   = nullptr;
    std::unique_ptr<TextureCache> textureCache; ///< Used to retrieve and store the images backing the textures.

    /// the debug scene manager
    std::unique_ptr<scenedebug::SceneDebugManager> debugManager;

    /// the sky box
    std::unique_ptr<Skybox> skybox;
    float                   skyboxLodBias = 0.0f;

    /// List of all cameras that can be picked from.
    /// cameras[0] is the first person camera controlled by the firstPersonController.
    std::vector<Camera>   cameras;
    std::size_t           selectedCameraIndex = 0; ///< Index of the currently selected camera.
    FirstPersonController firstPersonController;   ///< Used to control first person camera.

    /// List of all lights we have added to the scene.
    /// Used to render shadow maps for lights.
    std::vector<sg::Node *> lights;

    // Used to update the scene.
    ph::rt::render::PathTracingRenderPack *                 pathTracingRenderPack = nullptr;
    ph::rt::render::PathTracingRenderPack::RecordParameters recordParameters      = {};

    // Noise free render pack
    ph::rt::render::NoiseFreeRenderPack *                 noiseFreeRenderPack = nullptr;
    ph::rt::render::NoiseFreeRenderPack::RecordParameters noiseFreeParameters = {};

    // shadow map data members
    ph::rt::render::ShadowMapRenderPack *                 shadowRenderPack = nullptr;
    ph::rt::render::ShadowMapRenderPack::RecordParameters shadowParameters = {};
    // TODO: use nearest sampler to sample 32 bit float
    const VkFormat shadowMapFormat = VK_FORMAT_R16_SFLOAT;
    const uint32_t shadowMapSize   = 512;

    /// list of all loaded textures. this is to make it possible to reuse loaded texture object.
    /// TODO: replace ph::RawImage with std::future<ph::RawImage> for async loading.
    std::map<std::string, ph::RawImage> imageAssets;

    /// Animations being played.
    std::vector<std::shared_ptr<::animations::Timeline>> animations;

    struct PassParameters {
        VkCommandBuffer                             cb {};
        const ph::va::SimpleSwapchain::BackBuffer & bb;
        VkImageView                                 depthView {};
        // ShadowRenderOptions                 shadowOptions;
    };

    void resized() override;

    void update() override;

    virtual void resetScene();

    /// the main entry point of recording rendering commands.
    VkImageLayout record(const SimpleRenderLoop::RecordParameters &) override;

    /// Custom render pass(es) called before the main render pass. Override this method
    /// to do customized rendering to offscreen framebuffers.
    /// TODO: this is obsolete concept, should be merged into recordMainColorPass()
    virtual void recordOffscreenPass(const PassParameters &);
    virtual void recordShadowMap(const PassParameters &);
    virtual void recordPathTracer(const PassParameters &);
    virtual void recordNoiseFree(const PassParameters &);

    /// Main color pass. Render to default frame buffer.
    virtual void recordMainColorPass(const PassParameters &);

    void toggleShadowMode() {
        constexpr int count            = (int) ph::rt::render::NoiseFreeRenderPack::ShadowMode::NUM_SHADOW_MODES;
        int           newMode          = ((int) noiseFreeParameters.shadowMode + 1) % count;
        noiseFreeParameters.shadowMode = (ph::rt::render::NoiseFreeRenderPack::ShadowMode) (newMode);
    }

    /// Increments selectedCameraIndex to the next camera
    /// And picks that camera as the one to render the scene.
    void togglePrimaryCamera();

    /// Changes the selected primary camera by camera index.
    void setPrimaryCamera(size_t index);

    void onKeyPress(int key, bool down) override;
    void onMouseMove(float x, float y) override;
    void onMouseWheel(float delta) override;

    /// get the AABB extents of the current scene
    Eigen::AlignedBox3f bounds() const { return _bounds; }

protected:
    VkRenderPass mainColorPass() const { return _colorPass; }

    // set the extents of the scene
    void setBounds(const Eigen::AlignedBox3f & box) { _bounds = box; }

    /// Entry point to render ImGUI based UI.
    virtual void drawUI();

    /// Override this method to draw customize UI elements.
    virtual void describeImguiUI();

    // float getTimeBasedAccumFactor(int* accumTime = nullptr, int* accumDuration = nullptr);

    virtual void recreateMainRenderPack();

    /// camere a default camera and light based on the bounding box of the scene.
    void setupDefaultCamera(const Eigen::AlignedBox3f & bbox);

    /// Setup shadow map rendering pack. can only be called afer default render pack is setup.
    void setupShadowRenderPack();

    struct LoadOptions {
        /// path to the model asset
        std::string model;

        /// Name of the animation to play.
        std::string animation = "*"; // "*" means load all animations.

        /// The default material to use with model w/o materials.
        ph::rt::Material * defaultMaterial = nullptr;

        sg::Node * parent = nullptr;

        // If true, a geometry light is automatically created for
        // meshes using materials with nonzero emission.
        // Geometry lights are only used in path tracing, so this is
        // false by default.
        bool createGeomLights = false;
    };

    sg::Node * addPointLight(const Eigen::Vector3f & position, float range, const Eigen::Vector3f & emission, float radius = 0.0f,
                             bool enableDebugMesh = false);

    sg::Node * addDirectionalLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float brightness = 2.0f,
                                   const Eigen::Vector2f * dimensions = nullptr, bool enableDebugMesh = false);

    sg::Node * addDirectionalLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, const Eigen::Vector3f & emission,
                                   const Eigen::Vector2f * dimensions = nullptr, bool enableDebugMesh = false);

    void addCeilingLight(const Eigen::AlignedBox3f & bbox, float brightness = 2.0f, float radius = 0.0f, bool enableDebugMesh = false);

    sg::Node * addSpotLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float range, float brightness, Eigen::Vector2f cones,
                            Eigen::Vector2f dimensions, bool enableDebugMesh = false);

    sg::Node * addSpotLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float range, const Eigen::Vector3f & emission,
                            Eigen::Vector2f cones, Eigen::Vector2f dimensions, bool enableDebugMesh = false);

    Eigen::AlignedBox3f addModelToScene(const LoadOptions &);
    sg::Node *          addModelNodeToScene(const LoadOptions &, Eigen::AlignedBox3f & bbox);

    void addCornellBoxToScene(const Eigen::AlignedBox3f & bbox);

    /// Add a square floor to the scene (with lambertian material)
    /// The floor is on X-Z plane with +Y as the up/normal vector.
    /// Returns bounding box of the floor.
    Eigen::AlignedBox3f addFloorPlaneToScene(const Eigen::Vector3f & center, float dimension);

    sg::Node * addQuad(const char * name, float w, float h, ph::rt::Material * material, sg::Node * parent = nullptr,
                       const sg::Transform & transform = sg::Transform::Identity());

    sg::Node * addCircle(const char * name, float w, float h, ph::rt::Material * material, sg::Node * parent = nullptr,
                         const sg::Transform & transform = sg::Transform::Identity());

    sg::Node * addBox(const char * name, float w, float h, float d, ph::rt::Material * material, sg::Node * parent = nullptr,
                      const sg::Transform & transform = sg::Transform::Identity());

    sg::Node * addIcosahedron(const char * name, float radius, uint32_t subdivide, ph::rt::Material * material, sg::Node * parent = nullptr,
                              const sg::Transform & transform = sg::Transform::Identity());

    // Creates a simple mesh node.
    sg::Node * addMeshNode(sg::Node * parent, const sg::Transform & transform, ph::rt::Mesh * mesh, ph::rt::Material * material);

    // Add the skybox, which will be rendered in a separate pass, without using physray-sdk
    virtual void addSkybox(float lodBias = 0.0f);

    // Function that determines whether to reset accumulation for stochastic path tracing.
    virtual bool accumDirty();

    void setClusterMode(const PathTracerConfig::ClusterMode & mode) { ptConfig.clusterMode = mode; }
    void setClusterSubdivisions(uint32_t count) { ptConfig.sceneSubdivisions = static_cast<int>(count); }

    ph::rt::Mesh * createNonIndexedMesh(size_t vertexCount, const float * positions, const float * normals, const float * texcoords = nullptr,
                                        const float * tangents = nullptr);
    // accumulation
    Eigen::Vector3f                                _lastCameraPosition = Eigen::Vector3f::Zero();
    Eigen::Vector3f                                _lastCameraRotation = Eigen::Vector3f::Zero();
    size_t                                         _accumulatedFrames  = 0;
    std::chrono::high_resolution_clock::time_point _accumulationStartTime;
    float                                          _accumProgress   = 0.0;
    bool                                           _renderPackDirty = false;

public:
    virtual void setRpMode(Options::RenderPackMode rpmode) {
        _renderPackDirty = options.rpmode != rpmode;
        options.rpmode   = rpmode;
    }

protected:
    bool                _accumDirty = false;
    Eigen::AlignedBox3f transformBbox(Eigen::AlignedBox3f bbox, sg::Transform t);

    /// @brief Loads the contents of the scene asset into the model viewer.
    /// @param o Model loading options.
    /// @param sceneAsset Contains model and animation data to be displayed.
    void       loadSceneAsset(const LoadOptions & o, const SceneAsset * sceneAsset);
    VkExtent2D _renderTargetSize = {0, 0};

    std::shared_ptr<const SceneAsset> loadGltf(const LoadOptions & o);

    /// search the GLTF file within the folder.
    std::filesystem::path searchForGLTF(const std::filesystem::path folder) {
        for (auto entry : std::filesystem::directory_iterator(folder)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            std::for_each(ext.begin(), ext.end(), [](char ch) { return (char) std::tolower(ch); });
            if (ext == ".gltf" || ext == ".glb") {
                // found it.
                return entry.path();
            }
        }
        return {};
    }

    // Override this to perform transformations on joints that are inserted after joints
    // have been updated but before rendering.
    virtual void overrideAnimations() {}

    // Override this to add code that executes right after accumulation has completed.
    virtual void doAccumulationComplete(VkCommandBuffer) {}
    /// Clear color buffer to black by default.
    VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};

    /// Clear value for depth and stencil buffer.
    VkClearDepthStencilValue clearDepthStencil = {1.0f, 0};

    // Buffers for building the scene
    SceneBuildBuffers sbb;

private:
    struct FrameBuffer {
        ph::va::AutoHandle<VkFramebuffer> colorFb; ///< frame buffer with color buffer only.
        // ph::va::ImageObject               depth;
        // ph::va::AutoHandle<VkImage>       stencilView;
        // ph::va::AutoHandle<VkRenderPass>  colorDepthPass;
        // ph::va::AutoHandle<VkRenderPass>  colorDepthStencilPass;
    };

    sg::Node *                       _firstPersonNode = nullptr; /// Node that will hold the headlight and the default camera.
    Eigen::AlignedBox3f              _bounds;                    /// store the scene AABB bounds
    ph::va::AutoHandle<VkRenderPass> _colorPass;                 // render pass with only color buffer only.
    VkFormat                         _colorTargetFormat = VK_FORMAT_UNDEFINED;
    std::vector<FrameBuffer>         _frameBuffers; // one for each back buffer image
    ph::va::ImageObject              _depthBuffer;  // main depth and stencil buffer

    void recreateColorRenderPass();

    sg::Node * loadObj(const LoadOptions & o, Eigen::AlignedBox3f & bbox);

    ph::rt::Mesh * createCircle(float w, float h);
    ph::rt::Mesh * createQuad(float w, float h);
    ph::rt::Mesh * createIcosahedron(float radius, uint32_t subdivide, const float * aniso = nullptr);

    /// Adds the given model file's animations to the list of animations and plays the selected one.
    void                         addModelAnimations(const LoadOptions & o, const SceneAsset * sceneAsset);
    std::vector<Eigen::Vector3f> calculateTriangleTangents(const std::vector<Eigen::Vector3f> & normals, const float * aniso = nullptr);
};
