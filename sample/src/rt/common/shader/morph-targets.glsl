// Define data structure shared between CPU & GPU. This header will be included on CPU side too.
// So don't put GLSL specific grammar here.
#ifndef _MORTH_TARGETS_GLSL_
#define _MORTH_TARGETS_GLSL_

#ifdef __cplusplus
    #include <ph/rt-utils.h>
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

// Needed for normalizing temporal seed
#ifndef RAND_MAX
    #define RAND_MAX 2147483647
#endif

/// Note: Do _NOT_ use type "bool" in any structure shared between CPU & GPU, since it'll leave gaps in c++ that could
/// confuse shader code. Use the special "boolean" type. It is defined as "uint" in c++ to ensure 4-byte alignment.
#ifdef __cplusplus
typedef uint boolean;
#else
    #define boolean bool
#endif

// =====================================================================================================================
// Mesh
// =====================================================================================================================

// ---------------------------------------------------------------------------------------------------------------------

struct Vertex {
    // Position in object space of this vertex.
    vec3 position;
    // material index of the vertex
    uint16_t material;
    // Normal of the triangle's face at this vertex.
    vec3 normal;
    // TODO: This is a temporary hack to just make skinned meshing possible.
    // This may or may not be removed in the future, though the purpose of it
    // will probably change even if it is kept.
    uint16_t flags;
    // A direction in the face's surface.
    vec3 tangent;
    // Tangent padding
    uint16_t paddingTan;
    // X coordinate of the texture.
    float texU;
    // Y coordinate of the texture.
    float texV;
    // UV padding
    vec2 paddingUV;
};

#ifdef __cplusplus
static_assert(0 == (sizeof(Vertex) % 16));
#endif

#ifdef __cplusplus
} // namespace jedi::rt::device
#endif

#endif // _MORTH_TARGETS_GLSL_