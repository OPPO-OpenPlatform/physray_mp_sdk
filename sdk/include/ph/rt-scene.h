/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This is the interface of the ray tracing scene model of the PhysRay SDK.

#pragma once

#include <ph/va.h>
#include <string.h> // for memcpy
#include <cassert>

#ifdef _MSC_VER
    #ifdef PH_RT_DLL_INTERNAL
        #define PH_RT_API // __declspec(dllexport)
    #else
        #define PH_RT_API // __declspec(dllimport)
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

#define PH_RT_NO_COPY_NO_MOVE(X)       \
    X(const X &) = delete;             \
    X & operator=(const X &) = delete; \
    X(X &&)                  = delete; \
    X & operator=(X &&) = delete
#include <initializer_list>

/// Main namespace for Ray Tracing module
namespace ph {
namespace rt {

class World;

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
    std::string name;

    struct RootConstructParameters {
        World * w;
        Id      id;
    };

    /// Returns a copy of the user data.
    virtual std::vector<uint8_t> userData(const Guid & guid) const = 0;

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
    /// Allows textures to be loaded into vulkan.
    ///
    /// Supports std::hash.
    struct TextureHandle {
        VkImage         image;
        VkImageView     view;
        VkImageViewType viewType;
        VkFormat        format;
        VkExtent3D      extent; // if not zero, indicates size of the base level.

        constexpr TextureHandle(VkImage i = 0, VkImageView v = 0, VkImageViewType t = VK_IMAGE_VIEW_TYPE_2D, VkFormat f = VK_FORMAT_UNDEFINED, uint32_t w = 0,
                                uint32_t h = 0, uint32_t d = 0)
            : image(i), view(v), viewType(t), format(f), extent {w, h, d} {}

        constexpr TextureHandle(const ph::va::ImageObject & i): image(i.image), view(i.view), viewType(i.viewType), format(i.ci.format), extent(i.ci.extent) {}

        static constexpr TextureHandle EMPTY_2D() { return TextureHandle(0, 0, VK_IMAGE_VIEW_TYPE_2D); }

        static constexpr TextureHandle EMPTY_CUBE() { return TextureHandle(0, 0, VK_IMAGE_VIEW_TYPE_CUBE); }

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

    enum TextureType {
        ALBEDO = 0, // Diffuse albedo map.
        NORMAL,     // Normal map.
        ORM,        // Combined occlusion(R)-roughness(G)-metalness(B) map.
        EMISSION,   // Emissive texture map. Also used for subsurface scattering, in which case (A) is the sss amount.
        COUNT,
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
            for (size_t i = 0; i < std::size(maps); ++i) {
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
            for (size_t i = 0; i < std::size(maps); ++i) {
                if (maps[i] != rhs.maps[i]) return maps[i] < rhs.maps[i];
            }
            return false;
        }
    };

    typedef Desc CreateParameters;

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

    /// structure used to create new mesh instance.
    struct CreateParameters {
        size_t      vertexCount; ///< number of vertices in the mesh. If zero, then the whole is considered empty. All other fields are ignored in this case.
        VertexInput vertices;    ///< vertex data stored in GPU buffer.

        // Index data. If either indexBuffer or indexCount is zero, then the mesh is considered non-indexed.
        VkBuffer indexBuffer;
        size_t   indexOffset; ///< Byte offset of first index.
        size_t   indexCount;
        size_t   indexStride; ///< must be either 2 or 4, indicating 16-bit or 32-bit index buffer. All other values are invalid.

        CreateParameters(size_t vertCount = 0, VkBuffer idxBuff = (VkBuffer) VK_NULL_HANDLE, size_t idxOffset = 0, size_t idxCount = 0, size_t idxStride = 2)
            : vertexCount(vertCount), indexBuffer(idxBuff), indexOffset(idxOffset), indexCount(idxCount), indexStride(idxStride) {}

        bool isIndexed() const { return indexBuffer != VK_NULL_HANDLE && indexCount > 0; }
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

/// --------------------------------------------------------------------------------------------------------------------
///
/// Renders one mesh in the location of the owning node.
///
/// A model not attached to any node is considered invisible.
class Model : public Root {
public:
    /// Represents a subset of a mesh with certain material
    struct Subset {
        Subset(Material * mat = nullptr, size_t idxBase = 0, size_t idxCount = size_t(~0)): material(mat), indexBase(idxBase), indexCount(idxCount) {}
        Material * material   = nullptr;    ///< If null, then inherited the model's material.
        size_t     indexBase  = 0;          ///< Index of the first index of the subset.
        size_t     indexCount = size_t(~0); ///< Number of indices in the subset. Set to size_t(~0) means to the end of the mesh.
        uint32_t   flags      = 0;          ///< Flags of the subset. If zero, then inherited from the model.
    };

    /// Model adding parameters.
    struct CreateParameters {
        Mesh &              mesh;
        Material &          material; ///< default material of the
        std::vector<Subset> subsets;
    };

    /// @return The mesh that this model contains.
    virtual Mesh & mesh() const = 0;

    /// @return List of subsets of the model
    virtual ArrayView<Subset> subsets() const = 0;

    /// Custom flag of the model.
    uint32_t flags = 0;

    /// Use this flag with rt::render::ReflectiveRenderPack to mark the model as reflective.
    static constexpr uint32_t REFLECTIVE = 1LU;

    /// Use this flag with rt::render::ReflectiveRenderPack to mark the model as transparent.
    static constexpr uint32_t TRANSPARENT_FLAG = 2LU;

protected:
    using Root::Root;
};

// ---------------------------------------------------------------------------------------------------------------------
//
/// Represents a light in the scene.
///
/// Light component can only be attached to at most one node a time.
///
/// A light not attached to any node is considered off.
class Light : public Root {
public:
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
        /// TODO: define how far the light can reach, in local space.
    };

    /// Directional light specific fields.
    struct Directional {
        /// Direction of the light source in local space.
        /// This will be transformed to the node's world space
        /// when the light is processed.
        Eigen::Vector3f direction;

        // bounding box of the scene that this light needs to cover.
        Eigen::Vector3f bboxMin;
        Eigen::Vector3f bboxMax;

        Directional() {
            // Ubuntu 22.04 build fails with
            // -Werror=uninitialized when
            // assigning Eigen classes to uninitialized
            // members.
            direction = bboxMin = bboxMax = Eigen::Vector3f::Ones();
        }

        Directional(const Directional & other) {
            direction = other.direction;
            bboxMin   = other.bboxMin;
            bboxMax   = other.bboxMax;
        }

        Directional & operator=(const Directional & other) {
            direction = other.direction;
            bboxMin   = other.bboxMin;
            bboxMax   = other.bboxMax;
            return *this;
        }

        Directional & setDir(const Eigen::Vector3f & dir) {
            direction = dir;
            return *this;
        }

        Directional & setDir(float x, float y, float z) {
            direction << x, y, z;
            return *this;
        }

        Directional & setBBox(const Eigen::Vector3f & min_, const Eigen::Vector3f & max_) {
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
        Eigen::Vector3f direction;

        /// Angular falloff inner angle, in radian.
        float inner;

        /// Angular falloff outer angle, in radian.
        float outer;

        Spot() = default;

        Spot(const Spot & other) {
            direction = other.direction;
            inner     = other.inner;
            outer     = other.outer;
        }

        Spot & operator=(const Spot & other) {
            direction = other.direction;
            inner     = other.inner;
            outer     = other.outer;
            return *this;
        }

        Spot & setDir(float x, float y, float z) {
            direction << x, y, z;
            return *this;
        }

        Spot & setDir(const Eigen::Vector3f & dir) {
            direction = dir;
            return *this;
        }

        Spot & setFalloff(float inner_, float outer_) {
            inner = inner_;
            outer = outer_;
            return *this;
        }
    };

    struct Geom {
        /// id of the model entity that this light is attached to.
        int64_t modelEntity = 0;
    };

    struct Desc {
        /// Type of the light.
        Type type = Type::POINT;

        /// The dimensions of area lights (sphere, rectangle, disk)
        // For point lights, dimension.x is the radius of the sphere.
        // For directional lights, dimension.xy are the width and height of the quad.
        // For spot lights, dimension.xy are the width and height of the ellipse.
        // For mesh lights, dimension.xyz give the dimensions of the mesh's untransformed bbox
        Eigen::Vector3f dimension = {0.f, 0.f, 0.f};

        /// color/brightness of the light
        Eigen::Vector3f emission = {1.f, 1.f, 1.f};

        // Range now applies to all light types, since non-physical area lights
        // will be point-light-attenuated to provide greater artist control
        float range = 1.0f;

        // allow shadows to be cast from this light when true
        bool allowShadow = true;

        union {
            Point       point;
            Directional directional;
            Spot        spot;
            Geom        geom;
        };

        Desc() {}

        Desc(const Desc & other): type(other.type), dimension(other.dimension), emission(other.emission), range(other.range), allowShadow(other.allowShadow) {
            switch (type) {
            case Type::OFF:
                // Nothing to copy
                break;
            case Type::POINT:
                new (&point) Point(other.point);
                break;
            case Type::DIRECTIONAL:
                new (&directional) Directional(other.directional);
                break;
            case Type::SPOT:
                new (&spot) Spot(other.spot);
                break;
            case Type::GEOM:
                new (&geom) Geom(other.geom);
                break;
            }
        };

        Desc & operator=(const Desc & other) {
            type        = other.type;
            dimension   = other.dimension;
            emission    = other.emission;
            range       = other.range;
            allowShadow = other.allowShadow;

            switch (type) {
            case Type::OFF:
                // Nothing to copy
                break;
            case Type::POINT:
                point = other.point;
                break;
            case Type::DIRECTIONAL:
                directional = other.directional;
                break;
            case Type::SPOT:
                spot = other.spot;
                break;
            case Type::GEOM:
                geom = other.geom;
                break;
            }
            return *this;
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

        Desc & setEmission(const Eigen::Vector3f & v) {
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
    };

    /// Light create parameters
    struct CreateParameters {
        // Reserved for future use. Nothing for now.
    };

    /// Shadow map texture handle. For point light, this should be an
    /// cubemap. For spot and directional light, just regular 2D map.
    Material::TextureHandle shadowMap;
    float                   shadowMapBias      = 0.001f;
    float                   shadowMapSlopeBias = 0.001f;

    /// @return Most of the light's variables.
    const Desc & desc() const { return _desc; }

    /// Allow us to replace the light's variables.
    virtual void reset(const Desc & desc) = 0;

    /// calculate matrix that translate from light space to projection space.
    virtual Eigen::Matrix4f calculateProjView(const Eigen::Matrix<float, 3, 4> & worldTransform) const = 0;

protected:
    using Root::Root;

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

    /// construct from initializer list
    template<typename T>
    IndexBuffer(std::initializer_list<T> r): data(r.begin()), count(r.size()), stride(sizeof(T)) {
        static_assert(sizeof(T) == 2 || sizeof(T) == 4, "");
    }

    /// construct from typed range
    template<typename T>
    IndexBuffer(const ArrayView<T> & r): data(r.data()), count(r.size()), stride(sizeof(T)) {
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
                PH_LOGE("index[%zu] = %u is out of the valid range [%zu, %zu]", i, index, min, max);
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

struct DeviceData {
    struct SubsetData {
        uint32_t materialIndex;
        uint32_t indexBase;
        uint32_t indexCount;
        uint32_t flags;
    };

    struct ModelInstanceData {
        const Mesh *               mesh;
        Eigen::Matrix<float, 3, 4> worldTransform;
        uint32_t                   indexOffset;
        uint32_t                   indexCount;
        uint32_t                   vertexBase;
        std::vector<SubsetData>    subsets;
        uint32_t                   flags;
    };

    VkBuffer                       vertexBuffer;
    VkBuffer                       indexBuffer;
    std::vector<ModelInstanceData> instances;
};

// ---------------------------------------------------------------------------------------------------------------------
/// Represents ray traced scene.
class Scene : public Root {
public:
    struct CreateParameters {
        // Reserved for future use. Nothing for now.
    };

    struct EntityDesc {
        Model *                    model        = nullptr;
        Light *                    light        = nullptr;
        uint32_t                   instanceMask = 0;
        Eigen::Matrix<float, 3, 4> transform    = Eigen::Matrix<float, 3, 4>::Identity();
        bool                       valid() const { return model || light; }
    };

    /// add a model to the scene. Returns the entity id of the model.
    virtual int64_t addModel(Model & m, uint32_t instanceMask = 0xFF) = 0;

    /// Add a light to the scene. Returns the entity id of the light.
    virtual int64_t addLight(Light & l) = 0;

    /// Remove an entity, either model or light. from the scene.
    virtual void deleteEntity(int64_t) = 0;

    /// Set visibility of an entity.
    ///
    /// Making an entity invisible acts like deleting it from the scene. But it doesn't actually remove it from the scene.
    /// So it can be made visible again with less bookkeeping overhead.
    ///
    /// Changing entity visibility repeatedly is also slightly less expensive than deleting and adding it back to the scene repeatedly.
    virtual void setVisible(int64_t entity, bool visible) = 0;

    /// Set world transform of an entity.
    virtual void setTransform(int64_t entity, const Eigen::Matrix<float, 3, 4> & worldTransform) = 0;

    /// Retrieve entity description.
    virtual EntityDesc getEntityDesc(int64_t entity) const = 0;

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

    virtual DeviceData deviceData() = 0;

    virtual size_t getLightCount() const = 0;

    /// Performance statistics
    struct PerfStats {
        std::vector<NamedDuration> gpuTimestamps;
        size_t                     instanceCount = 0; ///< number of active instances in TLAS.
        size_t                     triangleCount = 0; ///< number of triangles in the whole scene.
    };

    /// Get rendering performance statistics. This function returns valid data only when
    /// WorldCreateParameters::enableGpuTimestamps is set to true
    virtual PerfStats perfStats() = 0;

protected:
    using Root::Root;
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

        /// This is what the world and scene class is using to submit GPU work. Must be a valid pointer.
        va::VulkanSubmissionProxy * vsp = nullptr;

        /// When using BVH of type AABB_CPU, the RT module will try loading pre-built BVH data from asset folder
        /// named "bvh-cache". On desktop, you can use this parameter to specify where that bvh-cache folder really
        /// is on disk. For example, if your bvh cache files are in c:\\data\\bvh-cache folder, then add "c:\\data"
        /// folder to this parameter.
        /// On Android, this parameter is ignored. You'll need to specify the bvh-cache folder via Android asset
        /// manager.
        std::vector<std::string> assetFolders;

        /// If this is not null, then the RT module will report internal CPU timing informations via this interface.
        /// This is useful if you are profiling your rendering performance.
        CpuTimeCollector * cpuTimeCollector = nullptr;

        /// Set to true to enable collection RT modules' internal GPU timestamps. You can query those timestamps
        /// via the perfStats() call of Scene class.
        bool enableGpuTimestamps = false;

        BvhType bvhType = KHR_RAY_QUERY;
    };

    PH_RT_API static World * createWorld(const WorldCreateParameters &);

    //@}

    /// Get the create parameters.
    virtual const WorldCreateParameters & cp() const = 0;

    /// Get the DOP that the RT world used to defer host operation to be in sync with GPU.
    virtual va::DeferredHostOperation & dop() const = 0;

    /// Update frame counter to allow DOP to run deferred tasks and recycle GPU resources used by previous frames.
    /// The frame numbers passed in must obey the following rules:
    ///  - frame number must never be decreasing
    ///  - newFrame must be greater than safeFrame.
    virtual void updateFrameCounter(int64_t currentFrame, int64_t safeFrame) = 0;

    /// Retrieve an dummy combined sampler of certain type
    virtual auto dummyTexture(VkImageViewType viewType) -> std::pair<VkSampler, VkImageView> = 0;

    /// Release all mesh and materials that are not referenced by any scene.
    virtual void prune() = 0;

    /// Reset the world back to default state. This is an dangerous method that will delete everything in this world
    /// and invalidate all existing pointers. Use it with caution to not leave dangling pointers behind.
    virtual void reset() = 0;

    /// Scene component factory and accessors
    /// There's no explicit delete function for scene components. They will be released automatically when the world is deleted. Or when purge() is called, if
    /// it is not referenced by any scene.
    //@{
    virtual auto createMesh(const Mesh::CreateParameters &) -> Mesh * = 0;
    virtual void tryDeleteMesh(Mesh *&)                               = 0; ///< delete mesh if it is not referenced by any scene
    virtual auto meshes() const -> std::vector<Mesh *>                = 0;

    virtual auto createMaterial(const Material::CreateParameters & materialCPs = Material::CreateParameters()) -> Material * = 0;
    virtual void tryDeleteMaterial(Material *&)               = 0; ///< delete material if it is not referenced by any scene
    virtual auto materials() const -> std::vector<Material *> = 0;
    virtual auto defaultMaterial() const -> Material &        = 0;

    virtual auto createModel(const Model::CreateParameters &) -> Model * = 0;
    virtual void tryDeleteModel(Model *&)                                = 0; ///< delete model if it is not referenced by any scene
    virtual auto models() const -> std::vector<Model *>                  = 0;

    virtual auto createLight(const Light::CreateParameters &) -> Light * = 0;
    virtual void tryDeleteLight(Light *&)                                = 0; ///< delete light if it is not referenced by any scene
    virtual auto lights() const -> std::vector<Light *>                  = 0;
    //@}

    /// scene factory
    //@{
    virtual auto createScene(const Scene::CreateParameters &) -> Scene * = 0;
    virtual void deleteScene(Scene *&)                                   = 0;
    virtual auto scenes() const -> std::vector<Scene *>                  = 0;
    //@}

    /// overloaded creators
    //@{
    Material * create(const Material::CreateParameters & createParameters) { return createMaterial(createParameters); }
    Mesh *     create(const Mesh::CreateParameters & createParameters) { return createMesh(createParameters); }
    Model *    create(const Model::CreateParameters & createParameters) { return createModel(createParameters); }
    Light *    create(const Light::CreateParameters & createParameters) { return createLight(createParameters); }
    Scene *    create(const Scene::CreateParameters & createParameters) { return createScene(createParameters); }
    //@}

    /// creator new item with name.
    template<typename T>
    auto create(std::string name, const T & creationParameters) {
        auto p = create(creationParameters);
        if (p) p->name = std::move(name);
        return p;
    }

    /// Misc. utility methods
    //@{

    const va::VulkanGlobalInfo & vgi() const { return cp().vsp->vgi(); }

    va::VulkanSubmissionProxy & vsp() const { return *cp().vsp; }

    //@}

protected:
    World() {};
};

} // namespace rt
} // namespace ph

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
        static_assert(sizeof(size_t) >= sizeof(float), "");

        // Make sure albedo is at the very beginning of the descriptor since our float iteration won't work otherwise.
        static_assert(0 == static_cast<size_t>(offsetof(ph::rt::Material::Desc, albedo)), "");

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
            memcpy(&value, p1 + i, sizeof(float));

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
