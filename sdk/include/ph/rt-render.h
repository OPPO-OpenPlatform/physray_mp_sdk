/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This is the interface of ray tracing render packs for the PhysRay SDK.

#pragma once

#include "ph/rt-scene.h"

/// Render namespace for Ray Tracing module
namespace ph {
namespace rt {
namespace render {

class RenderPackManager;

struct RenderPack {};

/// --------------------------------------------------------------------------------------------------------------------
/// point light shadow map renderer
struct ShadowMapRenderPack : Root {
    virtual ~ShadowMapRenderPack() = default;

    struct CreateParameters {
        World *  world           = nullptr;
        uint32_t shadowMapSize   = 0;
        VkFormat shadowMapFormat = VK_FORMAT_UNDEFINED;

        /// Define the input layout of the shadow map texture when calling record() method. The default value is VK_IMAGE_LAYOUT_UNDEFINED.
        VkImageLayout shadowMapLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        CreateParameters & set(World * w, uint32_t size, VkFormat format, VkImageLayout layout) {
            world           = w;
            shadowMapSize   = size;
            shadowMapFormat = format;
            shadowMapLayout = layout;
            return *this;
        }
    };

    struct RecordParameters {
        /// store all rendering commands. The buffer must be in recording state.
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

        /// The scene that the light is attached to.
        Scene * scene = nullptr;

        /// The light entity that the shadow map is generated for.
        int64_t lightEntity = 0;
    };

    /// Fill the command buffer with rendering commands to render the shadow map.
    /// After the record() call, the shadow map texture is in VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout.
    virtual void record(const RecordParameters &) = 0;

    /// Performance statistics of the shadow render pack.
    struct PerfStats {
        std::vector<NamedDuration> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

    static PH_RT_API ShadowMapRenderPack *      create(const CreateParameters &);
    static std::unique_ptr<ShadowMapRenderPack> createUnique(const CreateParameters & p) { return std::unique_ptr<ShadowMapRenderPack>(create(p)); }

protected:
    ShadowMapRenderPack(const RootConstructParameters & p): Root(p) {}
};

/// --------------------------------------------------------------------------------------------------------------------
/// a noise-free ray tracer
struct NoiseFreeRenderPack : Root {
    /// shadow tracing mode.
    enum class ShadowMode {
        /// Ray traced shadow.
        RAY_TRACED = 0,

        /// Rasterized shadow (via shadow map)
        RASTERIZED,

        /// Shadow is determined by shadow map first, then refined by ray tracing.
        REFINED,

        /// Debug mode. Reserved for internal use.
        DEBUG,

        /// number of shadow modes
        NUM_SHADOW_MODES,
    };

    /// Defines the main rendering mode of the render pack.
    enum class Mode {
        /// Render the scene with rasterization technique only. It is like a "RT OFF" mode.
        RASTERIZED,

        /// Render scene with all noise free ray tracing effects enabled.
        NOISE_FREE,

        /// Render scene with only ray traced shadow.
        SHADOW_TRACING,
    };

    struct CreateParameters {
        World * world = nullptr;

        Mode mode = Mode::NOISE_FREE;

        VkFormat targetFormat = VK_FORMAT_UNDEFINED;

        uint32_t targetWidth = 0, targetHeight = 0;

        // By default, the render pack determines if the target is in linear or sRGB color space based on
        // target format. This flag is to deal with the case that swapchain image can be set to be in sRGB
        // color space, regardless of the image format.
        bool targetIsSRGB = false;

        /// Define the input layout of the target image. After the rendering, the target image
        /// will always be transferred to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
        /// TODO: move to RecordParameters
        VkImageLayout targetLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        /// Specify the rendering viewport.
        VkViewport viewport = {0, 0, 0, 0, 0.f, 1.f};

        float clearColor[4] = {0.f, 0.f, 0.f, 1.f}; ///< clear color. ignored if clear is false.
        bool  clear         = true;                 ///< if set to true to clear the target view.

        CreateParameters & setMode(Mode m) {
            mode = m;
            return *this;
        }

        CreateParameters & setTarget(VkFormat format, uint32_t width, uint32_t height, VkImageLayout layout, bool isSRGB = false) {
            targetFormat = format;
            targetWidth  = width;
            targetHeight = height;
            targetLayout = layout;
            targetIsSRGB = isSRGB;
            return *this;
        }

        CreateParameters & setViewport(float x, float y, float w, float h) {
            viewport.x        = x;
            viewport.y        = y;
            viewport.width    = w;
            viewport.height   = h;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            return *this;
        }

        CreateParameters & setClear(bool clear_, const float * color = nullptr) {
            clear = clear_;
            if (clear_) {
                if (color) {
                    clearColor[0] = color[0];
                    clearColor[1] = color[1];
                    clearColor[2] = color[2];
                    clearColor[3] = color[3];
                } else {
                    clearColor[0] = clearColor[1] = clearColor[2] = 0.f;
                    clearColor[3]                                 = 1.f;
                }
            }
            return *this;
        }
    };

    /// Fill render pack with rendering commands. Override any existing commands in the render pack.
    struct RecordParameters {
        /// pointer to the scene
        Scene * scene = nullptr;

        /// store all rendering commands. The buffer must be in recording state.
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

        /// The target image to render to.
        /// For ex, for Screen spaceShadowRenderPack, this will be the incoming rasterized
        /// screen space shadow map that will be modified using ray-tracing.
        VkImage targetImage = VK_NULL_HANDLE;

        /// the image that stores the rendering result.
        VkImageView targetView = VK_NULL_HANDLE;

        /// the depth buffer view that store depth result.
        VkImageView depthView = VK_NULL_HANDLE;

        /// Projection matrix.
        Eigen::Matrix4f projMatrix = Eigen::Matrix4f::Identity();

        /// World -> View transformation
        Eigen::Matrix<float, 3, 4> viewMatrix = Eigen::Matrix<float, 3, 4>::Identity();

        /// Ambient light term
        Eigen::Vector3f ambientLight = Eigen::Vector3f::Zero();

        /// Use this to adjust saturation of the final render color.
        float saturation = 1.0f;

        /// Gamma correction. Note that his gamma correction is independent of LINEAR -> sRGB conversion.
        float gamma = 1.0f;

        /// Define the shadow rendering mode. If parameter is ignored by the rasterized render pack.
        ShadowMode shadowMode = ShadowMode::RAY_TRACED;

        /// Toggle on the heat map to illustrate the number of traversal steps in a path/shadow traced query
        bool enableHeatMap = false;

        /// roughness cutoff for ray traced reflection. A surface with roughness smaller than this value will have
        /// ray traced reflection effect.
        float reflectionRoughnessCutoff = .0f;

        // Rotation of skybox about the Y axis in radians
        float skyboxRotation = .0f;

        /// Max number of specular bounces.
        uint32_t maxSpecularBounces = 5;

        /// Diffuse irradiance map with mipmap chain.
        Material::TextureHandle irradianceMap = Material::TextureHandle::EMPTY_CUBE();

        /// Prefiltered reflection map with mipmap chain encoded based on roughness.
        Material::TextureHandle reflectionMap = Material::TextureHandle::EMPTY_CUBE();

        // 1 for enable lighting using skybox, 0 for skybox only work for alpha channel
        bool skyboxLighting = true;
    };

    ~NoiseFreeRenderPack() override = default;

    /// main entry point of the render pack class to record rendering commands to command buffer.
    virtual void record(const RecordParameters &) = 0;

    /// Constructing pipeline is done automatically by render pack class during record() function call. This process could be slow and causes
    /// hiccup of frame rate. This function allows the user to pre-construct the pipeline before the first frame rendering.
    /// \param sceneDescriptors the descriptor set layout of the ph::rt::Scene class. This is used to construct the pipeline layout.
    virtual void reconstructPipelines(VkDescriptorSetLayout sceneDescriptors) = 0;

    /// Performance statistics
    struct PerfStats {
        std::vector<NamedDuration> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

    /// main ray tracing render pack creation
    //@{
    static PH_RT_API NoiseFreeRenderPack *      create(const CreateParameters &);
    static std::unique_ptr<NoiseFreeRenderPack> createUnique(const CreateParameters & cp) { return std::unique_ptr<NoiseFreeRenderPack>(create(cp)); }
    //@}

protected:
    NoiseFreeRenderPack(const RootConstructParameters & p): Root(p) {}
};

// ---------------------------------------------------------------------------------------------------------------------
/// Helper function to append shadow mode to the end of a stream.
inline std::ostream & operator<<(std::ostream & os, NoiseFreeRenderPack::ShadowMode mode) {
    switch (mode) {
    case NoiseFreeRenderPack::ShadowMode::RAY_TRACED:
        os << "RAY_TRACED(";
        break;
    case NoiseFreeRenderPack::ShadowMode::RASTERIZED:
        os << "RASTERIZED(";
        break;
    case NoiseFreeRenderPack::ShadowMode::REFINED:
        os << "REFINED   (";
        break;
    case NoiseFreeRenderPack::ShadowMode::DEBUG:
        os << "DEBUG     (";
        break;
    default:
        os << "(";
        break;
    }
    os << (int) mode << ")";
    return os;
}

/// --------------------------------------------------------------------------------------------------------------------
/// a path tracer
struct PathTracingRenderPack : Root {
    enum Accumulation {
        OFF,
        ON,
        RETAIN,
    };

    /// Defines the main rendering mode of the render pack.
    enum class Mode {
        /// Render the scene with a full path tracer.
        PATH_TRACING,

        /// Render the scene with the performance path tracer
        FAST_PT,
    };

    struct CreateParameters {
        World * world = nullptr; ///< pointer to the RT world. Must be valid for the lifetime of the render pack.

        Mode mode = Mode::PATH_TRACING;

        VkFormat targetFormat = VK_FORMAT_UNDEFINED;

        uint32_t targetWidth = 0, targetHeight = 0;

        // By default, the render pack determines if the target is in linear or sRGB color space based on
        // target format. This flag is to deal with the case that swapchain image can be set to be in sRGB
        // color space, regardless of the image format.
        bool targetIsSRGB = false;

        /// Define the input layout of the target image. After the rendering, the target image
        /// will always be transferred to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
        /// TODO: move to RecordParameters
        VkImageLayout targetLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        /// Specify the rendering viewport.
        VkViewport viewport = {0, 0, 0, 0, 0.f, 1.f};

        float clearColor[4] = {0.f, 0.f, 0.f, 1.f}; ///< clear color. ignored if clear is false.
        bool  clear         = true;                 ///< if set to true to clear the target view.

        // By default, shaders may be configured with a number of global settings specified via uniforms.
        // When set to true, these uniforms are overridden with hardcoded precompiled defines specified in
        // defines.glsl, which provides a significant speedup. When building your release application,
        // hard-code your desired define values in define.glsl and set this to true.
        bool usePrecompiledShaderParameters = false;

        // If the scene is known to have no refractive or rough reflective materials,
        // you may clear this flag to speed up rendering in NOISE_FREE mode.
        bool refractionAndRoughReflection = true;

        CreateParameters & setFast(bool enabled) {
            mode = enabled ? Mode::FAST_PT : Mode::PATH_TRACING;
            return *this;
        }

        CreateParameters & setTarget(VkFormat format, uint32_t width, uint32_t height, VkImageLayout layout, bool sRGB = false) {
            targetFormat = format;
            targetWidth  = width;
            targetHeight = height;
            targetLayout = layout;
            targetIsSRGB = sRGB;
            return *this;
        }

        CreateParameters & setViewport(float x, float y, float w, float h) {
            viewport.x        = x;
            viewport.y        = y;
            viewport.width    = w;
            viewport.height   = h;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            return *this;
        }

        CreateParameters & setClear(bool clear_, const float * color = nullptr) {
            clear = clear_;
            if (clear_) {
                if (color) {
                    clearColor[0] = color[0];
                    clearColor[1] = color[1];
                    clearColor[2] = color[2];
                    clearColor[3] = color[3];
                } else {
                    clearColor[0] = clearColor[1] = clearColor[2] = 0.f;
                    clearColor[3]                                 = 1.f;
                }
            }
            return *this;
        }
    };

    struct TransparentShadowSettings {
        bool tshadowAlpha      = false; // enable transparent shadows
        bool tshadowColor      = false; // enable 3-channel colored transparent shadows
        bool tshadowAbsorption = false; // enable approximate volumetric absorption for transparent shadows
        bool tshadowFresnel    = false; // enable approximate fresnel attenuation for transparent shadows
        bool tshadowTextured   = false; // enable textured alpha, color, and normals when rendering transparent shadows
    };

    // PhysRay has two classes of transparent objects: Alpha (ior = 0) and transmissive (ior > 0).
    // Alpha objects are rendered with simple alpha-blended multiplicative transparency. Objects with alpha values below alphaCutoff are not rendered.
    // Transmissive objects with alpha values below alphaCutoff are rendered with refractive transparency. Above alpha cutoff, the objects are treated as
    // opaques.
    struct TransparencySettings {
        float    alphaCutoff                      = 0.f;   // skip rendering of ior = 0 objects with alpha below this value (0.0 = off)
        uint32_t alphaMaxHit                      = 2;     // maximum number of alpha blended hits before returning black
        uint32_t backscatterMode                  = 0;     // enable approximate volumetric backscattering for transmissive objects and their shadows
        bool     calculateAbsorptionTransmittance = false; // enable approximate spectral transmission for transmissive objects and their shadows
        float    fresnelCutoff                    = 0.55f; // fresnel cutoff value used for blended fresnel splitting. reflect above this value, refract below.
        TransparentShadowSettings shadowSettings;
    };

    /// Fill render pack with rendering commands. Override any existing commands in the render pack.
    struct RecordParameters {
        /// pointer to the scene
        Scene * scene = nullptr;

        /// store all rendering commands. The buffer must be in recording state.
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

        /// The target image to render to.
        /// For ex, for Screen spaceShadowRenderPack, this will be the incoming rasterized
        /// screen space shadow map that will be modified using ray-tracing.
        VkImage targetImage = VK_NULL_HANDLE;

        /// the image that stores the rendering result.
        VkImageView targetView = VK_NULL_HANDLE;

        /// the depth buffer view that store depth result.
        VkImageView depthView = VK_NULL_HANDLE;

        /// Projection matrix.
        Eigen::Matrix4f projMatrix = Eigen::Matrix4f::Identity();

        /// World -> View transformation
        Eigen::Matrix<float, 3, 4> viewMatrix = Eigen::Matrix<float, 3, 4>::Identity();

        /// Ambient light term
        Eigen::Vector3f ambientLight = Eigen::Vector3f::Zero();

        /// Use this to adjust saturation of the final render color.
        float saturation = 1.0f;

        /// Gamma correction. Note that his gamma correction is independent of LINEAR -> sRGB conversion.
        float gamma = 1.0f;

        /// Toggle on the heat map to illustrate the number of traversal steps in a path/shadow traced query
        bool enableHeatMap = false;

        /// Sets the upper end of the range threshold that determines a specific calibration for how our heat map appears
        float maxNumTraversalSteps = 200.f;

        /// roughness cutoff for ray traced reflection. A surface with roughness smaller than this value will have
        /// ray traced reflection effect.
        float reflectionRoughnessCutoff = 0.f;

        // Rotation of skybox about the Y axis in radians
        float skyboxRotation = 0.f;

        // Path tracing specific config
        int          initialCandidateCount = 0;   // initial candidate count for ReSTIR (M)
        float        jitterAmount          = 0.f; // sub-pixel jitter used to cheaply anti-alias the rasterized first-bounce of stochastic path tracers
        Accumulation accum                 = Accumulation::OFF; // accumulation mode for stochastic path tracers
        uint32_t     spp                   = 1;                 ///< samples per pixel per frame.
        // subsurface scattering for stochastic path tracers
        float subsurfaceChance = 0.f;  // chance of sampling subsurface contribution in stochastic path tracers
        float rmaxScalar       = 1.f;  // max radius used to sample offset positions for stochastic path tracing of subsurface scattering
        float emissionScalar   = 1.f;  // scales intensity of subsurface values stored in color channels of the emission map
        float sssamtScalar     = 1.f;  // scales intensity of subsurface amount values stored in the alpha channel of the emission map
        float nChance          = 0.5f; // chance of casting subsurface ray in the normal (vs. the tangent, bitangent) direction
        float gaussV           = 1.f;  // gaussian variance used to sample a position within the max radius for subsurface scattering
        // cluster params for stochastic path tracers
        uint32_t        clusterMode       = 0;
        Eigen::Vector3f sceneExtents      = Eigen::Vector3f::Zero();
        Eigen::Vector3f sceneCenter       = Eigen::Vector3f::Zero();
        int             sceneSubdivisions = 0;

        /// Ray tracing settings
        float minRayLength = 0.001f;

        /// Max number of diffuse bounces
        uint32_t maxDiffuseBounces = 3;

        /// Max number of specular bounces.
        uint32_t maxSpecularBounces = 5;

        /// Transparency settings
        TransparencySettings transparencySettings;

        /// Diffuse irradiance map with mipmap chain.
        Material::TextureHandle irradianceMap = Material::TextureHandle::EMPTY_CUBE();

        /// Prefiltered reflection map with mipmap chain encoded based on roughness.
        Material::TextureHandle reflectionMap = Material::TextureHandle::EMPTY_CUBE();

        // 1 for enable lighting using skybox, 0 for skybox only work for alpha channel
        uint32_t skyboxLighting = 1;

        // toggle restir sampling mode
        uint32_t restirMode = 0; // 0 = restir off
                                 // 1 = restir on (initial sampling only, using initialCandidateCount samples)
                                 // 2 = temporal restir (initial sampling + temporal resampling)
                                 // 3 = spatiotemporal restir (initial sampling + temporal resampling + spatial resampling)
        uint32_t misMode = 0;    // 0 = partitioned (DI evaluated using light sampling only, indirect hits on emissives are ignored)
                                 // 1 = veach MIS (DI evaluated using light sampling, power-weighted and combined with indirect hits on emissives)

        bool enableRestirMap = false; // show p_hat value stored in restir history buffer. only valid for restir modes 2 and 3.
    };

    /// Performance statistics
    struct PerfStats {
        std::vector<NamedDuration> gpuTimestamps;
    };

    ~PathTracingRenderPack() override = default;

    /// main entry point of the render pack class to record rendering commands to command buffer.
    virtual void record(const RecordParameters &) = 0;

    /// Constructing pipeline is done automatically by render pack class during record() function call. This process could be slow and causes
    /// hiccup of frame rate. This function allows the user to pre-construct the pipeline before the first frame rendering.
    /// \param sceneDescriptors the descriptor set layout of the ph::rt::Scene class. This is used to construct the pipeline layout.
    virtual void reconstructPipelines(VkDescriptorSetLayout sceneDescriptors) = 0;

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

    /// main ray tracing render pack creation
    //@{
    static PathTracingRenderPack *                create(const CreateParameters &);
    static std::unique_ptr<PathTracingRenderPack> createUnique(const CreateParameters & p) { return std::unique_ptr<PathTracingRenderPack>(create(p)); }
    //@}

protected:
    PathTracingRenderPack(const RootConstructParameters & p): Root(p) {}
};

/// --------------------------------------------------------------------------------------------------------------------
/// a ray tracing reflection renderer
struct ReflectionRenderPack : Root {
    //@{
    struct CreateParameters {
        World * world = nullptr; ///< pointer to the RT world. Can't be null.

        VkFormat targetFormat = VK_FORMAT_UNDEFINED; ///< format of the target image. Can't be VK_FORMAT_UNDEFINED.

        uint32_t targetWidth = 0, targetHeight = 0;

        /// Define the input layout of the target image. After the rendering, the target image
        /// will always be transferred to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
        /// TODO: move to RecordParameters
        VkImageLayout targetLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        /// Specify the rendering viewport.
        VkViewport viewport = {0, 0, 0, 0, 0, 1.f};

        /// Enable special shader logic to support Unity Engine. Such as how normal map texture is unpacked.
        bool unity = false;

        /// Set world pointer.
        CreateParameters & setWorld(World * w) {
            world = w;
            return *this;
        }

        /// Set target properties. Also reset viewport to default value.
        CreateParameters & setTarget(VkFormat format, uint32_t width, uint32_t height, VkImageLayout layout) {
            targetFormat = format;
            targetWidth  = width;
            targetHeight = height;
            targetLayout = layout;
            setViewport(0, 0, (float) width, (float) height);
            return *this;
        }

        CreateParameters & setViewport(float x, float y, float w, float h, float minDepth = 0.f, float maxDepth = 1.f) {
            viewport.x        = x;
            viewport.y        = y;
            viewport.width    = w;
            viewport.height   = h;
            viewport.minDepth = minDepth;
            viewport.maxDepth = maxDepth;
            return *this;
        }

        CreateParameters & setUnity(bool v) {
            unity = v;
            return *this;
        }
    };

    /// Fill render pack with rendering commands. Override any existing commands in the render pack.
    struct RecordParameters {
        /// pointer to the scene. Must be valid.
        Scene * scene = nullptr;

        /// store all rendering commands. The buffer must be valid and in recording state.
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

        /// The target image to render to. Must be a valid image.
        VkImage targetImage = VK_NULL_HANDLE;

        /// The view of the target image to render to. Must be a valid image view.
        VkImageView targetView = VK_NULL_HANDLE;

        /// Projection matrix.
        Eigen::Matrix4f projMatrix = Eigen::Matrix4f::Identity();

        /// World -> View transformation
        Eigen::Matrix<float, 3, 4> viewMatrix = Eigen::Matrix<float, 3, 4>::Identity();

        /// Ambient light term
        Eigen::Vector3f ambientLight = Eigen::Vector3f::Zero();

        /// roughness cutoff for ray traced reflection. A surface with roughness smaller than this value will have
        /// ray traced reflection effect.
        float reflectionRoughnessCutoff = 0.0f;

        /// Max number of bounces on specular surface.
        uint32_t maxSpecularBounces = 1;

        /// Max number of surfaces the ray can go through when calculating shadow on transparent surface.
        uint32_t alphaMaxHit = 2;

        /// The color to clear the target image to.
        Eigen::Vector4f background = Eigen::Vector4f::Zero();

        /// Diffuse irradiance map with mipmap chain.
        Material::TextureHandle irradianceMap = Material::TextureHandle::EMPTY_CUBE();

        /// Prefiltered reflection map with mipmap chain encoded based on roughness.
        Material::TextureHandle reflectionMap = Material::TextureHandle::EMPTY_CUBE();
    };

    ~ReflectionRenderPack() override = default;

    /// main entry point of the render pack class to record rendering commands to command buffer.
    virtual void record(const RecordParameters &) = 0;

    /// Performance statistics
    struct PerfStats {
        std::vector<NamedDuration> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

    static PH_RT_API ReflectionRenderPack * create(const CreateParameters &);

    static std::unique_ptr<ReflectionRenderPack> createUnique(const CreateParameters & p) { return std::unique_ptr<ReflectionRenderPack>(create(p)); }

protected:
    ReflectionRenderPack(const RootConstructParameters & p): Root(p) {}
};

} // namespace render
} // namespace rt
} // namespace ph