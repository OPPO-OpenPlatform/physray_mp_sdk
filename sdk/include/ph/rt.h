// This is the low level building blocks of PhysRay ray tracing SDK.

#pragma once

#include <ph/base.h>
#include <ph/va.h>

#include <array> // for std::size
#include <typeinfo>

/// Main namespace for Ray Tracing module
namespace ph::rt {

class World;

/// --------------------------------------------------------------------------------------------------------------------
///
/// this is root class of everthing in a ray traced world
class Root {
public:
    PH_NO_COPY(Root);

    PH_NO_MOVE(Root);

    typedef int64_t Id; ///< type of the root object id.

    /// returns pointer to the world that this object belongs to.
    World * world() const { return _w; }

    /// Returns a unique ID of the object. 0 is reserved for invalid object.
    Id id() const { return _id; };

    /// This name variable is reserved strictly for debugging and logging  for library users.
    /// PhysRay-RT's internal code will not depends on it. Feel free to set it to whatever value.
    std::string name;

    struct RootConstructParameters {
        World * w;
        Id      id;
    };

    /// Store a copy of user defined data. Set either data or length to zero to erase the data from the current object.
    void setUserData(const UInt128 & guid, const void * data, size_t length);

    ConstRange<uint8_t> userData(const UInt128 & guid) const;

protected:
    Root(const RootConstructParameters & p);

    virtual ~Root();

private:
    class Impl;
    Impl *  _impl;
    World * _w;
    Id      _id;
};

/// Defines location and orientation of an object in it's parent coordinate system.
///
/// It is based on right handed coordinate system:
///
///     +X -> right
///     +Y -> top
///     +Z -> inward (pointing out of screen, to your eyes)
///
/// It can transform vector from local space to parent space.
///
/// Call NodeTransform::translation() to get its translation part, and NodeTransform::rotation() for the rotation part.
class NodeTransform : public Eigen::AffineCompact3f {
public:
    using Eigen::AffineCompact3f::AffineCompact3f;
    using Eigen::AffineCompact3f::operator=;

    /// copy construct from affine transform
    NodeTransform(const Eigen::AffineCompact3f & t): Eigen::AffineCompact3f(t) {}

    /// copy form affine transform
    NodeTransform & operator=(const Eigen::AffineCompact3f & t) {
        *(Eigen::AffineCompact3f *) this = t;
        return *this;
    }

    /// ----------------------------------------------------------------------------------------------------------------
    /// Convenience function for setting transform from translation, rotation, and scaling.
    NodeTransform & reset(const Eigen::Vector3f & t = Eigen::Vector3f::Zero(), const Eigen::Quaternionf & r = Eigen::Quaternionf::Identity(),
                          const Eigen::Vector3f & s = Eigen::Vector3f::Ones()) {
        // Make sure the transform is initialized to identity.
        *this = Identity();

        // Combine everything into the transform by order of translate, rotate, scale.
        translate(t);
        rotate(r);
        scale(s);

        // Done.
        return *this;
    }

    /// ----------------------------------------------------------------------------------------------------------------
    /// Convenience function for setting transform from
    /// translation, rotation, and scaling.
    static inline NodeTransform make(const Eigen::Vector3f & t = Eigen::Vector3f::Zero(), const Eigen::Quaternionf & r = Eigen::Quaternionf::Identity(),
                                     const Eigen::Vector3f & s = Eigen::Vector3f::Ones()) {
        NodeTransform tr;
        tr.reset(t, r, s);
        return tr;
    }

    /// ----------------------------------------------------------------------------------------------------------------
    /// Get the scaling part of the transformation
    Eigen::Vector3f scaling() const {
        // Fetch the rotation scaling.
        Eigen::Matrix3f scaling;
        computeRotationScaling((Eigen::Matrix3f *) nullptr, &scaling);

        // Convert scale to vector and return it.
        return Eigen::Vector3f {scaling(0, 0), scaling(1, 1), scaling(2, 2)};
    }

    /// ----------------------------------------------------------------------------------------------------------------
    /// Decomposes the transform into its translation, rotation, and scale.
    /// Will ignore any null pointers.
    const NodeTransform & decompose(Eigen::Vector3f * t, Eigen::Quaternionf * r, Eigen::Vector3f * s) const {
        // Fetch the translation.
        if (t != nullptr) { *t = translation(); }

        // Fetch the rotation scaling.
        // If user requested rotation.
        if (r != nullptr) {
            // If user requested rotation and scale.
            if (s != nullptr) {
                // Compute rotation and scaling.
                Eigen::Matrix3f rotationMatrix;
                Eigen::Matrix3f scalingMatrix;
                computeRotationScaling(&rotationMatrix, &scalingMatrix);
                *r = rotationMatrix;
                *s = Eigen::Vector3f {scalingMatrix(0, 0), scalingMatrix(1, 1), scalingMatrix(2, 2)};

                // If user only requested rotation.
            } else {
                *r = rotation();
            }

            // If user requested scale and not rotation.
        } else if (s != nullptr) {
            *s = scaling();
        }
        return *this;
    }

    /// --------------------------------------------------------------------------------------------------------------------
    /// set rotation of the transformation
    NodeTransform & setRotation(const Eigen::Quaternionf & r) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f t = translation();
        Eigen::Vector3f s = scaling();
        reset(t, r, s);
        return *this;
    }

    /// --------------------------------------------------------------------------------------------------------------------
    /// set rotation of the transformation
    NodeTransform & setRotation(const Eigen::AngleAxisf & r) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f t = translation();
        Eigen::Vector3f s = scaling();
        reset(t, Eigen::Quaternionf(r), s);
        return *this;
    }

    /// --------------------------------------------------------------------------------------------------------------------
    ///
    NodeTransform & setScaling(const Eigen::Vector3f & s) {
        // Decompose the transform's current values so we can recreate it with the modified value.
        Eigen::Vector3f    t(translation());
        Eigen::Quaternionf r(rotation());
        reset(t, r, s);
        return *this;
    }

    // ---------------------------------------------------------------------------------------------------------------------
    /// convert to 4x4 matrix
    Eigen::Matrix4f matrix4f() const {
        Eigen::Matrix4f m;
        m.block<3, 4>(0, 0) = this->matrix();
        m.block<1, 4>(3, 0) = Eigen::Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
        return m;
    }
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// defines a material
class Material : public Root {

public:
    enum class TextureType {
        ALBEDO = 0, // Diffuse albedo map.
        NORMAL,     // Normal map.
        ORM,        // Combined occlusion(R)-roughness(G)-metalness(B) map.
        EMISSION,
        DEPTH, // Thickness/depth map used for subsurface scattering on thin objects
        COUNT,
    };

    /// Allows textures to be loaded into vulkan.
    ///
    /// Supports std::hash.
    struct TextureHandle {
        /// A texture handle containing no image.
        static const TextureHandle EMPTY;

        VkImage         image    = 0;
        VkImageView     view     = 0;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        VkExtent3D      extent   = {0, 0, 0}; // if not zero, indicates size of the base level.

        TextureHandle() = default;
        TextureHandle(VkImage i, VkImageView v, VkImageViewType t, uint32_t w, uint32_t h = 1, uint32_t d = 1)
            : image(i), view(v), viewType(t), extent({w, h, d}) {}
        TextureHandle(const ph::va::ImageObject & i): image(i.image), view(i.view), viewType(i.viewType), extent(i.ci.extent) {}

        /// @return true if the two texture handles point to the same image.
        bool operator==(const TextureHandle & rhs) const {
            // If all variables match, return true.
            return image == rhs.image && view == rhs.view && viewType == rhs.viewType;
        }

        /// @return true if the two texture handles do NOT point to the same image.
        bool operator!=(const TextureHandle & rhs) const { return !operator==(rhs); }

        /// Provides a natural ordering for texture handles for the purposes of ordered containers.
        bool operator<(const TextureHandle & rhs) const {
            if (image < rhs.image) return true;
            if (image > rhs.image) return false;
            if (view < rhs.view) return true;
            if (view > rhs.view) return false;
            return viewType < rhs.viewType;
        }

        /// Cast to bool
        operator bool() const { return !empty(); }

        /// @return true if this texture handle does not point to a texture.
        bool empty() const {
            // Return true if this texture handle does not point to anything.
            return image == 0;
        }
    };

    /// Material description
    struct Desc {

        /// Commonly used PBR surface properties.
        /// See https://google.github.io/filament/Filament.md.html for a good explanation of parameters list here.
        //@{
        float albedo[3]          = {1.f, 1.f, 1.f};
        float emissiveSaturation = 1.0f;
        float emissiveHueOffset  = 0.0f;
        float opaque             = 1.0; // 0.0 : fully transparent, 1.0 : fully opaque;
        float emission[3]        = {0.f, 0.f, 0.f};
        float roughness          = 1.f;
        float metalness          = 0.f;
        float ao                 = 1.0f;
        float clearcoat          = 0.f; // Define how strong clear coat reflection is.
        float clearcoatRoughness = 0.f; // roughness of the clear coat layer.

        float sss = 0.0f; // 0 = emissive, 1 = subsurface

        // Index of refraction. It is only meaningful for transparent materials.
        // IOR of commonly seen materials:
        //      vacuum  : 1.0 (by definition)
        //      air     : 1.000293
        //      water   : 1.333
        //      glass   : 1.4~1.7
        //      amber   : 1.55
        //      diamond : 2.417
        float ior = 1.45f;

        // float subsurface  = 0.f;
        // float specular = 0.5f;
        // float specularTint = 0.f;
        // Parameterized per https://google.github.io/filament/Filament.md.html#materialsystem/anisotropicmodel
        float anisotropic = 0.f;
        // float sheen = 0.f;
        // float sheenTint = 0.5f;

        //@}

        TextureHandle maps[static_cast<size_t>(TextureType::COUNT)];

        Desc & setAlbedo(float r, float g, float b) {
            albedo[0] = r;
            albedo[1] = g;
            albedo[2] = b;
            return *this;
        }
        Desc & setOpaqueness(float f) {
            opaque = f;
            return *this;
        }
        Desc & setEmission(float r, float g, float b) {
            emission[0] = r;
            emission[1] = g;
            emission[2] = b;
            return *this;
        }
        Desc & setRoughness(float f) {
            roughness = f;
            return *this;
        }
        Desc & setMetalness(float f) {
            metalness = f;
            return *this;
        }
        Desc & setOcclusion(float o) {
            ao = o;
            return *this;
        }
        Desc & setAnisotropic(float a) {
            anisotropic = a;
            return *this;
        }
        Desc & setIor(float i) {
            ior = i;
            return *this;
        }
        Desc & setSSS(float intensity) {
            sss = intensity;
            return *this;
        }
        Desc & setMap(TextureType type, const TextureHandle & image) {
            maps[(int) type] = image;
            return *this;
        }
        Desc & setAlbedoMap(const TextureHandle & image) { return setMap(TextureType::ALBEDO, image); }
        Desc & setEmissionMap(const TextureHandle & image) { return setMap(TextureType::EMISSION, image); }
        Desc & setNormalMap(const TextureHandle & image) { return setMap(TextureType::NORMAL, image); }
        Desc & setOrmMap(const TextureHandle & image) { return setMap(TextureType::ORM, image); }
        Desc & setDepthMap(const TextureHandle & image) { return setMap(TextureType::DEPTH, image); }

        bool operator==(const Desc & rhs) const {
            static_assert(0 == static_cast<size_t>(offsetof(Desc, albedo)));
            const float * p1    = albedo;
            const float * p2    = rhs.albedo;
            size_t        count = static_cast<size_t>(offsetof(Desc, maps) / sizeof(float));
            for (size_t i = 0; i < count; ++i) {
                if (p1[i] != p2[i]) return false;
            }
            for (size_t i = 0; i < std::size(maps); ++i) {
                if (maps[i] != rhs.maps[i]) return false;
            }
            return true;
        }

        bool operator!=(const Desc & rhs) const { return !operator==(rhs); }

        /// this less operator is to make it easier to put Material into associative containors
        bool operator<(const Desc & rhs) const {
            static_assert(0 == static_cast<size_t>(offsetof(Desc, albedo)));
            const float * p1    = albedo;
            const float * p2    = rhs.albedo;
            size_t        count = static_cast<size_t>(offsetof(Desc, maps) / sizeof(float));
            for (size_t i = 0; i < count; ++i) {
                if (p1[i] != p2[i]) return p1[i] < p2[i];
            }
            for (size_t i = 0; i < std::size(maps); ++i) {
                if (maps[i] != rhs.maps[i]) return maps[i] < rhs.maps[i];
            }
            return false;
        }
    };

    const Desc & desc() const { return _desc; }
    void         setDesc(const Desc & d) { _desc = d; }

protected:
    Material(const RootConstructParameters & rcp, const Desc & d): Root(rcp), _desc(d) {}

private:
    Desc _desc;
};

/// Typed pointer for strided array. "Strided" mean each element in the array might not be tightly packed.
/// Byte distance from one element to next is specified by the stride member. Note that in very rared occasion,
/// the stride could be smaller then the size of each element, or even 0
template<typename T>
struct StridedBuffer {
    T *    ptr    = nullptr; ///< the pointer.
    size_t stride = 0;       ///< byte distance from one element to the next.

    bool empty() const { return !ptr; }

    void clear() {
        ptr    = nullptr;
        stride = 0;
    }

    template<typename T2 = T>
    T2 * next(T2 * p) const {
        return (T *) ((const uint8_t *) p + stride);
    }

    template<typename T2 = T>
    T2 & operator[](size_t i) {
        return *(T2 *) ((const uint8_t *) ptr + i * stride);
    }

    template<typename T2 = T>
    const T2 & operator[](size_t i) const {
        return *(const T2 *) ((const uint8_t *) ptr + i * stride);
    }

    operator bool() const { return !!ptr; }
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// defines a 3D mesh in it's own coordinate space.
class Mesh : public Root {
public:
    /// Parameters to morph() method. For data/properties that you don't want to morph or animate, simplly leave
    /// them empty. Moreph positions only when necessary, since it'll triger BVH rebuild/update.
    struct MorphParameters {
        StridedBuffer<const float> positions; ///< position array in float3 format.
        StridedBuffer<const float> normals;   ///< normal array in flaot3 format.
        StridedBuffer<const float> texcoords; ///< texture coordinates array in float2 format.
        StridedBuffer<const float> tangents;  ///< tangent vector in float3 format.
    };

    /// Update mesh shape and/or appearance w/o changing mesh topology or number of vertices.
    virtual void morph(const MorphParameters &) = 0;

    struct VertexFieldOffsets {
        // offsets in bytes from the start of the Vertex
        size_t position = (size_t) -1;
        size_t normal   = (size_t) -1;
    };

    struct VertexBuffer {
        VkBuffer           buffer;
        size_t             stride;      ///< Distance in bytes between the start of a vertex and the start of the next vertex
        size_t             vertexCount; ///< Number of vertices in the buffer
        size_t             offset;      ///< Offset in bytes of the first vertex from the start of the buffer
        VertexFieldOffsets offsets;
    };

    /// Update mesh shape and/or appearance w/o changing mesh topology or number of vertices.
    virtual void morph(const VertexBuffer &, VkCommandBuffer) = 0;
    virtual void syncVertexFromBuffer()                       = 0;

    /// retrieve a copy of vertex positions
    virtual Blob<Eigen::Vector3f> positions() const = 0;

    /// retrieve a copy of vertex normals
    virtual Blob<Eigen::Vector3f> normals() const = 0;

    /// retrieve a copy of vertex texcoords
    virtual Blob<Eigen::Vector2f> texcoords() const = 0;

    /// retrieve a copy of vertex tangents
    virtual Blob<Eigen::Vector3f> tangents() const = 0;

    /// retrieve a copy of index buffer
    virtual Blob<uint32_t> indices() const = 0;

protected:
    using Root::Root;
};

class Scene;
class NodeComponent;

/// --------------------------------------------------------------------------------------------------------------------
///
/// represents a node in a scene graph
class Node : public Root {
public:
    PH_NO_COPY(Node);
    PH_NO_MOVE(Node);

    /// each node belongs to one and only one scene.
    virtual Scene & scene() const = 0;

    /// get the current local->parent transform of the node.
    virtual const NodeTransform & transform() const = 0;

    /// Changes the local transform of this node.
    virtual void setTransform(const NodeTransform & localToParent) = 0;

    /// Get the current local->world transform of the node.
    virtual const NodeTransform & worldTransform() const = 0;

    /// An convenient function to directly set local->world transform of the node.
    virtual void setWorldTransform(const NodeTransform & worldToParent) = 0;

    // TODO: consider move node component management method here (like addMeshView(), addLight() and addCamera())
    virtual Node * parent() const = 0;

    // Returns list of node componnents
    virtual ConstRange<NodeComponent *> components() const = 0;

    virtual bool worldTransformDirty() const = 0;

protected:
    using Root::Root;
};

// ---------------------------------------------------------------------------------------------------------------------
enum class NodeComponentType {
    MeshView,
    Camera,
    Light,
};

// ---------------------------------------------------------------------------------------------------------------------
//
/// Represents a trait assigned to a given node.
class NodeComponent : public Root {
public:
    PH_NO_COPY(NodeComponent);
    PH_NO_MOVE(NodeComponent);

    /// constructor
    NodeComponent(const ph::rt::Root::RootConstructParameters & rcp, Node & node, NodeComponentType type);

    /// destructor
    virtual ~NodeComponent() = default;

    /// The node component is attached to one and only one node.
    Node & node() const { return _node; }

    /// Returns the type of the node component
    NodeComponentType type() const { return _type; }

private:
    /// Node that this component has been attached to.
    Node & _node;

    NodeComponentType _type;
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// Renders one mesh in the location of the owning node.
class MeshView : public NodeComponent {
public:
    /// @return The mesh attached to this view.
    virtual Mesh & mesh() const = 0;

    /// @return The material attached to this view
    virtual Material & material() const = 0;

protected:
    using NodeComponent::NodeComponent;
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// Represents a camera in the scene.
/// TODO: Only perspective cameras are currently supported.
/// Orthographic cameras will currently be ignored.
class Camera : public NodeComponent {
public:
    /// Define camera handness.
    enum Handness {
        LEFT_HANDED,
        RIGHT_HANDED,
    };

    /// Camera descriptor
    struct Desc {
        /// vertical field of view in radian. Set to zero to make an orthographic camera.
        float yFieldOfView = 1.0f;

        /// Define handness of the camera. Default is right handed.
        Handness handness = RIGHT_HANDED;

        /// distance of near plane
        float zNear = 0.1f;

        /// distance of far plane
        float zFar = 10000.0f;
    };

    PH_NO_COPY(Camera);
    PH_NO_MOVE(Camera);
    virtual ~Camera() = default;

    const Desc & desc() const { return _desc; }

    /// Used to modify the camera's variables, all at once.
    void reset(const Desc & desc) { _desc = desc; }

    /// Calculate projection matrix of the camera.
    /// \param displayAspectRatio =(displayW / displayH).
    Eigen::Matrix4f calculateProj(float displayAspectRatio = 1.0f) const;

protected:
    Camera(const ph::rt::Root::RootConstructParameters & rcp, Node & node, const Desc & desc)
        : NodeComponent(rcp, node, NodeComponentType::Camera), _desc(desc) {};

private:
    Desc _desc;
};

// ---------------------------------------------------------------------------------------------------------------------
//
/// Represents a light in the scene.
/// TODO: Only point light is currently supported,
/// the other light types will be ignored right now.
class Light : public NodeComponent {
public:
    /// Light Type
    enum Type {
        OFF,         ///< light is turned off.
        POINT,       ///< omnidirectional point light
        DIRECTIONAL, ///< directional light
        SPOT,        ///< spot light
    };

    /// Point light specific fields.
    struct Point {
        /// How far the light can reach, in world space.
        float range;
    };

    /// Directional light specific fields.
    struct Directional {
        /// Direction of the light source in local space.
        /// This will be transformed to the node's world space
        /// when the light is processed.
        float direction[3];

        // bounding box of the scene that this light needs to cover.
        float bboxMin[3];
        float bboxMax[3];

        Directional & setDir(const Eigen::Vector3f & dir) {
            direction[0] = dir.x();
            direction[1] = dir.y();
            direction[2] = dir.z();
            return *this;
        }

        Directional & setBBox(const Eigen::AlignedBox3f & bbox) {
            bboxMin[0] = bbox.min().x();
            bboxMin[1] = bbox.min().y();
            bboxMin[2] = bbox.min().z();
            bboxMax[0] = bbox.max().x();
            bboxMax[1] = bbox.max().y();
            bboxMax[2] = bbox.max().z();
            return *this;
        }
    };

    /// Spot light specific fields.
    struct Spot {
        /// Direction of the light source in local space.
        /// This will be transformed to the node's world space
        /// when the light is processed.
        float direction[3];

        /// Angular falloff inner angle, in radian.
        float inner;

        /// Angular falloff outer angle, in radian.
        float outer;

        /// How far the light can reach, in world space.
        float range;

        Spot & setDir(const Eigen::Vector3f & dir) {
            direction[0] = dir.x();
            direction[1] = dir.y();
            direction[2] = dir.z();
            return *this;
        }
    };

    struct Desc {
        /// Type of the light.
        Type type = POINT;

        /// The dimensions of area lights (sphere, rectangle, disk)
        // For point lights, dimension.x is the radius of the sphere.
        // For directional lights, dimension.xy are the width and height of the quad.
        // For spot lights, dimension.xy are the width and height of the ellipse.
        float dimension[2] {0.f, 0.f};

        /// color/brightness of the light
        float emission[3] {1.0f, 1.0f, 1.0f};

        union {
            Point       point {};
            Directional directional;
            Spot        spot;
        };

        Desc & setEmission(const Eigen::Vector3f & v) {
            emission[0] = v.x();
            emission[1] = v.y();
            emission[2] = v.z();
            return *this;
        }

        Desc & setEmission(float r, float g, float b) {
            emission[0] = r;
            emission[1] = g;
            emission[2] = b;
            return *this;
        }
    };

    /// Shadow map texture handle. For point light, this shoud be an
    /// cubemap. For spot and directional light, just regular 2D map.
    Material::TextureHandle shadowMap;

    PH_NO_COPY(Light);
    PH_NO_MOVE(Light);
    virtual ~Light() = default;

    /// @return Most of the light's variables.
    const Desc & desc() const { return _desc; }

    /// Allow us to replace the light's variables.
    void reset(const Desc & desc) { _desc = desc; }

protected:
    Light(const ph::rt::Root::RootConstructParameters & rcp, Node & node, const Desc & desc): NodeComponent(rcp, node, NodeComponentType::Light), _desc(desc) {}

private:
    /// Holds most fo the light's variables.
    Desc _desc;
};

/// A helper utility for 16-bit and 32-bit index buffer.
struct IndexBuffer {
    /// Pointer to the array holding the indices.
    const void * data = nullptr;
    /// The number of indices stored in data.
    size_t count = 0;
    /// The number of bytes each index is made from and
    /// the gap between each element.
    /// Must be either 2 for 16-bit or 4 for 32-bit integers.
    size_t stride = 2;

    /// Default constructor
    IndexBuffer() = default;

    /// construct from initializer list
    template<typename T>
    IndexBuffer(std::initializer_list<T> r): data(r.begin()), count(r.size()), stride(sizeof(T)) {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4);
    }

    /// construct from typed range
    template<typename T>
    IndexBuffer(const ConstRange<T> & r): data(r.data()), count(r.size()), stride(sizeof(T)) {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4);
    }

    /// construct from typed pointer
    template<typename T>
    IndexBuffer(const T * p, size_t c): data(p), count(c), stride(sizeof(T)) {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4);
    }

    /// construct from void pointer with stride
    IndexBuffer(const void * p, size_t c, size_t s): data(p), count(c), stride(s) { PH_ASSERT(2 == s || 4 == s); }

    bool empty() const { return !data || !count; }

    uint32_t at(size_t i) const {
        PH_ASSERT(data && (i < count));
        PH_ASSERT((2 == stride) || (4 == stride));
        auto ptr = (const uint8_t *) data + stride * i;
        return (2 == stride) ? (*(const uint16_t *) ptr) : (*(const uint32_t *) ptr);
    }

    uint32_t operator[](size_t i) const { return at(i); }

    /// verify all indices are in valid range. return false, if there are outliers.
    bool verify(size_t min, size_t max) const {
        for (size_t i = 0; i < count; ++i) {
            auto index = at(i);
            if (index < min || index > max) {
                PH_LOGE("index[%zu] = %zu is out of the valid range [%zu, %zu]", i, index, min, max);
                return false;
            }
        }
        return true;
    }
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// Represents ray traced scene.
///
/// Internally, this class maintains a scene graph hierarchy composed by Node.
class JediPbrt3Exporter;
class Scene : public Root {
protected:
    using Root::Root;

public:
    /// Add mesh objects to the scene. Returns the node that represents the mesh.
    /// The returned node will be released along with the scene. No need to
    /// explicitly release it.
    ///
    /// To add an area light source to the scene, add a mesh with emissive material.
    //@{
    struct NodeAddingParameters {
        Node *        parent {};                             /// Parent node will be attached to.
        NodeTransform transform {NodeTransform::Identity()}; ///< transform to parent space
    };
    virtual auto addNode(const NodeAddingParameters &) -> Node * = 0;
    //@}

    /// Add mesh view to the scene.
    //@{
    struct MeshViewCreateParameters {
        /// The node to which this component will be added.
        Node * node = nullptr;

        /// The mesh this is rendering.
        Mesh * mesh = nullptr;

        /// Material of the mesh.
        /// \todo multiple materials per mesh.
        Material * material = nullptr;

        std::vector<ph::rt::Node *>  skin;
        std::vector<Eigen::Matrix4f> inverseBindMatrices;
    };
    virtual MeshView * addMeshView(const MeshViewCreateParameters &) = 0;
    //@}

    /// Add light to the scene
    //@{
    struct LightCreateParameters {
        /// The node to which this component will be added.
        Node * node = nullptr;
        /// Variables object will be set to.
        Light::Desc desc;
    };
    virtual auto addLight(const LightCreateParameters &) -> Light * = 0;
    //@}

    /// Add camera to the scene.
    //@{
    struct CameraCreateParameters {
        /// The node to which this component will be added.
        Node * node = nullptr;

        /// Variables object will be set to.
        Camera::Desc desc;

        /// handness
    };
    virtual auto addCamera(const CameraCreateParameters &) -> Camera * = 0;
    //@}

    /// Refresh internal data structure to be prepared for command buffer recording.
    virtual void prepareForRecording(VkCommandBuffer cb) = 0;

    /// Performance statistics
    struct PerfStats {
        Blob<va::AsyncTimestamps::QueryResult> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

    virtual auto getMaterials() const -> std::vector<Material *> = 0;

    // slow material update. do not use during runtime
    virtual void debug_updateMaterial(Material *, Material::Desc) = 0;
    virtual void updateMaterials(VkCommandBuffer cmdbuf)          = 0;
};

/// --------------------------------------------------------------------------------------------------------------------
/// point light shadow map renderer
struct ShadowMapRenderPack : Root {
    struct RecordParameters {
        /// store all rendering commands. The buffer must be in recording state.
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

        /// The light source. light->shadowMap must point to a valid cubemap.
        Light * light;
    };
    virtual void record(const RecordParameters &) = 0;

    /// Performance statistics of the shadow render pack.
    struct PerfStats {
        Blob<va::AsyncTimestamps::QueryResult> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

protected:
    using Root::Root;
};

/// --------------------------------------------------------------------------------------------------------------------
/// a main ray tracing renderer
struct RayTracingRenderPack : Root {
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

        /// Alpha-blended shadow mode
        RAY_TRACED_ALPHA,

        /// number of shadow modes
        NUM_SHADOW_MODES,
    };

    /// Helper function to append shadow mode to the end of a stream.
    friend inline std::ostream & operator<<(std::ostream & os, ShadowMode mode) {
        switch (mode) {
        case ShadowMode::RAY_TRACED:
            os << "RAY_TRACED(";
            break;
        case ShadowMode::RASTERIZED:
            os << "RASTERIZED(";
            break;
        case ShadowMode::REFINED:
            os << "REFINED   (";
            break;
        case ShadowMode::DEBUG:
            os << "DEBUG     (";
            break;
        default:
            os << "(";
            break;
        }
        os << (int) mode << ")";
        return os;
    }

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

        /// camera parameters. While multiple cameras can be present in a scene,
        /// only the one selected here will be used.
        //@{
        ph::rt::Camera * camera = nullptr;
        //@}

        /// Ambient light term
        Eigen::Vector3f ambientLight = {0.f, 0.f, 0.f};

        /// Use this to adjust suaturation of the final render color.
        float saturation = 1.0f;

        /// Gamma correction. Note that his gamma correction is independent of LINEAR -> sRGB conversion.
        float gamma = 1.0;

        /// Define the shadow rendering mode. If parameter is ignored by the rasterized render pack.
        ShadowMode shadowMode = ShadowMode::RAY_TRACED;

        /// Define the shadow rendering mode for reflections.
        ShadowMode reflectedShadowMode = ShadowMode::RASTERIZED;

        /// Toggle on the heat map to illustrate the number of traversal steps in a path/shadow traced query
        bool enableHeatMap = false;

        /// Sets the upper end of the range threshold that determines a specific calibration for how our heat map appears
        float maxNumTraversalSteps = 200.f;

        /// roughness cutoff for ray traced reflection. A surface with rougnness smaller than this value will have
        /// ray traced reflection effect.
        float reflectionRoughnessCutoff = 0.0f;

        // Rotation of skybox about the Y axis in radians
        float skyboxRotation = 0.f;

        // Path tracing specific config
        uint32_t reflectionMode   = 0;
        uint32_t backscatterMode  = 0;
        float    jitterAmount     = 0.f;
        float    subsurfaceChance = 0.f;

        /// Max number of diffuse bounces
        uint32_t maxDiffuseBounces = 3;

        /// Max number of specular bounces.
        uint32_t maxSpecularBounces = 5;

        /// Diffuse irradiance map with mipmap chain.
        Material::TextureHandle irradianceMap;

        /// Prefiltered reflection map with mipmap chain encoded based on roughness.
        Material::TextureHandle reflectionMap;

        // True if time-based accumulation is enabled (maxSpp = -1) and has completed
        bool timeAccumDone = false;
    };
    virtual void record(const RecordParameters &) = 0;

    /// Performance statistics
    struct PerfStats {
        Blob<va::AsyncTimestamps::QueryResult> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

    // Todo: only needed for path tracing
    virtual float accumulationProgress(int, uint64_t) { return 0.f; }

    virtual void prepareForRecording(const RecordParameters &) = 0;

protected:
    using Root::Root;
};

/// Represents the whole ray traced world. This is also the factory class for all other objects used by ray tracing.
/// This class is multithread safe.
/// TODO: consider move all creation/deletion function to their own classes.
class World {
public:
    PH_NO_COPY(World);
    PH_NO_MOVE(World);

    /// World factory
    //@{
    struct WorldCreateParameters {
        /// This is the submision proxy that the rest of the RT module will use
        /// to submit graphics and compute workload to GPU. An important note is that when a method in rt.h accepts
        /// VkCommandBuffer parameter, then all GPU work of that method will _NOT_ go through this proxy, but simply
        /// be recorded into the command buffer.
        va::VulkanSubmissionProxy & vsp;

        /// When using BVH of type AABB_CPU, the RT module will try loading pre-built BVH data from asset folder
        /// named "bvh-cache". On desktop, you can use this parameter to specify where that bvh-cache folder really
        /// is on disk. For example, if your bvh cache files are in c:\\data\\bvh-cache folder, then add "c:\\data"
        /// folder to this parameter.
        /// On Android, this parameter is ignored. You'll need to specify the bvh-cache folder via Android asset
        /// manager.
        std::vector<std::string> assetFolders;

        /// If this is not null, then the RT module use it report internal CPU timing informations. This is useful
        /// if you are profiling your rendering performance.
        SimpleCpuFrameTimes * cpuFrameTimes = nullptr;

        /// Set to true to enable collection RT modules' internal GPU timestamps. You can query those timestamps
        /// via the perfStats() call of Scene and RenderPack classes.
        bool enableGpuTimestamps = false;

        /// Type of the bounding volumn hierachy.
        enum BvhType {
            /// This is the default type that relies on VK_KHR_ray_query extension.
            KHR_RAY_QUERY = 0,

            /// This is a softare BVH implementation. Most of the computation is done by CPU. It does not require
            /// any special VK extensions.
            AABB_CPU,

            /// This is an experimental compute shader based BVH implementation. It is still at very early stage.
            /// Use at your own risk.
            AABB_GPU,

            /// Number of BVH types.
            NUM_BVH_TYPES,
        };
        BvhType bvhType = KHR_RAY_QUERY;
    };
    static PH_API auto createWorld(const WorldCreateParameters &) -> World *;
    static PH_API void deleteWorld(World *&);
    static auto        createUniqueWorld(const WorldCreateParameters & wcp) {
        return std::unique_ptr<World, void (*)(World *)>(createWorld(wcp), [](World * p) { World::deleteWorld(p); });
    }
    //@}

    /// Material factory
    //@{
    using MaterialCreateParameters                                              = Material::Desc;
    virtual auto createMaterial(const MaterialCreateParameters &) -> Material * = 0;
    virtual void deleteMaterial(Material *&)                                    = 0;
    //@}

    /// Mesh factory
    /// TODO: support indexed mesh.
    //@{
    struct MeshCreateParameters {
        size_t                     count = 0; ///< number of vertices in the mesh
        StridedBuffer<const float> positions; ///< position array in float3 format
        StridedBuffer<const float> normals;   ///< normal array in float3 format
        StridedBuffer<const float> texcoords; ///< texture coordinates array in float2 format. this vector is optional.
        StridedBuffer<const float> tangents;  ///< tangent vector in float3 format. this vector is optional.
        IndexBuffer                indices;   ///< index buffer (optional)
    };
    virtual auto createMesh(const MeshCreateParameters &) -> Mesh * = 0;
    virtual void deleteMesh(Mesh *&)                                = 0;

    //@}

    /// scene factory
    //@{

    /// scene creation parameters
    struct SceneCreateParameters {
        // TBD
    };
    virtual auto createScene(const SceneCreateParameters &) -> Scene * = 0;
    // struct SceneLoadParameters {
    //     AssetSystem & as;
    //     const char * sceneAssetName; ///< asset name of the scene.
    // };
    // auto loadScene(const SceneLoadParameters &) -> Scene*;
    virtual void deleteScene(Scene *&) = 0;
    //@}

    /// screen space shadow map render pack
    //@{
    struct ShadowMapRenderPackCreateParameters {
        uint32_t      shadowMapSize   = 0;
        VkFormat      shadowMapFormat = VK_FORMAT_UNDEFINED;
        VkImageLayout shadowMapLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        ShadowMapRenderPackCreateParameters & set(uint32_t size, VkFormat format, VkImageLayout layout) {
            shadowMapSize   = size;
            shadowMapFormat = format;
            shadowMapLayout = layout;
            return *this;
        }
    };
    virtual auto createShadowMapRenderPack(const ShadowMapRenderPackCreateParameters &) -> ShadowMapRenderPack * = 0;
    virtual void deleteShadowMapRenderPack(ShadowMapRenderPack *&)                                               = 0;
    //@}

    /// main ray tracing render pack creation

    //@{
    struct RayTracingRenderPackCreateParameters {
        /// Defines the main rendering mode of the render pack.
        enum Mode {
            /// Render the scene with rasterization technique only. It is like a "RT OFF" mode.
            RASTERIZED,

            /// Render the scene with a full path tracer.
            PATH_TRACING,

            /// Render scene with limited ray tracing effect, but w/o any random noise.
            NOISE_FREE,

            /// Render scene with everything rasterized except shadow being ray traced.
            SHADOW_TRACING,
        };
        Mode mode = NOISE_FREE;

        VkFormat targetFormat = VK_FORMAT_UNDEFINED;

        uint32_t targetWidth = 0, targetHeight = 0;

        // By default, the render pack determines if the target is in linear or sRGB color space based on
        // target format. This flag is to deal with the case that swapchain image can be set to be in sRGB
        // color space, regardless of the image format.
        bool targetIsSRGB = false;

        /// Define the input layout of the target image. After the rendering, the target image
        /// will always be transferred to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
        /// TODO: move to RecordParameters
        VkImageLayout targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        /// Specify the rendering viewport.
        VkViewport viewport {};

        float clearColor[4] = {0.f, 0.f, 0.f, 1.f}; ///< clear color. ignored if clear is false.
        bool  clear         = true;                 ///< if set to true to clear the target view.

        /// this group of parameters are only used in PATH_TRACING mode.
        //@{
        int32_t spp    = 1; ///< samples per pixel. Only used in PATH_TRACING mode.
        int32_t maxSpp = 0; ///< max spp. Only used in PATH_TRACING mode when accumulation is enabled and scene
                            ///< is not updating.
                            ///< If it is 0, accumulates infinitely.
                            ///< If it is -1, accumulates as many samples as possible within 3s.
        bool accum = false; ///< Set to true to enagble acculmulative rendering. Only used in PATH_TRACING mode.
        //@}

        RayTracingRenderPackCreateParameters & setTarget(VkFormat format, uint32_t width, uint32_t height, VkImageLayout layout) {
            targetFormat = format;
            targetWidth  = width;
            targetHeight = height;
            targetLayout = layout;
            return *this;
        }

        RayTracingRenderPackCreateParameters & setViewport(float x, float y, float w, float h) {
            viewport.x        = x;
            viewport.y        = y;
            viewport.width    = w;
            viewport.height   = h;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            return *this;
        }

        RayTracingRenderPackCreateParameters & setClear(bool clear_, const float * color = nullptr) {
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

        RayTracingRenderPackCreateParameters & setTracing(uint32_t samplesPerPixel, uint32_t maxSamplesPerPixel, bool accumulative) {
            spp    = samplesPerPixel;
            maxSpp = accumulative ? maxSamplesPerPixel : samplesPerPixel;
            accum  = accumulative;
            return *this;
        }
    };
    virtual auto createRayTracingRenderPack(const RayTracingRenderPackCreateParameters &) -> RayTracingRenderPack * = 0;
    virtual void deleteRayTracingRenderPack(RayTracingRenderPack *&)                                                = 0;
    //@}

    /// templated creator
    template<typename T>
    auto create(const T & creationParameters) {
        if constexpr (std::is_same_v<T, MaterialCreateParameters>)
            return createMaterial(creationParameters);
        else if constexpr (std::is_same_v<T, MeshCreateParameters>)
            return createMesh(creationParameters);
        else if constexpr (std::is_same_v<T, SceneCreateParameters>)
            return createScene(creationParameters);
        else if constexpr (std::is_same_v<T, ShadowMapRenderPackCreateParameters>)
            return createShadowMapRenderPack(creationParameters);
        else if constexpr (std::is_same_v<T, RayTracingRenderPackCreateParameters>)
            return createRayTracingRenderPack(creationParameters);
        else
            static_assert(AlwaysFalse<T>);
    }

    /// templated creator
    template<typename T>
    auto create(const char * name, const T & creationParameters) {
        auto p = create(creationParameters);
        if (p) p->name = name;
        return p;
    }

    /// templated deletor
    template<typename T>
    static void destroy(T *& p) {
        if (!p) return;
        if constexpr (std::is_same_v<T, World>)
            World::deleteWorld(p);
        else if constexpr (std::is_same_v<T, Material>)
            p->world()->deleteMaterial(p);
        else if constexpr (std::is_same_v<T, Mesh>)
            p->world()->deleteMesh(p);
        else if constexpr (std::is_same_v<T, Scene>)
            p->world()->deleteScene(p);
        else if constexpr (std::is_same_v<T, ShadowMapRenderPackCreateParameters>)
            p->world()->deleteShadowMapRenderPack(p);
        else if constexpr (std::is_same_v<T, RayTracingRenderPackCreateParameters>)
            p->world()->deleteRayTracingRenderPack(p);
        else
            static_assert(AlwaysFalse<T>);
    }

protected:
    World()          = default;
    virtual ~World() = default;
};

/// This is an utility class to export RT scene to PBRT3 format. It is currently only used
/// for internal testing. Please ignore it your own code.
/// \todo Move this out of rt.h (into <sdkroot>/dev folder)
class JediPbrt3Exporter {
public:
    struct InfiniteLightSetup {
        std::string     mapAssetPath;
        Eigen::Vector3f ambient;
    };

    virtual auto getStringStream() -> std::string                                                     = 0;
    virtual void exportCameraAndFilm(const ph::rt::NodeTransform &, const ph::rt::Camera *, int, int) = 0;
    virtual void exportGlobalSetup(int)                                                               = 0;
    virtual void exportWorld(const ph::rt::Scene *, const InfiniteLightSetup &)                       = 0;
    virtual void addTexture(const ph::va::ImageObject &, std::string)                                 = 0;

    static PH_API auto createExporter() -> JediPbrt3Exporter *;
    static PH_API void deleteExporter(JediPbrt3Exporter *&);

protected:
    JediPbrt3Exporter()          = default;
    virtual ~JediPbrt3Exporter() = default;
};

/// A utility function to setup SimpleVulkanInstance::ConstructParameters to use hardware ray query feature
/// \param hw Whether to use hardware VK_KHR_ray_query extension. If set to false, then setup the construt
///           parameter for in-house compute shader based pipeline. In this case, return value is alwasy false.
/// \return   If the construction parameter is properly set for hardware rayquery. If false is returned,
///           then construction parameter structure is set to do in-house shader based pipeline.
PH_API bool setupInstanceConstructionForRayQuery(ph::va::SimpleVulkanInstance::ConstructParameters &, bool hw);

/// A utility function to setup SimpleVulkanDevice::ConstructParameters to for ray tracing
/// \param hw Whether to use hardware VK_KHR_ray_query extension. If set to false, then setup the construt
///           parameter for in-house compute shader based pipeline. In this case, return value is alwasy false.
/// \return   If the construction parameter is properly set for hardware rayquery. If false is returned,
///           then construction parameter structure is set to do in-house shader based pipeline.
PH_API bool setupDeviceConstructionForRayQuery(ph::va::SimpleVulkanDevice::ConstructParameters &, bool hw);

/// RT module unit test function. Do _NOT_ call in your own code.
PH_API void unitTest();

} // namespace ph::rt

namespace std {

/// A functor that allows for hashing texture handles.
template<>
struct hash<ph::rt::Material::TextureHandle> {
    size_t operator()(const ph::rt::Material::TextureHandle & key) const {
        // Records the calculated hash of the texture handle.
        size_t hash = 7;

        // Hash the members.
        hash = 79 * hash + (size_t) key.image;
        hash = 79 * hash + (size_t) key.view;

        return hash;
    }
};

/// A functor that allows for hashing material descriptions.
template<>
struct hash<ph::rt::Material::Desc> {
    size_t operator()(const ph::rt::Material::Desc & key) const {
        // Make sure floats will fit inside the integral type we are copying the unmodified bytes to.
        // If on a platform that defines it differently than we expect, this method will need to be rewritten.
        static_assert(sizeof(size_t) >= sizeof(float));

        // Make sure albedo is at the very beginning of the descriptor since our float iteration won't work otherwise.
        static_assert(0 == static_cast<size_t>(offsetof(ph::rt::Material::Desc, albedo)));

        // Records the calculated hash of the texture handle.
        size_t hash = 7;

        // Hash the members.
        // Hash the float members first.
        // Get a pointer to the first float variable in the struct.
        const float * p1 = key.albedo;

        // Calculate how many floats there are in the struct between albedo and maps.
        size_t count = static_cast<size_t>(offsetof(ph::rt::Material::Desc, maps) / sizeof(float));

        // Iterate over all floats in the struct before maps.
        for (size_t i = 0; i < count; ++i) {
            // Copy the float bits to size_t. Anything the float doesn't take up will default to zero.
            size_t value = 0;
            std::memcpy(&value, p1 + i, sizeof(float));

            hash = 79 * hash + value;
        }

        // Used to calculate the hash of the texture handles.
        std::hash<ph::rt::Material::TextureHandle> textureHandleHasher;

        // Iterate all the textures in this material.
        for (size_t i = 0; i < std::size(key.maps); ++i) {
            // Add the hash of each texture.
            hash = 79 * hash + textureHandleHasher(key.maps[i]);
        }

        return hash;
    }
};

} // namespace std
