#ifndef _RT_SCENE_GLSL_
#define _RT_SCENE_GLSL_

#ifdef __cplusplus
    #include <ph/rt-scene.h>
namespace ph::rt::device {
typedef unsigned int                  uint;
typedef Eigen::Matrix<int32_t, 2, 1>  ivec2;
typedef Eigen::Matrix<int32_t, 3, 1>  ivec3;
typedef Eigen::Matrix<int32_t, 4, 1>  ivec4;
typedef Eigen::Matrix<uint32_t, 2, 1> uvec2;
typedef Eigen::Matrix<uint32_t, 3, 1> uvec3;
typedef Eigen::Matrix<uint32_t, 4, 1> uvec4;
typedef Eigen::Vector2f               vec2;
typedef Eigen::Vector3f               vec3;
typedef Eigen::Vector4f               vec4;
typedef Eigen::Matrix3f               mat3;
typedef Eigen::Matrix4f               mat4;
    #define INITIAL_VALUE(x) = x
#else
    #extension GL_EXT_shader_explicit_arithmetic_types : require
    #define constexpr const
    #define inline
    #define INITIAL_VALUE(x)
#endif

/// Note: Do _NOT_ use type "bool" in any structures shared between CPU & GPU, since it'll leave gaps in c++ that could
/// confuse shader code. Use the special "boolean" type. It is defined as "uint" in c++ to ensure 4-byte alignment.
#ifdef __cplusplus
typedef uint boolean;
#else
    #define boolean bool
#endif

// ---------------------------------------------------------------------------------------------------------------------
struct Vertex {
    vec3  position;
    float texU;
    vec3  normal;
    float texV;
    vec3  tangent;
    float padding; // to keep the structure aligned to 16 bytes.
};
#ifdef __cplusplus
static_assert(0 == (sizeof(Vertex) % 16), "");
#endif

// ---------------------------------------------------------------------------------------------------------------------
// Represents 1 instanced triangle
// TODO: repalce this with a Subset struct. So we don't to repeat the material and flag information on each triangle.
struct Triangle {
    uint32_t material; // index into material buffer.
    uint32_t flags;    // flags copied over from model subset.
};
#ifdef __cplusplus
static_assert(0 == (sizeof(Triangle) % 8), "");
#endif

// ---------------------------------------------------------------------------------------------------------------------
// One (model) instance maps to one model attached a node. If a model is attached to multiple nodes, it'll considered
// having multiple instances. Each instance will have an entry in the instance buffer represented by this structure.
struct Instance {
    mat4 world;         // world transformation
    mat4 invW;          // inverse of the world matrix
    uint vertexBase;    // index into the vertex buffer.
    uint indexBase;     // index into the index buffer.
    uint triangleBase;  // index into the triangle buffer.
    uint triangleCount; // size of the relevant triangle buffer region.
    vec3 padding;
    uint maskToInstance;
};
#ifdef __cplusplus
static_assert(0 == (sizeof(Instance) % 16), "");
#endif

// ---------------------------------------------------------------------------------------------------------------------
// GLSL definition of axis aligned bounding box. This struct needs to match the C++ AABB class defined in aabb.h
struct AABB {
    vec3  min;
    int   rope;
    vec3  max;
    int   nouse;
    ivec2 children;  ///< >0: index into the TLAS/BLAS array; <=0: index into the BLAS/triangle array
    ivec2 instances; ///< Indices into instance array for children. Each component is valid iff corresponding child is a BLAS root.
};

// ---------------------------------------------------------------------------------------------------------------------
//
struct Material {
    vec3  albedo;
    float opaque;

    vec3  emission;
    float roughness;

    float metalness INITIAL_VALUE(0.0);
    float ao        INITIAL_VALUE(1.0f);
    float ior       INITIAL_VALUE(1.45f);
    float sss       INITIAL_VALUE(0.0f); // 0 = emissive, >0 = subsurface, with sss scaling subsurface intensity

    float clearcoat          INITIAL_VALUE(0.0f); // Define how strong clear coat reflection is.
    float clearcoatRoughness INITIAL_VALUE(1.0f);
    float anisotropic        INITIAL_VALUE(0.0f);
    float sssamt             INITIAL_VALUE(0.0f); // used as 1 - subsurfaceAmount for attenuating thin-object SSS

    vec3  emissiveSaturationHue; // This is a vec3 in case we want to support the value part of hsv later.
    float unused;

    // Texture map list. This list need to match rt::Material::TextureType
#ifdef __cplusplus
    union {
        int textures[(int) rt::Material::TextureType::COUNT];
        struct {
#endif
            // index into texture array. 0 means no texture.
            int albedoMap;
            int normalMap;
            int ormMap; // Combined ambient occlusion and metalRoughness map (R: AO; G: roughness; B: metalness)
            int emissionMap;
#ifdef __cplusplus
        };
    };
#endif

    // float subsurface;
    // float specular;
    // float specularTint;
    // float sheen;

    // float sheenTint;

#ifdef __cplusplus
    Material() {}
    Material(const rt::Material::Desc & d) {
        albedo[0]                = d.albedo[0];
        albedo[1]                = d.albedo[1];
        albedo[2]                = d.albedo[2];
        emissiveSaturationHue[0] = d.emissiveSaturation;
        emissiveSaturationHue[1] = d.emissiveHueOffset;
        opaque                   = d.opaque;
        emission[0]              = d.emission[0];
        emission[1]              = d.emission[1];
        emission[2]              = d.emission[2];
        roughness                = d.roughness;
        metalness                = d.metalness;
        ao                       = d.ao;
        ior                      = d.ior;
        sss                      = d.sss;
        clearcoat                = d.clearcoat;
        clearcoatRoughness       = d.clearcoatRoughness;
        anisotropic              = d.anisotropic;
        sssamt                   = d.sssamt;
        for (auto & t : textures) t = 0;
    }
#endif
};
#ifdef __cplusplus
static_assert(0 == (sizeof(Material) % 16), "");
#endif

// ---------------------------------------------------------------------------------------------------------------------
// Note: this need to match Light::Desc::Type definition in rt-scene.h
constexpr int LIGHT_OFF         = 0;
constexpr int POINT_LIGHT       = 1;
constexpr int DIRECTIONAL_LIGHT = 2;
constexpr int SPOT_LIGHT        = 3;
constexpr int GEOM_LIGHT        = 4;

// ---------------------------------------------------------------------------------------------------------------------
struct Light {
    mat4 projView; // matrix translating from light space to clip space

    vec3 emission;
    int  type; // 0: off, 1: point, 2: directional, 3: spot, 4: mesh

    vec3  position; // position in world space (used by point and spot light)
    float range;    // range of the point/spot light, in world space.

    vec3 direction; // direction in world space (used by spot and directional light)
    int  shadowMap; // index to the texture array.

    uint  shadowMapWidth;
    uint  shadowMapHeight;
    float bias;
    float slopeBias;

    // dimension in light space
    // point lights: dimension.x = 0 for delta point lights, dimension.x > 0 gives radius of spherical light
    // directional lights: dimension.x = dimension.y = 0 for delta directional lights; both must be greater than zero to denote dimensions of rectangular area
    // light spot lights: dimension.x = dimension.y = 0 for delta spotlights; both must be greater than 0 to denote dimensions of disk area light
    // mesh lights: dimensions give world-space dimensions of the instance's bounding box
    vec3 dimension;
    // if true, this light will cast shadows across the scene.
    boolean allowShadow INITIAL_VALUE(true);

    // Only used for spot lights:
    // extents.x = half-angle of cone illuminated by the spotlight. Clamped to [0, 90]
    // extents.y = half-angle of inner cone of spotlight, beyond which falloff begins. Clamped to [0, extents.x]
    vec2 cones;
    // Only used for mesh lights.
    // Gives the index into the instance buffer corresponding to the light.
    int instanceIndex INITIAL_VALUE(-1);
    float             nouse;

#ifdef __cplusplus

    bool operator==(const Light & rhs) const {
        if (projView != rhs.projView) return false;
        if (emission != rhs.emission) return false;
        if (type != rhs.type) return false;
        if (position != rhs.position) return false;
        if (range != rhs.range) return false;
        if (direction != rhs.direction) return false;
        if (shadowMap != rhs.shadowMap) return false;
        if (shadowMapWidth != rhs.shadowMapWidth) return false;
        if (shadowMapHeight != rhs.shadowMapHeight) return false;
        if (bias != rhs.bias) return false;
        if (slopeBias != rhs.slopeBias) return false;
        if (dimension != rhs.dimension) return false;
        if (allowShadow != rhs.allowShadow) return false;
        if (cones != rhs.cones) return false;
        if (instanceIndex != rhs.instanceIndex) return false;
        return true;
    }

    bool operator!=(const Light & rhs) const { return !operator==(rhs); }

#endif
};

constexpr uint32_t SCENE_MATERIAL_BINDING     = 0;
constexpr uint32_t SCENE_VERTEX_BINDING       = 1;
constexpr uint32_t SCENE_INDEX_BINDING        = 2;
constexpr uint32_t SCENE_TRIANGLE_BINDING     = 3;
constexpr uint32_t SCENE_INSTANCE_BINDING     = 4;
constexpr uint32_t SCENE_LIGHT_BINDING        = 5;
constexpr uint32_t SCENE_TLAS_BINDING         = 6;
constexpr uint32_t SCENE_BLAS_BINDING         = 7;
constexpr uint32_t SCENE_STORAGE_BUFFER_COUNT = 8;
constexpr uint32_t SCENE_SAMPLER_BINDING      = 8;
constexpr uint32_t SCENE_SHADOW_MAP_BINDING   = 9;

#ifdef __cplusplus
} // ph::rt::device
#endif

#endif