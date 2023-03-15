// contains shared data struture between CPU & GPU for skinning.
#ifndef _SKINNED_MESH_GLSL
#define _SKINNED_MESH_GLSL

#ifdef __cplusplus
    #include <ph/va.h>
namespace skinning {
typedef Eigen::Matrix<uint32_t, 4, 1> uvec4;
typedef unsigned int                  uint;
typedef Eigen::Vector2f               vec2;
typedef Eigen::Vector3f               vec3;
typedef Eigen::Vector4f               vec4;
typedef Eigen::Matrix3f               mat3;
typedef Eigen::Matrix4f               mat4;
#else
    #extension GL_EXT_shader_explicit_arithmetic_types : require
    #define constexpr const
    #define inline
    #define INITIAL_VALUE(x)
#endif

// ---------------------------------------------------------------------------------------------------------------------
struct Vertex {
    // Position in object space of this vertex.
    vec3  position;
    float posPadding;

    // Normal of the triangle's face at this vertex.
    vec3  normal;
    float normalPadding;
};
#ifdef __cplusplus
static_assert(0 == (sizeof(Vertex) % 16));
#endif

// ---------------------------------------------------------------------------------------------------------------------
struct WeightedJoint {
    // Each component matches to one of the transform indices
    // inside of joints and controls how much impact this transform
    // has on the weighted sum. Joint indices that aren't being used are given a
    // weight of 0, the sum of all weights should normally be 1.
    vec4 weights;
    // Each component is an index pointing to one of the 4 transforms
    // the transform of this vertex is calculated from the weighted sum of.
    uvec4 joints;
};
#ifdef __cplusplus
static_assert(0 == (sizeof(WeightedJoint) % 16));
#endif

#ifdef __cplusplus
} // namespace skinning
#endif

#endif // _SKINNED_MESH_GLSL