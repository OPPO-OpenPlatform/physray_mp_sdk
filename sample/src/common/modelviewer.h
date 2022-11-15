#pragma once
#include "animations/timeline.h"
#include "first-person-controller.h"
#include "scene-asset.h"
#include "scene-utils.h"
#include "skybox.h"
#include "texture-cache.h"
#include "vkutils.h"
#include <ph/rt.h>
#include "skinning.h"
#include "pathtracerconfig.h"
#include "debug-scene-data.h"
#include "morphtargets.h"

struct ModelViewer : SimpleScene {
    struct Options {
        int32_t  spp            = 1; ///< samples per pixel per frame
        int32_t  maxSpp         = 0; ///< total accumulated samples per pixel
        uint32_t diffBounces    = 3;
        uint32_t specBounces    = 5;
        uint32_t activeCamera   = 0;    ///< index of the active camera
        uint32_t maxFrames      = 0;    ///< quit the app after certian number of frames. 0 means infinie number of frames.
        bool     accum          = true; ///< enable accumulative rendering
        bool     animated       = true; ///< enable animation when app starts.
        bool     showUI         = false; ///< show ImGUI based in-game GUI when set to true.
        bool     showFrameTimes = true; ///< show detail frame time break down in the in-game GUI.
        bool     showDebugUI    = true; ///< show debug menu in the in-game gui.

        /// Minimum number of frames per second.
        /// If the actual frame rate is lower, then it will clamp delta
        /// time to match the minimum frame rate, preventing animations and
        /// the like from progressing too quickly.
        float minFrameRate = 10.0f;

        /// Minimum number of frames per second.
        /// If the actual frame rate is higher, then it will clamp delta
        /// time to match the maximum frame rate, preventing animations and
        /// the like from progressing too slowly.
        /// Default value is infinity, meaning this will allow frames
        /// be as short as they like.
        float maxFrameRate = std::numeric_limits<float>::infinity();

        using RenderPackMode      = ph::rt::World::RayTracingRenderPackCreateParameters::Mode;
        using ShadowMode          = ph::rt::RayTracingRenderPack::ShadowMode;
        RenderPackMode rpmode     = RenderPackMode::PATH_TRACING;
        ShadowMode     shadowMode = ShadowMode::RAY_TRACED;

        // Assets
        std::vector<std::string> additionalAssetFolders;
        std::string              irradianceMapAsset = "texture/skybox1/irradiance-astc.ktx2";
        std::string              reflectionMapAsset = "texture/skybox1/prefiltered-reflection-astc.ktx2";

        /// Set to true to enable left handed mode. Right handed by default.
        bool leftHanded = false;

        /// Set to true to use flythrough camera. Orbital camera is used by default.
        bool flythroughCamera = false;

        skinning::SkinningMode skinMode = skinning::SkinningMode::OFF;
    };

    ModelViewer(SimpleApp & app, const Options & o);

    ~ModelViewer() { ph::rt::World::deleteWorld(world), world = nullptr; }

    void resize() override;

    Options                       options;
    skinning::SkinningManager     skinningManager;
    PathTracerConfig              ptConfig;
    MorphTargetManager            morphTargetManager;
    ph::AssetSystem *             assetSys   = nullptr;
    ph::rt::World *               world      = nullptr;
    ph::rt::Scene *               scene      = nullptr;
    ph::rt::Material *            lambertian = nullptr;
    ph::rt::Material *            glossy     = nullptr;
    std::unique_ptr<TextureCache> textureCache; ///< Used to retrieve and store the images backing the textures.
    scenedebug::SceneDebugManager debugManager;

    /// the sky box
    std::unique_ptr<Skybox> skybox;
    float                   skyboxLodBias = 0.0f;

    /// List of all cameras that can be picked from.
    /// cameras[0] is the first person camera controlled by the firstPersonController.
    std::vector<ph::rt::Camera *> cameras;
    std::size_t                   selectedCameraIndex = 0; ///< Index of the currently selected camera.
    float                         defaultZFar         = 0.f;
    FirstPersonController         firstPersonController; ///< Used to control first person camera.

    /// List of all lights we have added to the scene.
    std::vector<ph::rt::Light *> lights;

    // Used to update the scene.
    ph::rt::RayTracingRenderPack *                 pathRayTracingRenderPack = nullptr;
    ph::rt::RayTracingRenderPack::RecordParameters recordParameters         = {};
    // Used to indicate pending changes to the render pack mode that we want to display
    Options::RenderPackMode targetMode = Options::RenderPackMode::PATH_TRACING;

    // shadow map data members
    ph::rt::ShadowMapRenderPack *                 shadowRenderPack = nullptr;
    ph::rt::ShadowMapRenderPack::RecordParameters shadowParameters = {};
    const VkFormat                                shadowMapFormat  = VK_FORMAT_R32_SFLOAT;
    const uint32_t                                shadowMapSize    = 512;

    /// list of all loaded textures. this is to make it possible to reuse loaded texture object.
    /// TODO: replace ph::RawImage with std::future<ph::RawImage> for async loading.
    std::map<std::string, ph::RawImage> imageAssets;

    /// Animations being played.
    std::vector<std::shared_ptr<::animations::Timeline>> animations;

    const FrameTiming & update() override;

    void prepare(VkCommandBuffer cb) override;
    void recordOffscreenPass(const PassParameters &) override;
    void recordMainColorPass(const PassParameters &) override;

    void toggleShadowMode() {
        constexpr int count         = (int) ph::rt::RayTracingRenderPack::ShadowMode::NUM_SHADOW_MODES;
        int           newMode       = ((int) recordParameters.shadowMode + 1) % count;
        recordParameters.shadowMode = (ph::rt::RayTracingRenderPack::ShadowMode) (newMode);
    }

    /// Increments selectedCameraIndex to the next camera
    /// And picks that camera as the one to render the scene.
    void togglePrimaryCamera();

    /// Changes the selected primary camera by camera index.
    void setPrimaryCamera(size_t index);

    void onKeyPress(int key, bool down) override;
    void onMouseMove(float x, float y) override;
    void onMouseWheel(float delta) override;

protected:
    void describeImguiUI() override;

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

        ph::rt::Node * parent = nullptr;
    };

    ph::rt::Light * addPointLight(const Eigen::Vector3f & position, float range, const Eigen::Vector3f & emission, float radius = 0.0f,
                                  bool enableDebugMesh = false);

    ph::rt::Light * addDirectionalLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float brightness = 2.0f,
                                        const Eigen::Vector2f * dimensions = nullptr, bool enableDebugMesh = false);

    ph::rt::Light * addDirectionalLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, const Eigen::Vector3f & emission,
                                        const Eigen::Vector2f * dimensions = nullptr, bool enableDebugMesh = false);

    void addCeilingLight(const Eigen::AlignedBox3f & bbox, float brightness = 2.0f, float radius = 0.0f, bool enableDebugMesh = false);

    ph::rt::Light * addSpotLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float range, float brightness, Eigen::Vector2f cones,
                                 Eigen::Vector2f dimensions, bool enableDebugMesh = false);

    ph::rt::Light * addSpotLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float range, const Eigen::Vector3f & emission,
                                 Eigen::Vector2f cones, Eigen::Vector2f dimensions, bool enableDebugMesh = false);

    Eigen::AlignedBox3f addModelToScene(const LoadOptions &);
    ph::rt::Node *      addModelNodeToScene(const LoadOptions &, Eigen::AlignedBox3f & bbox);

    void addCornellBoxToScene(const Eigen::AlignedBox3f & bbox);

    /// Add a square floor to the scene (with lambertian material)
    /// The floor is on X-Z plane with +Y as the up/normal vector.
    /// Returns bounding box of the floor.
    Eigen::AlignedBox3f addFloorPlaneToScene(const Eigen::Vector3f & center, float dimension);

    ph::rt::Node * addQuad(const char * name, float w, float h, ph::rt::Material * material, ph::rt::Node * parent = nullptr,
                           const ph::rt::NodeTransform & transform = ph::rt::NodeTransform::Identity());

    ph::rt::Node * addCircle(const char * name, float w, float h, ph::rt::Material * material, ph::rt::Node * parent = nullptr,
                             const ph::rt::NodeTransform & transform = ph::rt::NodeTransform::Identity());

    ph::rt::Node * addBox(const char * name, float w, float h, float d, ph::rt::Material * material, ph::rt::Node * parent = nullptr,
                          const ph::rt::NodeTransform & transform = ph::rt::NodeTransform::Identity());

    ph::rt::Node * addIcosahedron(const char * name, float radius, uint32_t subdivide, ph::rt::Material * material, ph::rt::Node * parent = nullptr,
                                  const ph::rt::NodeTransform & transform = ph::rt::NodeTransform::Identity());

    // Creates a simple mesh node.
    ph::rt::Node * addMeshNode(ph::rt::Node * parent, const ph::rt::NodeTransform & transform, ph::rt::Mesh * mesh, ph::rt::Material * material);

    // Add the skybox, which will be rendered in a separate pass, without using physray-sdk
    void addSkybox(float lodBias = 0.0f);

    ph::va::ImageObject createShadowCubeMap();

protected:
    Eigen::AlignedBox3f transformBbox(Eigen::AlignedBox3f bbox, ph::rt::NodeTransform t);

    /// @brief Loads the contents of the scene asset into the model viewer.
    /// @param o Model loading options.
    /// @param sceneAsset Contains model and animation data to be displayed.
    void       loadSceneAsset(const LoadOptions & o, const SceneAsset * sceneAsset);
    VkExtent2D _renderTargetSize = {0, 0};

private:
    /// Node that will hold the headlight and the default camera.
    ph::rt::Node * _firstPersonNode = nullptr;

    ph::rt::Node *                    loadObj(const LoadOptions & o, Eigen::AlignedBox3f & bbox);
    std::shared_ptr<const SceneAsset> loadGltf(const LoadOptions & o);

    ph::rt::Mesh * createCircle(float w, float h);
    ph::rt::Mesh * createQuad(float w, float h);
    ph::rt::Mesh * createIcosahedron(float radius, uint32_t subdivide);

    /// Adds the given model file's animations to the list of animations and plays the selected one.
    void addModelAnimations(const LoadOptions & o, const SceneAsset * sceneAsset);
};
