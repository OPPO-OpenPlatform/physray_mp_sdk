/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

// This is the main interface of the ray tracing model of the PhysRay SDK.

#pragma once

#include <vulkan/vulkan.h>
#include <string.h> // for memcpy
#include <cassert>

/// printf like macro to print error message
#ifndef Ph_RT_LOGE
    #define PH_RT_LOGE(...) (void) 0
#endif

#ifdef _MSC_VER
    #ifdef PH_RT_DLL_INTERNAL
        #define PH_RT_API __declspec(dllexport)
    #else
        #define PH_RT_API __declspec(dllimport)
    #endif
    #pragma warning(disable : 4201) // nameless struct/union
#else
    #ifdef PH_RT_DLL_INTERNAL
        #define PH_RT_API __attribute__((visibility("default")))
    #else
        #define PH_RT_API
    #endif
#endif

#define PH_RT_PROTECTED_CTOR_DTOR(name, parent)               \
protected:                                                    \
    name(const RootConstructParameters & rcp): parent(rcp) {} \
    ~name() override {}

/// determine C++ standard
#ifdef _MSC_VER
    #if _MSVC_LANG < 201103L
        #define PH_RT_CXX_STANDARD 1
    #elif _MSVC_LANG < 201402L
        #define PH_RT_CXX_STANDARD 11
    #elif _MSVC_LANG < 201703L
        #define PH_RT_CXX_STANDARD 14
    #elif _MSVC_LANG < 202002L
        #define PH_RT_CXX_STANDARD 17
    #else
        #define PH_RT_CXX_STANDARD 20
    #endif
#else
    #if __cplusplus < 201103L
        #define PH_RT_CXX_STANDARD 1
    #elif __cplusplus < 201402L
        #define PH_RT_CXX_STANDARD 11
    #elif __cplusplus < 201703L
        #define PH_RT_CXX_STANDARD 14
    #elif __cplusplus < 202002L
        #define PH_RT_CXX_STANDARD 17
    #else
        #define PH_RT_CXX_STANDARD 20
    #endif
#endif

#if PH_RT_CXX_STANDARD >= 17
    #define PH_RT_CONSTEXPR constexpr
#else
    #define PH_RT_CONSTEXPR
#endif

#if PH_RT_CXX_STANDARD >= 11
    #define PH_RT_NO_COPY_NO_MOVE(X)       \
        X(const X &) = delete;             \
        X & operator=(const X &) = delete; \
        X(X &&)                  = delete; \
        X & operator=(X &&) = delete
    #include <initializer_list>
#else
    #define PH_RT_NO_COPY_NO_MOVE(X) \
        X(const X &);                \
        X & operator=(const X &);    \
        X(X &&);                     \
        X & operator=(X &&)
#endif

#include "rt/str.inl"
#include "rt/blob.inl"
#include "rt/geometry.inl"
#include "rt/guid.inl"

/// Main namespace for Ray Tracing module
namespace ph {
namespace rt {

class World;

template<class T, size_t N>
PH_RT_CONSTEXPR size_t countOf(const T (&)[N]) {
    return N;
}

/// --------------------------------------------------------------------------------------------------------------------
///
/// this is root class of everything in a ray traced world
class Root {
public:
    typedef int64_t Id; ///< type of the root object id. 0 is reserved as INVALID id.

    /// returns pointer to the world that this object belongs to.
    World * world() const { return _w; }

    /// Returns a unique ID of the object. 0 is reserved for invalid object.
    Id id() const {
        assert(0 != _id);
        return _id;
    };

    /// This name variable is reserved strictly for debugging and logging for library users.
    /// PhysRay-RT's internal code will not depends on it. Feel free to set it to whatever value.
    /// Note that we have to use a custom string class since we want to be completely independent of STL implementations.
    StrA name;

    struct RootConstructParameters {
        World * w;
        Id      id;
    };

    /// Returns a copy of the user data.
    virtual Blob<uint8_t> userData(const Guid & guid) const = 0;

    /// Store a copy of user defined blob data. Set either data or length to zero to erase the data from the current object.
    virtual void setUserData(const Guid & guid, const void * data, size_t length) = 0;

protected:
    Root(const RootConstructParameters & p): _w(p.w), _id(p.id) {
        assert(_w);
        assert(_id != 0);
    }

    virtual ~Root() {}

private:
    World * _w;
    Id      _id;

    PH_RT_NO_COPY_NO_MOVE(Root);
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// defines a material
class Material : public Root {
public:
    enum TextureType {
        ALBEDO = 0, // Diffuse albedo map.
        NORMAL,     // Normal map.
        ORM,        // Combined occlusion(R)-roughness(G)-metalness(B) map.
        EMISSION,   // Emissive texture map. Also used for subsurface scattering, in which case (A) is the sss amount.
        COUNT,
    };

    /// Allows textures to be loaded into vulkan.
    ///
    /// Supports std::hash.
    struct TextureHandle {
        VkImage         image;
        VkImageView     view;
        VkImageViewType viewType;
        VkFormat        format;
        VkExtent3D      extent; // if not zero, indicates size of the base level.

        PH_RT_CONSTEXPR TextureHandle(VkImage i = 0, VkImageView v = 0, VkImageViewType t = VK_IMAGE_VIEW_TYPE_2D, VkFormat f = VK_FORMAT_UNDEFINED,
                                      uint32_t w = 0, uint32_t h = 0, uint32_t d = 0)
            : image(i), view(v), viewType(t), format(f)
#if PH_RT_CXX_STANDARD >= 17
              // this is for constexpr to work
              ,
              extent {w, h, d} {
#else
        {
            extent.width  = w;
            extent.height = h;
            extent.depth  = d;
#endif
              }

              PH_RT_CONSTEXPR TextureHandle(const ph::va::ImageObject & i)
            : image(i.image), view(i.view), viewType(i.viewType), format(i.ci.format), extent(i.ci.extent) {
        }
        static PH_RT_CONSTEXPR TextureHandle EMPTY_2D() { return TextureHandle(0, 0, VK_IMAGE_VIEW_TYPE_2D); }

        static PH_RT_CONSTEXPR TextureHandle EMPTY_CUBE() { return TextureHandle(0, 0, VK_IMAGE_VIEW_TYPE_CUBE); }

        /// @return true if the two texture handles point to the same image.
        bool operator==(const TextureHandle & rhs) const {
            // If all variables match, return true.
            return image == rhs.image && view == rhs.view;
        }

        /// @return true if the two texture handles do NOT point to the same image.
        bool operator!=(const TextureHandle & rhs) const { return !operator==(rhs); }

        /// Provides a natural ordering for texture handles for the purposes of ordered containers.
        bool operator<(const TextureHandle & rhs) const {
            if (image < rhs.image) return true;
            if (image > rhs.image) return false;
            return view < rhs.view;
        }

        /// Cast to bool
        operator bool() const { return !empty(); }

        /// @return true if this texture handle does not point to a texture.
        bool empty() const {
            // Return true if this view handle is empty.
            return view == 0;
        }
    };

    /// Material description
    struct Desc {
        /// Commonly used PBR surface properties.
        /// See https://google.github.io/filament/Filament.md.html for a good explanation of parameters list here.
        //@{
        float albedo[3];
        float emissiveSaturation;
        float emissiveHueOffset;
        float opaque; // 0.0 : fully transparent, 1.0 : fully opaque;
        float emission[3];
        float roughness;
        float metalness;
        float ao;
        float clearcoat;          // Define how strong clear coat reflection is.
        float clearcoatRoughness; // roughness of the clear coat layer.

        float sss; // 0 = emissive, 1 = subsurface

        // Index of refraction. It is only meaningful for transparent materials.
        // IOR of commonly seen materials:
        //      vacuum  : 1.0 (by definition)
        //      air     : 1.000293
        //      water   : 1.333
        //      glass   : 1.4~1.7
        //      amber   : 1.55
        //      diamond : 2.417
        float ior;

        // float subsurface  = 0.f;
        // float specular = 0.5f;
        // float specularTint = 0.f;
        // Parameterized per https://google.github.io/filament/Filament.md.html#materialsystem/anisotropicmodel
        float anisotropic;
        // float sheen = 0.f;
        // float sheenTint = 0.5f;

        float sssamt;

        //@}

        TextureHandle maps[static_cast<size_t>(TextureType::COUNT)];

        Desc()
            : emissiveSaturation(1.0f), emissiveHueOffset(0.0f), opaque(1.0), roughness(1.f), metalness(0.f), ao(1.0f), clearcoat(0.0f),
              clearcoatRoughness(0.f), sss(0), ior(1.45f), anisotropic(0.f) {
            albedo[0] = albedo[1] = albedo[2] = 1.f;
            emission[0] = emission[1] = emission[2] = 0.f;
        }

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
        Desc & setSSSAmt(float t) {
            sssamt = t;
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
        bool   isLight() const { return (sss == 0.f) && ((emission[0] + emission[1] + emission[2]) > 0.f); }

        bool operator==(const Desc & rhs) const {
            static_assert(0 == static_cast<size_t>(offsetof(Desc, albedo)), "");
            const float * p1    = albedo;
            const float * p2    = rhs.albedo;
            size_t        count = static_cast<size_t>(offsetof(Desc, maps) / sizeof(float));
            for (size_t i = 0; i < count; ++i) {
                if (p1[i] != p2[i]) return false;
            }
            for (size_t i = 0; i < countOf(maps); ++i) {
                if (maps[i] != rhs.maps[i]) return false;
            }
            return true;
        }

        bool operator!=(const Desc & rhs) const { return !operator==(rhs); }

        /// this less operator is to make it easier to put Material into associative containers
        bool operator<(const Desc & rhs) const {
            static_assert(0 == static_cast<size_t>(offsetof(Desc, albedo)), "");
            const float * p1    = albedo;
            const float * p2    = rhs.albedo;
            size_t        count = static_cast<size_t>(offsetof(Desc, maps) / sizeof(float));
            for (size_t i = 0; i < count; ++i) {
                if (p1[i] != p2[i]) return p1[i] < p2[i];
            }
            for (size_t i = 0; i < countOf(maps); ++i) {
                if (maps[i] != rhs.maps[i]) return maps[i] < rhs.maps[i];
            }
            return false;
        }
    };

    const Desc & desc() const { return _desc; }

    virtual void setDesc(const Desc &) = 0;

protected:
    Material(const RootConstructParameters & p): Root(p) {}
    Desc _desc;
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// defines a 3D mesh in it's own coordinate space.
class Mesh : public Root {
public:
    struct VertexElement {
        VkBuffer buffer; ///< The GPU buffer that contains the vertex data. It must have VK_BUFFER_USAGE_TRANSFER_SRC_BIT usage flag.
        uint64_t offset; ///< Offset in bytes of the first element from the start of the buffer
        uint16_t stride; ///< Distance in bytes between the start of the element of a vertex and the start of the same element of the next vertex.
        VkFormat format;

        VertexElement(VkBuffer b = 0, uint64_t o = 0, uint16_t s = 0, VkFormat f = VK_FORMAT_R32G32B32_SFLOAT): buffer(b), offset(o), stride(s), format(f) {}

        void clear() { buffer = 0; }

        bool empty() const { return 0 == buffer; }
    };

    struct VertexInput {
        VertexElement position;
        VertexElement normal;
        VertexElement texcoord;
        VertexElement tangent;
    };

    /// Update mesh vertices w/o changing mesh topology or number of vertices.
    ///
    /// For elements that you don't want to update. Set the element index and/or offset to -1.
    ///
    /// This method simply remembers the new input layout. The actually data copy happens when Scene::refreshGpuData()
    /// is called the next time.
    ///
    /// Use destVertexBase and vertexCount to specify the target range of the mesh that you want to morph.
    /// (vertexCount == ~0) means to the end of the mesh.
    virtual void morph(const VertexInput & input, size_t destVertexBase = 0, size_t vertexCount = size_t(~0)) = 0;

protected:
    Mesh(const RootConstructParameters & p): Root(p) {}
};

class Scene;
class NodeComponent;

/// --------------------------------------------------------------------------------------------------------------------
///
/// represents a node in a scene graph
class Node : public Root {
public:
    /// each node belongs to one and only one scene.
    virtual Scene & scene() const = 0;

    /// @return The parent of this node. Null if this is a root node.
    virtual Node * parent() const = 0;

    /// Reset parent node with the following rules and restrictions:
    ///  - Set to null means attaching to root node of the scene.
    ///  - The new parent node can't be descendant of the current one.
    ///  - Can't set parent that belongs to different scene.
    ///  - Can't set parent to point to the current node itself.
    ///  - Can't use this method to change parent for the root node of the scene (which is always null).
    virtual void setParent(Node *) = 0;

    /// Returns list of node components
    virtual BlobProxy<NodeComponent *> components() const = 0;

    /// Attach a new node component (such as model, light) to the node.
    /// No op, if the component is already attached to it.
    ///
    /// Noet that certain component (such as Light) might have restrictions that it can only be attached to
    /// at most one node at any given time. In that case, attaching to new node w/o detaching it from current
    /// node is an invalid operation and will be ignored.
    virtual void attachComponent(NodeComponent *) = 0;

    /// Detach a component from the node.
    virtual void detachComponent(NodeComponent *) = 0;

    /// Set the nodeComponent at some index to be visible or not.
    virtual void setComponentVisible(size_t, bool) = 0;

    /// Get the nodeComponent at some index.
    virtual bool getComponentVisible(size_t) = 0;

    /// get the current local->parent transform of the node.
    virtual const Float3x4 & transform() const = 0;

    /// Changes the local transform of this node.
    virtual void setTransform(const Float3x4 & localToParent) = 0;

    /// Get the current local->world transform of the node.
    virtual const Float3x4 & worldTransform() const = 0;

    /// An convenient function to directly set local->world transform of the node.
    virtual void setWorldTransform(const Float3x4 & worldToParent) = 0;

    // Return the children of this Node
    virtual const Blob<Node *> children() = 0;

    // Instance mask for rayQuery.
    // This value is set as the mask value on BLASes created
    // using this Node.
    uint32_t maskToInstance = 0xFF;

protected:
    Node(const RootConstructParameters & p): Root(p) {}
};

class Light;
class Model;

// ---------------------------------------------------------------------------------------------------------------------
//
/// Represents a trait assigned to a given node.
class NodeComponent : public Root {
public:
    enum Type {
        MODEL,
        LIGHT,
    };

    /// destructor
    virtual ~NodeComponent() {};

    /// \return List of nodes that the component is attached to.
    virtual BlobProxy<Node *> nodes() const = 0;

    /// Returns the type of the node component
    Type type() const { return _type; }

    /// Cast to certain component class, with type check in debug build. Zero performance hit in release build.
    template<typename T>
    const T & castTo() const {
        assert(this->type() == T::componentTypeOfTheClass());
        return *(const T *) this;
    }

    /// Cast to certain component class, with type check in debug build. Zero performance hit in release build.
    template<typename T>
    T & castTo() {
        assert(this->type() == T::componentTypeOfTheClass());
        return *(T *) this;
    }

    /// Safely cast to certain component type with runtime check. Return's null pointer, if type is incorrect.
    template<typename T>
    const T * tryCastTo() const {
        return (this->type() == T::componentTypeOfTheClass()) ? *(const T *) this : nullptr;
    }

    /// Safely cast to certain component type with runtime check. Return's null pointer, if type is incorrect.
    template<typename T>
    T * tryCastTo() {
        return (this->type() == T::componentTypeOfTheClass()) ? *(T *) this : nullptr;
    }

protected:
    NodeComponent(const Root::RootConstructParameters & rcp, Type type): Root(rcp), _type(type) {}

    Type _type;
};

/// --------------------------------------------------------------------------------------------------------------------
///
/// Renders one mesh in the location of the owning node.
///
/// A model not attached to any node is considered invisible.
class Model : public NodeComponent {
public:
    /// Get type of the class at compile time
    static PH_RT_CONSTEXPR Type componentTypeOfTheClass() { return MODEL; }

    /// Represents a subset of a mesh with certain material
    struct Subset {
        Subset(Material * mat = nullptr, size_t idxBase = 0, size_t idxCount = size_t(~0)): material(mat), indexBase(idxBase), indexCount(idxCount) {}
        Material * material   = nullptr;    ///< If null, then inherited the model's material.
        size_t     indexBase  = 0;          ///< Index of the first index of the subset.
        size_t     indexCount = size_t(~0); ///< Number of indices in the subset. Set to size_t(~0) means to the end of the mesh.
        uint32_t   flags      = 0;          ///< Flags of the subset. If zero, then inhereted from the model.
    };

    /// @return The mesh that this model contains.
    virtual Mesh & mesh() const = 0;

    /// @return List of subsets of the model
    virtual BlobProxy<Subset> subsets() const = 0;

    /// instance flags.
    uint32_t flags = 0;

    /// Use this flag to mark the object (or the submesh) as reflective regardless the material attached to it.
    static constexpr uint32_t REFLECTIVE = 1LU;

protected:
    Model(const RootConstructParameters & rcp): NodeComponent(rcp, componentTypeOfTheClass()) {}
};

// ---------------------------------------------------------------------------------------------------------------------
//
/// Represents a light in the scene.
///
/// Light component can only be attached to at most one node a time.
///
/// A light not attached to any node is considered off.
class Light : public NodeComponent {
public:
    /// Get type of the class at compile time
    static PH_RT_CONSTEXPR Type componentTypeOfTheClass() { return LIGHT; }

    /// Light Type
    enum Type {
        OFF,         ///< light is turned off.
        POINT,       ///< omnidirectional point light
        DIRECTIONAL, ///< directional light
        SPOT,        ///< spot light
        GEOM,        ///< mesh light
    };

    /// Point light specific fields.
    struct Point {
        /// How far the light can reach, in world space.
    };

    /// Directional light specific fields.
    struct Directional {
        /// Direction of the light source in local space.
        /// This will be transformed to the node's world space
        /// when the light is processed.
        Float3 direction;

        // bounding box of the scene that this light needs to cover.
        Float3 bboxMin;
        Float3 bboxMax;

        Directional & setDir(const Float3 & dir) {
            direction = dir;
            return *this;
        }

        Directional & setDir(float x, float y, float z) {
            direction.x = x;
            direction.y = y;
            direction.z = z;
            return *this;
        }

        Directional & setBBox(const Float3 & min_, const Float3 & max_) {
            bboxMin = min_;
            bboxMax = max_;
            return *this;
        }
    };

    /// Spot light specific fields.
    struct Spot {
        /// Direction of the light source in local space.
        /// This will be transformed to the node's world space
        /// when the light is processed.
        Float3 direction;

        /// Angular falloff inner angle, in radian.
        float inner;

        /// Angular falloff outer angle, in radian.
        float outer;

        Spot & setDir(float x, float y, float z) {
            direction.x = x;
            direction.y = y;
            direction.z = z;
            return *this;
        }

        Spot & setDir(const Float3 & dir) {
            direction = dir;
            return *this;
        }

        Spot & setFalloff(float inner_, float outer_) {
            inner = inner_;
            outer = outer_;
            return *this;
        }
    };

    // geom lights only need node, which is already accessible
    // via NodeComponent's node() method
    struct Geom {};

    struct Desc {
        /// Type of the light.
        Type type;

        /// The dimensions of area lights (sphere, rectangle, disk)
        // For point lights, dimension.x is the radius of the sphere.
        // For directional lights, dimension.xy are the width and height of the quad.
        // For spot lights, dimension.xy are the width and height of the ellipse.
        // For mesh lights, dimension.xyz give the dimensions of the mesh's untransformed bbox
        Float3 dimension;

        /// color/brightness of the light
        Float3 emission;

        // Range now applies to all light types, since non-physical area lights
        // will be point-light-attenuated to provide greater artist control
        float range;

        // allow shadows to be cast from this light when true
        bool allowShadow;

        union {
            Point       point;
            Directional directional;
            Spot        spot;
            Geom        geom;
        };

        Desc & setType(Type t) {
            type = t;
            return *this;
        }

        Desc & setDimension(float w, float h, float d = 0.f) {
            dimension[0] = w;
            dimension[1] = h;
            dimension[2] = d;
            return *this;
        }

        Desc & setEmission(const Float3 & v) {
            emission = v;
            return *this;
        }

        Desc & setEmission(float r, float g, float b) {
            emission[0] = r;
            emission[1] = g;
            emission[2] = b;
            return *this;
        }

        Desc & setRange(float r) {
            range = r;
            return *this;
        }

        Desc & setPoint(const Point & p) {
            type  = POINT;
            point = p;
            return *this;
        }

        Desc & setDirectional(const Directional & d) {
            type        = DIRECTIONAL;
            directional = d;
            return *this;
        }

        Desc & setSpot(const Spot & s) {
            type = SPOT;
            spot = s;
            return *this;
        }

        Desc & setGeom(const Geom & g) {
            type = GEOM;
            geom = g;
            // dimensions for geometry lights
            // must be initialized to the untransformed
            // bbox of the associated mesh via setDimension
            return *this;
        }

        Desc(): type(POINT) {
            dimension.set(0.f, 0.f, 0.f);
            emission.set(1.0f, 1.0f, 1.0f);
            range       = 1.0f;
            allowShadow = true;
        }
    };

    /// Shadow map texture handle. For point light, this should be an
    /// cubemap. For spot and directional light, just regular 2D map.
    Material::TextureHandle shadowMap;
    float                   shadowMapBias;
    float                   shadowMapSlopeBias;

    /// @return Most of the light's variables.
    const Desc & desc() const { return _desc; }

    /// Allow us to replace the light's variables.
    virtual void reset(const Desc & desc) = 0;

protected:
    Light(const RootConstructParameters & rcp): NodeComponent(rcp, componentTypeOfTheClass()), shadowMapBias(0.001f), shadowMapSlopeBias(0.001f) {}

    virtual ~Light() = default;

    /// Holds most fo the light's variables.
    Desc _desc;
};

/// A helper utility for 16-bit and 32-bit index buffer.
struct IndexBuffer {
    IndexBuffer(): data(nullptr), count(0), stride(2) {}
    /// Pointer to the array holding the indices.
    const void * data;
    /// The number of indices stored in data.
    size_t count;
    /// The number of bytes each index is made from and
    /// the gap between each element.
    /// Must be either 2 for 16-bit or 4 for 32-bit integers.
    size_t stride;

#if PH_RT_CXX_STANDARD >= 11
    /// construct from initializer list
    template<typename T>
    IndexBuffer(std::initializer_list<T> r): data(r.begin()), count(r.size()), stride(sizeof(T)) {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4, "");
    }
#endif

    /// construct from typed range
    template<typename T>
    IndexBuffer(const BlobProxy<T> & r): data(r.data()), count(r.size()), stride(sizeof(T)) {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4, "");
    }

    /// construct from typed pointer
    template<typename T>
    IndexBuffer(const T * p, size_t c): data(p), count(c), stride(sizeof(T)) {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4, "");
    }

    /// construct from void pointer with stride
    IndexBuffer(const void * p, size_t c, size_t s): data(p), count(c), stride(s) { assert(2 == s || 4 == s); }

    bool empty() const { return !data || !count; }

    uint32_t at(size_t i) const {
        assert(data && (i < count));
        assert((2 == stride) || (4 == stride));
        auto ptr = (const uint8_t *) data + stride * i;
        return (2 == stride) ? (*(const uint16_t *) ptr) : (*(const uint32_t *) ptr);
    }

    uint32_t operator[](size_t i) const { return at(i); }

    /// verify all indices are in valid range. return false, if there are outliers.
    bool verify(size_t min, size_t max) const {
        for (size_t i = 0; i < count; ++i) {
            auto index = at(i);
            if (index < min || index > max) {
                PH_RT_LOGE("index[%zu] = %zu is out of the valid range [%zu, %zu]", i, index, min, max);
                return false;
            }
        }
        return true;
    }
};

struct NamedDuration {
    const char * name;       ///< operation name
    uint64_t     durationNs; ///< operation duration in nanoseconds.
};

// ---------------------------------------------------------------------------------------------------------------------
/// Represents ray traced scene.
class Scene : public Root {
protected:
    Scene(const RootConstructParameters & p): Root(p) {}

public:
    /// Parameter structure to add new node to scene.
    struct NodeCreateParameters {
        /// Parent node will be attached to. Set to null is same as set to Scene::rootNode(). In both cases,
        /// the node will to attach to world space.
        Node * parent;
    };

    /// \returns root node of the scene. Always valid.
    virtual Node & rootNode() const = 0;

    /// Add a new node to the scene graph. Returns pointer to the node.
    virtual Node * createNode(const NodeCreateParameters &) = 0;

    /// Remove a node and its subtree from the scene graph.
    ///
    /// If root node pointer is passed to this method, it'll effectively delete the whole scene graph. All components
    /// attach to root node will also be removed. But the root node itself remains valid.
    ///
    /// If the item is deleted, the parameter pointer will be reset to null.
    virtual void deleteNodeAndSubtree(Node *&) = 0;

    /// structure used to create new mesh instance.
    struct MeshCreateParameters {
        size_t vertexCount;         ///< number of vertices in the mesh. If zero, then the whole is considered empty. All other fields are ignored in this case.
        Mesh::VertexInput vertices; ///< vertex data stored in GPU buffer.

        // Index data. If either indexBuffer or indexCount is zero, then the mesh is considered non-indexed.
        VkBuffer indexBuffer;
        size_t   indexOffset; ///< Byte offset of first index.
        size_t   indexCount;
        size_t   indexStride; ///< must be either 2 or 4, indicating 16-bit or 32-bit index buffer. All other values are invalid.

        MeshCreateParameters(size_t vertCount = 0, VkBuffer idxBuff = (VkBuffer) VK_NULL_HANDLE, size_t idxOffset = 0, size_t idxCount = 0,
                             size_t idxStride = 2)
            : vertexCount(vertCount), indexBuffer(idxBuff), indexOffset(idxOffset), indexCount(idxCount), indexStride(idxStride) {}

        bool isIndexed() const { return indexBuffer != VK_NULL_HANDLE && indexCount > 0; }
    };

    /// Add new mesh object to the scene. Returns the node that represents the mesh.
    /// The returned node will be released along with the scene. No need to
    /// explicitly release it.
    virtual Mesh * createMesh(const MeshCreateParameters &) = 0;

    /// Delete mesh, if it is not referenced by anything. No op otherwise.
    /// If the item is deleted, the parameter pointer will be reset to null.
    virtual void deleteMesh(Mesh *&) = 0;

    /// return default material of the scene.
    virtual Material & defaultMaterial() const = 0;

    /// Create new material to scene.
    typedef Material::Desc MaterialCreateParameters;
    virtual Material *     createMaterial(const MaterialCreateParameters & materialCPs = MaterialCreateParameters()) = 0;

    /// Delete material, if it is not referenced by anything. No op otherwise.
    /// If the item is deleted, the parameter pointer will be reset to null.
    virtual void deleteMaterial(Material *&) = 0;

    /// Retrieve list of materials.
    virtual Blob<Material *> materials() const = 0;

    /// Model adding parameters.
    struct ModelCreateParameters {
        Mesh &              mesh;
        Material &          material; ///< default material of the
        Blob<Model::Subset> subsets;
    };

    /// Add a new model to scene.
    virtual Model * createModel(const ModelCreateParameters &) = 0;

    /// Delete model if it is not referenced by anything. NO op otherwise.
    /// If the item is deleted, the parameter pointer will be reset to null.
    virtual void deleteModel(Model *&) = 0;

    /// Parameters to create light
    struct LightCreateParameters {
        // Reserved for future use. Nothing for now.
    };
    virtual Light * createLight(const LightCreateParameters &) = 0;

    /// Delete light from scene.
    /// If the item is deleted, the parameter pointer will be reset to null.
    virtual void deleteLight(Light *&) = 0;

    /// Refresh internal GPU data structure based on the latest scene graph to be prepared for rendering.
    /// Note that this method doesn't actually modifying any GPU data. It just record all necessary commands
    /// to the incoming command buffer. It is caller's responsibility to submit the command to GPU device.
    ///
    /// This method will also clear all internal dirty flags. So if this method is called the 2nd time w/o
    /// any change to the scene, it'll be just a no-op. Nothing will be added to the command buffer.
    virtual void refreshGpuData(VkCommandBuffer cb) = 0;

    /// descriptor set and layout
    struct Descriptors {
        VkDescriptorSetLayout layout;
        VkDescriptorSet       set;
    };

    /// retrieve scene descriptors
    virtual Descriptors descriptors(VkCommandBuffer cb, bool includeBVH) = 0;

    /// Performance statistics
    struct PerfStats {
        Blob<NamedDuration> gpuTimestamps;
        size_t              instanceCount = 0; ///< number of active instances in TLAS.
        size_t              triangleCount = 0; /// number of triangles in the whole scene.
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

    /// overloaded creator
    Node *     create(const NodeCreateParameters & createParameters) { return createNode(createParameters); }
    Material * create(const MaterialCreateParameters & createParameters) { return createMaterial(createParameters); }
    Mesh *     create(const MeshCreateParameters & createParameters) { return createMesh(createParameters); }
    Model *    create(const ModelCreateParameters & createParameters) { return createModel(createParameters); }
    Light *    create(const LightCreateParameters & createParameters) { return createLight(createParameters); }

#if PH_RT_CXX_STANDARD >= 11
    /// templated creator with name
    template<typename T>
    auto create(StrA name, const T & creationParameters) {
        auto p = create(creationParameters);
        if (p && name) p->name = std::move(name);
        return p;
    }
#endif

    /// templated deleter
    void destroy(Node *& p) {
        if (p) deleteNodeAndSubtree(p);
    }
    void destroy(Material *& p) {
        if (p) deleteMaterial(p);
    }
    void destroy(Mesh *& p) {
        if (p) deleteMesh(p);
    }
    void destroy(Model *& p) {
        if (p) deleteModel(p);
    }
    void destroy(Light *& p) {
        if (p) deleteLight(p);
    }
};

/// --------------------------------------------------------------------------------------------------------------------
/// point light shadow map renderer
struct ShadowMapRenderPack : Root {
    struct RecordParameters {
        RecordParameters(): commandBuffer((VkCommandBuffer) VK_NULL_HANDLE) {}
        /// store all rendering commands. The buffer must be in recording state.
        VkCommandBuffer commandBuffer;

        /// The light source. light->shadowMap must point to a valid cubemap.
        Light * light;
    };
    virtual void record(const RecordParameters &) = 0;

    /// Performance statistics of the shadow render pack.
    struct PerfStats {
        Blob<NamedDuration> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

protected:
    ShadowMapRenderPack(const RootConstructParameters & p): Root(p) {}
};

/// --------------------------------------------------------------------------------------------------------------------
/// a main ray tracing renderer
struct RayTracingRenderPack : Root {
    /// shadow tracing mode.
    enum ShadowMode {
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

    enum Accumulation {
        OFF,
        ON,
        RETAIN,
    };

    struct TransparentShadowSettings {
        bool tshadowAlpha;      // enable transparent shadows
        bool tshadowColor;      // enable 3-channel colored transparent shadows
        bool tshadowAbsorption; // enable approximate volumetric absorption for transparent shadows
        bool tshadowFresnel;    // enable approximate fresnel attenuation for transparent shadows
        bool tshadowTextured;   // enable textured alpha, color, and normals when rendering transparent shadows
        TransparentShadowSettings(): tshadowAlpha(false), tshadowColor(false), tshadowAbsorption(false), tshadowFresnel(false), tshadowTextured(false) {}
    };

    // PhysRay has two classes of transparent objects: Alpha (ior = 0) and transmissive (ior > 0).
    // Alpha objects are rendered with simple alpha-blended multiplicative transparency. Objects with alpha values below alphaCutoff are not rendered.
    // Transmissive objects with alpha values below alphaCutoff are rendered with refractive transparency. Above alpha cutoff, the objects are treated as
    // opaques.
    struct TransparencySettings {
        float                     alphaCutoff;                      // skip rendering of ior = 0 objects with alpha below this value (0.0 = off)
        uint32_t                  alphaMaxHit;                      // maximum number of alpha blended hits before returning black
        uint32_t                  backscatterMode;                  // enable approximate volumetric backscattering for transmissive objects and their shadows
        bool                      calculateAbsorptionTransmittance; // enable approximate spectral transmission for transmissive objects and their shadows
        float                     fresnelCutoff; // fresnel cutoff value used for blended fresnel splitting. reflect above this value, refract below.
        TransparentShadowSettings shadowSettings;
        TransparencySettings()
            : alphaCutoff(0.f), alphaMaxHit(2), backscatterMode(0), calculateAbsorptionTransmittance(false), fresnelCutoff(0.55f),
              shadowSettings(TransparentShadowSettings()) {}
    };

    /// Fill render pack with rendering commands. Override any existing commands in the render pack.
    struct RecordParameters {
        /// pointer to the scene
        Scene * scene;

        /// Projection matrix.
        Float4x4 projMatrix;

        /// World -> View transformation
        Float3x4 viewMatrix;

        /// store all rendering commands. The buffer must be in recording state.
        VkCommandBuffer commandBuffer;

        /// The target image to render to.
        /// For ex, for Screen spaceShadowRenderPack, this will be the incoming rasterized
        /// screen space shadow map that will be modified using ray-tracing.
        VkImage targetImage;

        /// the image that stores the rendering result.
        VkImageView targetView;

        /// the depth buffer view that store depth result.
        VkImageView depthView;

        /// Ambient light term
        Float3 ambientLight;

        /// Use this to adjust saturation of the final render color.
        float saturation;

        /// Gamma correction. Note that his gamma correction is independent of LINEAR -> sRGB conversion.
        float gamma;

        /// Define the shadow rendering mode. If parameter is ignored by the rasterized render pack.
        ShadowMode shadowMode;

        /// Toggle on the heat map to illustrate the number of traversal steps in a path/shadow traced query
        bool enableHeatMap;

        /// Sets the upper end of the range threshold that determines a specific calibration for how our heat map appears
        float maxNumTraversalSteps;

        /// roughness cutoff for ray traced reflection. A surface with roughness smaller than this value will have
        /// ray traced reflection effect.
        float reflectionRoughnessCutoff;

        // Rotation of skybox about the Y axis in radians
        float skyboxRotation;

        // Path tracing specific config
        int          initialCandidateCount; // initial candidate count for ReSTIR (M)
        float        jitterAmount;          // sub-pixel jitter used to cheaply anti-alias the rasterized first-bounce of stochastic path tracers
        Accumulation accum;
        uint32_t     spp; ///< samples per pixel per frame.
        // subsurface scattering for stochastic path tracers
        float subsurfaceChance; // chance of sampling subsurface contribution in stochastic path tracers
        float rmaxScalar;       // max radius used to sample offset positions for stochastic path tracing of subsurface scattering
        float emissionScalar;   // scales intensity of subsurface values stored in color channels of the emission map
        float sssamtScalar;     // scales intensity of subsurface amount values stored in the alpha channel of the emission map
        float nChance;          // chance of casting subsurface ray in the normal (vs. the tangent, bitangent) direction
        float gaussV;           // gaussian variance used to sample a position within the max radius for subsurface scattering
        // cluster params for stochastic path tracers
        uint32_t clusterMode;
        Float3   sceneExtents;
        Float3   sceneCenter;
        int      sceneSubdivisions;

        /// Ray tracing settings
        float minRayLength;

        /// Max number of diffuse bounces
        uint32_t maxDiffuseBounces;

        /// Max number of specular bounces.
        uint32_t maxSpecularBounces;

        /// Transparency settings
        TransparencySettings transparencySettings;

        /// Diffuse irradiance map with mipmap chain.
        Material::TextureHandle irradianceMap;

        /// Prefiltered reflection map with mipmap chain encoded based on roughness.
        Material::TextureHandle reflectionMap;

        // 1 for enable lighting using skybox, 0 for skybox only work for alpha channel
        uint32_t skyboxLighting;

        bool sRGB;

        // toggle restir sampling mode
        uint32_t restirMode; // 0 = restir off
                             // 1 = restir on (initial sampling only, using initialCandidateCount samples)
                             // 2 = temporal restir (initial sampling + temporal resampling)
                             // 3 = spatiotemporal restir (initial sampling + temporal resampling + spatial resampling)
        uint32_t misMode;    // 0 = partitioned (DI evaluated using light sampling only, indirect hits on emissives are ignored)
                             // 1 = veach MIS (DI evaluated using light sampling, power-weighted and combined with indirect hits on emissives)

        bool enableRestirMap; // show p_hat value stored in restir history buffer. only valid for restir modes 2 and 3.

        RecordParameters()
            : scene(nullptr), projMatrix(Float4x4::identity()), viewMatrix(Float3x4::identity()), commandBuffer((VkCommandBuffer) VK_NULL_HANDLE),
              targetImage((VkImage) VK_NULL_HANDLE), targetView((VkImageView) VK_NULL_HANDLE), depthView((VkImageView) VK_NULL_HANDLE),
              ambientLight(Float3::make()), saturation(1.0f), gamma(1.0f), shadowMode(ShadowMode::RAY_TRACED), enableHeatMap(false),
              maxNumTraversalSteps(200.f), reflectionRoughnessCutoff(0.0f), skyboxRotation(0.f), initialCandidateCount(0), jitterAmount(0.f),
              accum(Accumulation::OFF), spp(1), subsurfaceChance(0.f), rmaxScalar(1.f), emissionScalar(1.f), sssamtScalar(1.f), nChance(0.5f), gaussV(1.f),
              clusterMode(0), sceneExtents(Float3::make()), sceneCenter(Float3::make()), sceneSubdivisions(0), minRayLength(0.001f), maxDiffuseBounces(3),
              maxSpecularBounces(5), transparencySettings(TransparencySettings()), irradianceMap(Material::TextureHandle::EMPTY_CUBE()),
              reflectionMap(Material::TextureHandle::EMPTY_CUBE()), skyboxLighting(1), sRGB(false), restirMode(0), misMode(0), enableRestirMap(false) {}
    };

    /// main entry point of the render pack class to record rendering commands to command buffer.
    virtual void record(const RecordParameters &) = 0;

    /// call this before calling record to ensure that the first frame draws without a hitch.
    /// recordparameters must have a valid scene and command buffer.
    void preloadPipelines(const RayTracingRenderPack::RecordParameters & rp) {
        rp.scene->refreshGpuData(rp.commandBuffer);
        auto desc = rp.scene->descriptors(rp.commandBuffer, true);
        reconstructPipelines(desc.layout);
    }

    /// Performance statistics
    struct PerfStats {
        Blob<NamedDuration> gpuTimestamps;
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

protected:
    virtual void reconstructPipelines(VkDescriptorSetLayout) = 0;
    RayTracingRenderPack(const RootConstructParameters & p): Root(p) {}
};

// ---------------------------------------------------------------------------------------------------------------------
/// A utility class to help collecting CPU times on frame basis.
class CpuTimeCollector {
public:
    virtual void begin(const char * name) = 0;

    /// \returns Returns duration in nanoseconds from the call to the paired begin(). Or 0, in case of error.
    virtual uint64_t end() = 0;

    struct ScopedTimer {
        CpuTimeCollector * t;
        ScopedTimer(CpuTimeCollector & t_, const char * name): t(&t_) { t_.begin(name); }
        ScopedTimer(CpuTimeCollector * t_, const char * name): t(t_) {
            if (t_) t_->begin(name);
        }
        ~ScopedTimer() {
            if (t) t->end();
        }
    };

protected:
    CpuTimeCollector() {}
    virtual ~CpuTimeCollector() {}
};

//----------------------------------------------------------------------------------------------------------------------
/// A proxy class to submit vulkan commands to GPU
struct CommandQueueProxy {
    struct SubmitInfo {
        BlobProxy<VkSemaphore>          waitSemaphores;
        BlobProxy<VkPipelineStageFlags> waitStages;
        BlobProxy<VkCommandBuffer>      commandBuffers;
        BlobProxy<VkSemaphore>          signalSemaphores;
    };

    struct PresentInfo {
        BlobProxy<VkSemaphore>    waitSemaphores;
        BlobProxy<VkSwapchainKHR> swapchains;
        BlobProxy<uint32_t>       imageIndices;
    };

    /// destructor
    virtual ~CommandQueueProxy() {}

    /// TODO: replace this with createCommandPool()
    virtual uint32_t queueFamilyIndex() const = 0;

    virtual VkResult submit(const BlobProxy<SubmitInfo> &, VkFence signalFence = (VkFence) VK_NULL_HANDLE) = 0;

    // wait for the queue to be completely idle (including both CPU and GPU)
    virtual VkResult waitIdle() = 0;
};

//----------------------------------------------------------------------------------------------------------------------
/// Represents the whole ray traced world. This is also the factory class for all other objects used by ray tracing.
/// This class is multithread safe.
/// TODO: consider move all creation/deletion function to their own classes.
class World {
public:
    virtual ~World() {}

    /// World factory
    //@{
    struct WorldCreateParameters {
        /// Type of the bounding volume hierarchy.
        enum BvhType {
            /// This is the default type that relies on VK_KHR_ray_query extension.
            KHR_RAY_QUERY = 0,

            /// This is an experimental compute shader based BVH implementation. It is still at very early stage.
            /// Use at your own risk.
            AABB_GPU,

            /// Number of BVH types.
            NUM_BVH_TYPES,
        };

        /// Vulkan global information
        //@{
        const VkAllocationCallbacks * allocator;
        VkInstance                    instance;
        VkPhysicalDevice              phydev;
        VkDevice                      device;
        void *                        vmaAllocator; // If not null, must be an valid VmaAllocator handle.
        CommandQueueProxy *           graphicsQueue;
        //@}

        /// When using BVH of type AABB_CPU, the RT module will try loading pre-built BVH data from asset folder
        /// named "bvh-cache". On desktop, you can use this parameter to specify where that bvh-cache folder really
        /// is on disk. For example, if your bvh cache files are in c:\\data\\bvh-cache folder, then add "c:\\data"
        /// folder to this parameter.
        /// On Android, this parameter is ignored. You'll need to specify the bvh-cache folder via Android asset
        /// manager.
        Blob<StrA> assetFolders;

        /// If this is not null, then the RT module will report internal CPU timing informations via this interface.
        /// This is useful if you are profiling your rendering performance.
        CpuTimeCollector * cpuTimeCollector;

        /// Set to true to enable collection RT modules' internal GPU timestamps. You can query those timestamps
        /// via the perfStats() call of Scene and RenderPack classes.
        bool enableGpuTimestamps;

        BvhType bvhType;

        /// constructor
        WorldCreateParameters(const VkAllocationCallbacks * const alloc = nullptr, const VkInstance inst = nullptr, const VkPhysicalDevice phyDev = nullptr,
                              const VkDevice dev = nullptr, void * vmaAlloc = nullptr, ph::rt::CommandQueueProxy * gfxQueue = nullptr,
                              Blob<StrA> assetFolder = Blob<StrA>(), CpuTimeCollector * cpuTimeCollec = nullptr, bool enableGPUTimeStamp = false,
                              ph::rt::World::WorldCreateParameters::BvhType bvhT = ph::rt::World::WorldCreateParameters::BvhType::KHR_RAY_QUERY)
            : allocator(alloc), instance(inst), phydev(phyDev), device(dev), vmaAllocator(vmaAlloc), graphicsQueue(gfxQueue), assetFolders(assetFolder),
              cpuTimeCollector(cpuTimeCollec), enableGpuTimestamps(enableGPUTimeStamp), bvhType(bvhT) {}
    };

    PH_RT_API static World * createWorld(const WorldCreateParameters &);

    //@}

    /// Get the create parameters.
    virtual const WorldCreateParameters & cp() const = 0;

    /// Update frame counter to allow RT module to recycle GPU resources used by previous frames.
    /// The frame numbers passed in must obey the following rules:
    ///  - frame number must never be decreasing
    ///  - newFrame must be greater than safeFrame.
    virtual void updateFrameCounter(int64_t currentFrame, int64_t safeFrame) = 0;

    /// scene factory
    //@{

    /// scene creation parameters
    struct SceneCreateParameters {
        // Reserved for future use.
    };
    virtual auto createScene(const SceneCreateParameters &) -> Scene * = 0;

    /// Delete a scene. This will also delete all sub objects (such as node, mesh) owned by the scene and invalidate
    /// their pointers.
    virtual void deleteScene(Scene *&) = 0;

    //@}

    /// screen space shadow map render pack
    //@{
    struct ShadowMapRenderPackCreateParameters {
        ShadowMapRenderPackCreateParameters()
            : shadowMapSize(0), shadowMapFormat(VK_FORMAT_UNDEFINED), shadowMapLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {}
        uint32_t      shadowMapSize;
        VkFormat      shadowMapFormat;
        VkImageLayout shadowMapLayout;

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

            /// Render the scene with the performance path tracer
            FAST_PT,
        };

        static bool isStochastic(const Mode & m) { return (m == Mode::PATH_TRACING) || (m == Mode::FAST_PT); }
        static bool isNoiseFree(const Mode & m) { return (m == Mode::NOISE_FREE) || (m == Mode::SHADOW_TRACING); }

        Mode mode;

        VkFormat targetFormat;

        uint32_t targetWidth, targetHeight;

        // By default, the render pack determines if the target is in linear or sRGB color space based on
        // target format. This flag is to deal with the case that swapchain image can be set to be in sRGB
        // color space, regardless of the image format.
        bool targetIsSRGB;

        /// Define the input layout of the target image. After the rendering, the target image
        /// will always be transferred to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
        /// TODO: move to RecordParameters
        VkImageLayout targetLayout;

        /// Specify the rendering viewport.
        VkViewport viewport;

        float clearColor[4]; ///< clear color. ignored if clear is false.
        bool  clear;         ///< if set to true to clear the target view.

        // By default, shaders may be configured with a number of global settings specified via uniforms.
        // When set to true, these uniforms are overridden with hardcoded precompiled defines specified in
        // defines.glsl, which provides a significant speedup. When building your release application,
        // hard-code your desired define values in define.glsl and set this to true.
        bool usePrecompiledShaderParameters = false;

        // If the scene is known to have no refractive or rough reflective materials,
        // you may clear this flag to speed up rendering in NOISE_FREE mode.
        bool refractionAndRoughReflection = true;

        RayTracingRenderPackCreateParameters(Mode m = NOISE_FREE, VkFormat targetformat_ = VK_FORMAT_UNDEFINED, uint32_t width = 0, uint32_t height = 0,
                                             bool isSRGB = false, VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            : mode(m), targetFormat(targetformat_), targetWidth(width), targetHeight(height), targetIsSRGB(isSRGB), targetLayout(layout), clear(true),
              usePrecompiledShaderParameters(false), refractionAndRoughReflection(true) {
            viewport.x        = 0;
            viewport.y        = 0;
            viewport.width    = 0;
            viewport.height   = 0;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            clearColor[0]     = 0.f;
            clearColor[1]     = 0.f;
            clearColor[2]     = 0.f;
            clearColor[3]     = 1.f;
        }

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
    };
    virtual RayTracingRenderPack * createRayTracingRenderPack(const RayTracingRenderPackCreateParameters &) = 0;
    virtual void                   deleteRayTracingRenderPack(RayTracingRenderPack *&)                      = 0;
    //@}

    /// overloaded creators
    //@{
    Scene *                create(const SceneCreateParameters & createParameters) { return createScene(createParameters); }
    ShadowMapRenderPack *  create(const ShadowMapRenderPackCreateParameters & createParameters) { return createShadowMapRenderPack(createParameters); }
    RayTracingRenderPack * create(const RayTracingRenderPackCreateParameters & createParameters) { return createRayTracingRenderPack(createParameters); }
    //@}

#if PH_RT_CXX_STANDARD >= 11
    /// creator new item with name.
    template<typename T>
    auto create(StrA name, const T & creationParameters) {
        auto p = create(creationParameters);
        if (p) p->name = std::move(name);
        return p;
    }
#endif

    /// overloaded deleter
    static void destroy(Scene *& p) { p->world()->deleteScene(p); }
    static void destroy(ShadowMapRenderPack *& p) { p->world()->deleteShadowMapRenderPack(p); }
    static void destroy(RayTracingRenderPack *& p) { p->world()->deleteRayTracingRenderPack(p); }

protected:
    World() {};
};

} // namespace rt
} // namespace ph