/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : rps.h
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

// This is the main interface of the Render Pack System
#pragma once
#include <ph/va.h>
#include <atomic>
#include <functional>
#include <map>
#include <unordered_map>
#include <optional>

#if true
    #define PH_RPS_PURE_METHOD(...)                                                                  \
        {                                                                                            \
            __VA_ARGS__;                                                                             \
            PH_THROW("method is not implemented yet. Please contact RPS develop team for support."); \
        }
#else
    #define PH_RPS_PURE_METHOD(...) = 0
#endif

namespace ph::rps {

class CommandRecorder;

class Factory;

/// The root class of the RPS module.
class Root {
public:
    PH_NO_COPY_NO_MOVE(Root);

    union Identity {
        uint64_t u64;
        int64_t  i64;
        bool     operator==(const Identity & rhs) const { return u64 == rhs.u64; }
        bool     operator!=(const Identity & rhs) const { return u64 != rhs.u64; }
        bool     operator<(const Identity & rhs) const { return u64 < rhs.u64; }
    };

    inline static constexpr Identity INVALID_ID = {0};

    /// The signal triggered when the item is about to be destructed.
    mutable sigslot::signal<Root *> onDestructing;

    /// Triggered when the item is fully desctructed. Accessing
    /// any data member of the destructed item is strictly
    /// prohibited and will cause undefined behavior.
    mutable sigslot::signal<> onDestructed;

    Factory & fac() const { return *_fac; }

    Identity id() const { return _id; }

    /// Return current name of the object.
    const char * name() const { return _name; }

    /// Give the object a new name. The method will take it only if it meets all
    /// requirements defined by the object, such as the uniqueness of the name.
    virtual void setName(const char *) PH_RPS_PURE_METHOD();

    // virtual std::vector<uint8_t> serialize() const PH_RPS_PURE_METHOD();

protected:
    Root(Factory * f, uint64_t id): _fac(f), _id({id}) {
        // factory pointer could be null. So don't assert here.
        PH_ASSERT(id); // ID should never be 0.
    }
    virtual ~Root() {
        PH_ASSERT(0 == _ref);
        try {
            onDestructed();
        } catch (...) {}
    }

    const char * _name = nullptr;

private:
    friend class RefBase;
    friend class Factory;
    friend class RootImplBase;
    std::atomic<uint64_t> _ref = 0;
    Factory *             _fac = nullptr;
    Identity              _id;
};

/// base class of Ref
class RefBase {
protected:
    RefBase() {}

    ~RefBase() {}

    static void addRef(Root * p) {
        PH_ASSERT(p);
        ++p->_ref;
    }

    static void release(Root * p);
};

/// Reference of a RPS object.
template<typename T>
class Ref : public RefBase {
public:
    constexpr Ref() = default;

    Ref(T & t) {
        _ptr = &t;
        addRef(_ptr);
    }

    Ref(T * t) {
        if (!t) return;
        _ptr = t;
        addRef(_ptr);
    }

    ~Ref() { clear(); }

    /// copy constructor
    Ref(const Ref & rhs) {
        if (rhs._ptr) addRef(rhs._ptr);
        _ptr = rhs._ptr;
    }

    /// move constructor
    Ref(Ref && rhs) {
        _ptr     = rhs._ptr;
        rhs._ptr = nullptr;
    }

    void clear() {
        if (_ptr) release(_ptr), _ptr = nullptr;
    }

    bool empty() const { return !_ptr; }

    bool valid() const { return _ptr != nullptr; }

    void reset(T * t) {
        if (_ptr == t) return;
        clear();
        _ptr = t;
        if (t) addRef(_ptr);
    }

    template<typename T2 = T>
    T2 * get() const {
        return (T2 *) _ptr;
    }

    /// get address of the underlying pointer
    T * const * addr() const {
        PH_ASSERT(_ptr);
        return &_ptr;
    }

    /// copy operator
    Ref & operator=(const Ref & rhs) {
        if (_ptr == rhs._ptr) return *this;
        if (_ptr) release(_ptr);
        if (rhs._ptr) addRef(rhs._ptr);
        _ptr = rhs._ptr;
        return *this;
    }

    /// move operator
    Ref & operator=(Ref && rhs) {
        if (this != &rhs) {
            if (_ptr) release(_ptr);
            _ptr     = rhs._ptr;
            rhs._ptr = nullptr;
        }
        return *this;
    }

    /// comparison operator
    bool operator==(const Ref & rhs) const { return _ptr == rhs._ptr; }

    /// comparison operator
    bool operator!=(const Ref & rhs) const { return _ptr != rhs._ptr; }

    /// comparison operator
    bool operator<(const Ref & rhs) const { return _ptr < rhs._ptr; }

    bool operator!() const { return 0 == _ptr; }

    /// dereference operator
    T * operator->() const {
        PH_ASSERT(_ptr);
        return _ptr;
    }

    /// dereference operator
    T & operator*() const {
        PH_ASSERT(_ptr);
        return *_ptr;
    }

private:
    T * _ptr = nullptr;
};

class Buffer : public Root {
public:
    struct View {
        Ref<Buffer> buffer;
        size_t      offset = 0;
        size_t      size   = size_t(-1);
        bool        empty() const { return !buffer; }
    };

    struct CreateParameters {
        size_t                size;
        VkBufferUsageFlags    usages;
        VkMemoryPropertyFlags memory = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VkMemoryAllocateFlags alloc  = 0;
    };

    struct Access {
        VkPipelineStageFlags stages      = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkAccessFlags        access      = VK_ACCESS_TRANSFER_WRITE_BIT;
        size_t               offset      = 0;
        size_t               size        = size_t(-1);
        uint32_t             queueFamily = VK_QUEUE_FAMILY_IGNORED;
    };

    struct ImportParameters {
        VkBuffer             handle {};
        VkDeviceSize         size {};
        VkPipelineStageFlags stages      = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkAccessFlags        access      = VK_ACCESS_TRANSFER_WRITE_BIT;
        uint32_t             queueFamily = VK_QUEUE_FAMILY_IGNORED;
    };

    /// commonly used access states
    static constexpr Access TS() { return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT}; }
    static constexpr Access TD() { return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT}; }
    static constexpr Access VB() { return {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT}; }
    static constexpr Access IB() { return {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT}; }
    static constexpr Access UB() { return {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_UNIFORM_READ_BIT}; }
    static constexpr Access SB() { return {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_SHADER_READ_BIT}; }

    /// Transition buffer region into specific state
    virtual void cmdSetAccess(CommandRecorder &, const Access &) PH_RPS_PURE_METHOD();

    /// @brief Record buffer content read command into the command recorder.
    /// The actual reading command is executed later along with the submission of the command buffer.
    virtual std::shared_ptr<va::BufferObject> cmdRead(CommandRecorder & rec, size_t offset = 0, size_t size = size_t(-1))
        PH_RPS_PURE_METHOD((void) rec; (void) offset; (void) size;);

    /// @brief Record GPU commands to update buffer content.
    /// The data pointer must maintain valid the entire time until the record commands are submitted to GPU and
    /// finished execution. Deleting or modifying the memory pointed by the data pointer in any way before that
    /// will lead to undefined behavior including, but not limit to, garbage data or CPU/GPU crash.
    virtual void cmdWrite(CommandRecorder & rec, const void * srcData, size_t dstOffset = 0, size_t srcSize = size_t(-1))
        PH_RPS_PURE_METHOD((void) rec; (void) srcData; (void) dstOffset; (void) srcSize;);

    /// Record GPU command to transfer data between buffers.
    virtual void cmdCopyTo(CommandRecorder & rec, Buffer & dst, size_t srcOffset = 0, size_t dstOffset = 0, size_t size = size_t(-1))
        PH_RPS_PURE_METHOD((void) rec; (void) srcOffset; (void) dst; (void) dstOffset; (void) size;);

    template<typename T>
    void cmdWrite(CommandRecorder & rec, const ConstRange<T> & v) {
        cmdWrite(rec, v.data(), 0, v.size() * sizeof(T));
    }

    template<typename T>
    void cmdRead(CommandRecorder & rec, MutableRange<T> v) {
        return cmdRead(rec, v.data(), 0, v.size() * sizeof(T));
    }

protected:
    using Root::Root;
};

class Image : public Root {
public:
    struct View {
        Ref<Image> image;

        /// set 0 to range.aspect to automatically determine the aspect
        VkImageSubresourceRange range = {0, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};

        ///< set to VK_FORMAT_UNDEFINED means same as the image.
        VkFormat format = VK_FORMAT_UNDEFINED;

        // VkComponentMapping components {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    };

    struct CreateParameters1 {
        VkImageCreateInfo     ci     = {};
        VkMemoryPropertyFlags memory = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    };

    struct CreateParameters2 {
        ImageProxy            proxy;
        VkImageUsageFlags     usage  = VK_IMAGE_USAGE_SAMPLED_BIT;
        VkMemoryPropertyFlags memory = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    };

    struct Access {
        VkPipelineStageFlags    stages      = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkAccessFlags           access      = VK_ACCESS_TRANSFER_WRITE_BIT;
        VkImageLayout           layout      = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t                queueFamily = VK_QUEUE_FAMILY_IGNORED;
        VkImageSubresourceRange range       = {0, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        VkFormat                format      = VK_FORMAT_UNDEFINED;
    };

    /// @brief set access to transfer source
    static constexpr Access TS() { return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL}; }

    /// @brief set access to transfer destination
    static constexpr Access TD() { return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}; }

    /// @brief set access for shader resource view.
    inline static constexpr Access SR() {
        return Access {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    };

    /// @brief set access for render target view.
    inline static constexpr Access RT() {
        return Access {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    };

    /// @brief set access for depth stencil (read write)
    inline static constexpr Access DS(bool readonly = false) {
        return readonly
                   ? Access {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}
                   : Access {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL};
    };

    // TODO: Rename to Desc
    struct ImportParameters {
        VkImage               image       = 0; ///< Handle to the image that we are attaching to.
        VkImageType           type        = VK_IMAGE_TYPE_2D;
        VkFormat              format      = VK_FORMAT_R8G8B8A8_UNORM;
        VkExtent3D            extent      = {1, 1, 1};
        uint32_t              mipLevels   = 1;
        uint32_t              arrayLayers = 1;
        VkSampleCountFlagBits samples     = VK_SAMPLE_COUNT_1_BIT;
        Access                initialAccess;
    };

    virtual const ImportParameters & desc() const PH_RPS_PURE_METHOD();

    struct PixelArray {
        /// Pointer to pixels.
        const void * data = 0;

        /// Distance in bytes from one row of pixel block to the next. Set to zero to calculate from (pixel size * width_in_pixel_blocks)
        size_t pitch = 0;
    };

    inline static constexpr VkImageSubresourceRange WHOLE_IMAGE = {0, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};

    inline static constexpr VkImageSubresource FIRST_SUB = {};

    /// When image access flags are modified by external rendering process. Call this method to update the image status
    /// to ensure the internal state we tracked matches the actual state of the VkImage.
    /// \param access   Pointer to the new access flags of the VkImage. Can be NULL. In this case, the function will
    ///                 not update the access of the image.
    /// \return         Returns current access flag of the image.
    virtual Access syncAccess(const Access * access) PH_RPS_PURE_METHOD((void) access;);

    Access syncAccess(const Access & access) { return syncAccess(&access); }

    /// Transition image access state.
    virtual void cmdSetAccess(CommandRecorder &, const Access &) PH_RPS_PURE_METHOD();

    /// Record image content read command into the command recorder. The actual reading command is executed later
    /// along with the submission of the command buffer.
    virtual auto cmdRead(CommandRecorder & rec, const VkImageSubresourceRange & range = WHOLE_IMAGE)
        -> std::tuple<ImageDesc, std::shared_ptr<va::BufferObject>> PH_RPS_PURE_METHOD((void) rec; (void) range;);

    /// Record GPU commands to update one mipmap of the image.
    ///
    /// The image data specified by 'data' parameter must maintain valid until the recorded commands are submitted to GPU and
    /// finished execution. Deleting or modifying the memory pointed by the data parameter in any way before that will
    /// lead to undefined behavior including, but not limit to, garbage data or CPU/GPU crash.
    virtual void cmdWriteSubresource(CommandRecorder & rec, const PixelArray & pixels, const VkImageSubresource & subresource = FIRST_SUB)
        PH_RPS_PURE_METHOD((void) rec; (void) pixels; (void) subresource;);

    /// Record GPU command to transfer data between images.
    virtual void cmdCopyTo(CommandRecorder & rec, Image & dst, const VkImageSubresourceRange & sourceRange = WHOLE_IMAGE,
                           const VkImageSubresource & dstSubresource = FIRST_SUB)
        PH_RPS_PURE_METHOD((void) rec; (void) sourceRange; (void) dst; (void) dstSubresource;);

protected:
    using Root::Root;
};

class Sampler : public Root {
public:
    struct CreateParameters : VkSamplerCreateInfo {
        CreateParameters() {
            sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            pNext                   = nullptr;
            flags                   = 0;
            magFilter               = VK_FILTER_LINEAR;
            minFilter               = VK_FILTER_LINEAR;
            mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            mipLodBias              = 0.0f;
            anisotropyEnable        = false;
            maxAnisotropy           = 0.f;
            compareEnable           = false;
            compareOp               = VK_COMPARE_OP_NEVER;
            minLod                  = 0.0f;
            maxLod                  = VK_LOD_CLAMP_NONE;
            borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
            unnormalizedCoordinates = false;
        };

        CreateParameters & setNearest() {
            magFilter  = VK_FILTER_NEAREST;
            minFilter  = VK_FILTER_NEAREST;
            mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            return *this;
        }

        CreateParameters & setLinear() {
            magFilter  = VK_FILTER_LINEAR;
            minFilter  = VK_FILTER_LINEAR;
            mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            return *this;
        }

        CreateParameters & setClampToEdge() {
            addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            return *this;
        }
    };

protected:
    using Root::Root;
};

class Shader : public Root {
public:
    enum ShadingLanguage {
        SPIR_V = 0,
        GLSL   = 1,
    };

    struct CreateParameters {
        /// The stage that this shader is for.
        VkShaderStageFlagBits stage;

        /// Specify the shading language
        ShadingLanguage language;

        /// Pointer to shader code. could be either spir-v binary or source code.
        const char * code;

        /// Specify length of the code in bytes. In the case of GLSL, it is NOT including the null terminator and can be set to 0, if the code is
        /// zero-terminated. For SPIR-V, the length must be the actual length of the binary.
        size_t length = 0;

        /// shader entry point
        const char * entry = 0;
    };

protected:
    using Root::Root;
};

/// @brief A structure to represent one of the following 3 items: image, sampler or combined-image-sampler
///
/// The struct represents an image (either sampled image or storage image), when sampler is empty and image is not.
///
/// The struct represents an sampler, when image is empty and sampler is not.
///
/// The struct represents an combined-image-sampler, when both image and sampler are valid.
///
/// The case that both image and sampler are empty is not allowed and could trigger undefined behavior.
struct ImageSampler {
    Image::View  image;
    Ref<Sampler> sampler;
};

/// Represents one program augment
class Argument : public Root {
public:
    /// bind the argument to buffers
    virtual void set(ConstRange<Buffer::View>) PH_RPS_PURE_METHOD();

    // bind the argument to images and/or samplers
    virtual void set(ConstRange<ImageSampler>) PH_RPS_PURE_METHOD();

    // virtual void set(const ConstRange<Ref<Sampler>> &) PH_RPS_PURE_METHOD();

    // bind the argument to (push) constant
    virtual void set(size_t, const void *) PH_RPS_PURE_METHOD();

protected:
    using Root::Root;
};

/// Collection of program arguments.
class ArgumentSet : public Root {
public:
    struct CreateParameters {
        // Nothing for now. Reserved for future use.
    };

    // clear the argument set to make
    virtual void reset() PH_RPS_PURE_METHOD();

    /// Get the existing or create new argument by name.
    /// Argument instance is owned by the ArgumentSet. So we don't return Ref<Argument> here.
    virtual Argument & getOrCreateArgumentByName(const char *) PH_RPS_PURE_METHOD();

    // This will erase an argument from the set.
    virtual void eraseArgumentByName(const char *) PH_RPS_PURE_METHOD();

    void setb(const char * name, ConstRange<Buffer::View> value) { getOrCreateArgumentByName(name).set(value); }
    void setb(const char * name, const Buffer::View & value) { setb(name, {&value, 1}); }
    void setb(const char * name, const Ref<Buffer> & value) { setb(name, Buffer::View {value}); }

    void seti(const char * name, ConstRange<ImageSampler> value) { getOrCreateArgumentByName(name).set(value); }
    void seti(const char * name, const ImageSampler & value) { seti(name, {&value, 1}); }

    // void setc(const char * name, size_t size, const void * data) { getOrCreateArgumentByName(name).set(size, data); }

protected:
    using Root::Root;
};

/// Represents single GPU pipeline object, either graphics or compute.
struct Program {
    struct ShaderRef {
        Ref<Shader> shader;
        std::string entry = "main";
        bool        empty() const { return shader.empty(); }
    };

    struct Reflection {
        typedef VkDescriptorSetLayoutBinding Descriptor;

        typedef VkPushConstantRange Constant;

        /// Collection of descriptors in one set. We can't use binding point as key, since multiple shader variable might bind to same set and binding point.
        typedef std::unordered_map<std::string, Descriptor> DescriptorSet;

        /// Collection of descriptor sets indexed by their set index in shader.
        typedef std::vector<DescriptorSet> DescriptorLayout;

        /// Collection of push constants.
        typedef std::unordered_map<std::string, Constant> ConstantLayout;

        /// Properties of vertex shader input.
        struct VertexShaderInput {
            uint32_t location = 0;
            VkFormat format   = VK_FORMAT_UNDEFINED;
        };

        /// Collection of vertex shader input.
        typedef std::unordered_map<std::string, VertexShaderInput> VertexLayout;

        std::string      name; ///< name of the program that this reflect is from. this field is for logging and debugging.
        DescriptorLayout descriptors;
        ConstantLayout   constants;
        VertexLayout     vertex;
    };

    static inline constexpr size_t DRAW_TIER    = 0;
    static inline constexpr size_t PROGRAM_TIER = 1;
    static inline constexpr size_t PASS_TIER    = 2;
    static inline constexpr size_t GLOBAL_TIER  = 3;

    struct ArgumentSetBinding {
        size_t           tier;
        Ref<ArgumentSet> args;
    };

    struct PushConstantBinding {
        const char * name;
        const void * value;
    };

    virtual Reflection reflect() const PH_RPS_PURE_METHOD();
};

class ComputeProgram : public Root, public Program {
public:
    struct CreateParameters {
        ShaderRef cs;
    };

    struct DispatchParameters {
        ConstRange<ArgumentSetBinding> arguments;
        size_t                         width  = 1;
        size_t                         height = 1;
        size_t                         depth  = 1;
    };

    /// Record an dispatch call to command buffer. Only available for compute program.
    virtual void cmdDispatch(CommandRecorder &, const DispatchParameters &) PH_RPS_PURE_METHOD();

protected:
    using Root::Root;
};

class Pass;

class GraphicsProgram : public Root, public Program {
public:
    struct VertexElement {
        uint32_t offset; /// offset in bytes of the element within the vertex structure
        VkFormat format; ///< format of the element.
    };

    struct VertexBinding {
        std::unordered_map<std::string, VertexElement> elements;

        /// distance in bytes from one vertex to next
        size_t stride = 0;

        /// True means this vertex binding contains per-instance data. Default is false (meaning per-vertex data).
        bool perInstance = false;
    };

    typedef std::vector<VertexBinding> VertexInput;

    struct CreateParameters {
        VkRenderPass        pass    = 0;
        size_t              subpass = 0;
        ShaderRef           vs, fs;
        VertexInput         vertex;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        int                 depth    = 0; // 0: disabled, 1: enabled, 2: read-only
        int                 blend    = 0; // 0: opaque,   1: enabled.

        CreateParameters & enableDepth() {
            depth = 1;
            return *this;
        }
        CreateParameters & enableAlphaBlending() {
            blend = 1;
            return *this;
        }
    };

    struct DrawParameters {
        ConstRange<ArgumentSetBinding>  arguments;
        ConstRange<PushConstantBinding> constants;

        ConstRange<Buffer::View> vertices;
        Buffer::View             indices; ///< indicates indexed draw if not empty.

        uint32_t instanceCount = 1; ///< instance count
        uint32_t firstInstance = 0; ///< index of the first instance.

        union {
            /// Vertex count for non-indexed draw.
            uint32_t vertexCount = 0;

            /// Index count for indexed draw.
            uint32_t indexCount;
        };

        union {
            /// Index of the first vertex for non-indexed draw.
            uint32_t firstVertex = 0;

            /// Index of the first index of the indexed draw.
            uint32_t firstIndex;
        };

        /// Vertex offset of indexed draw. Ignored for non-indexed draw.
        int32_t vertexOffset = 0;

        /// Can be 1, 2, 4 for 8/16/32 bit index buffer. Ignored for non-indexed draw.
        uint8_t indexStride = 2;

        DrawParameters & setNonIndexed(size_t vertexCount_, size_t firstVertex_ = 0) {
            indices.buffer.clear();
            vertexCount = (uint32_t) vertexCount_;
            firstVertex = (uint32_t) firstVertex_;
            return *this;
        }

        DrawParameters & setIndexed(const Buffer::View & ib, size_t indexCount_, size_t firstIndex_ = 0, int32_t vertexOffset_ = 0, size_t indexStride_ = 2) {
            indices      = ib;
            indexCount   = (uint32_t) indexCount_;
            firstIndex   = (uint32_t) firstIndex_;
            vertexOffset = vertexOffset_;
            indexStride  = (uint8_t) indexStride_;
            return *this;
        }

        DrawParameters & setInstance(size_t count, size_t first = 0) {
            instanceCount = (uint32_t) count;
            firstInstance = (uint32_t) first;
            return *this;
        }
    };

    virtual void cmdDraw(CommandRecorder &, const DrawParameters &) PH_RPS_PURE_METHOD();

protected:
    using Root::Root;
};

/// Represents one graphics render pass of the scene, which contains at least 1 sub pass in it.
class Pass : public Root {
public:
    enum InputState {
        CLEAR = 0,
        LOAD,
    };

    enum OutputState {
        DISCARD = 0,
        STORE,
    };

    struct AttachmentDesc {
        VkFormat    format;
        InputState  input  = CLEAR;
        OutputState output = STORE;
    };

    /// Describe attachment reference of each subpass.
    /// The size_t is an index into the CreateParameters::attachments array.
    struct SubpassDesc {
        std::vector<size_t>   inputs;
        std::vector<size_t>   colors;
        std::optional<size_t> depthStencil;
        // TODO: readonly depth
    };

    struct CreateParameters {
        std::vector<AttachmentDesc> attachments;
        std::vector<SubpassDesc>    subpasses;
    };

    struct RenderTarget {
        Image::View view;

        /// Specify clear value of the render target. This field is only effective on attachment with "CLEAR" input state.
        VkClearValue clear = {};

        RenderTarget & setClearColorF(float x, float y, float z, float w) {
            clear.color.float32[0] = x;
            clear.color.float32[1] = y;
            clear.color.float32[2] = z;
            clear.color.float32[3] = w;
            return *this;
        }

        RenderTarget & setClearColorI(int32_t x, int32_t y, int32_t z, int32_t w) {
            clear.color.int32[0] = x;
            clear.color.int32[1] = y;
            clear.color.int32[2] = z;
            clear.color.int32[3] = w;
            return *this;
        }

        RenderTarget & setClearColorU(uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
            clear.color.uint32[0] = x;
            clear.color.uint32[1] = y;
            clear.color.uint32[2] = z;
            clear.color.uint32[3] = w;
            return *this;
        }

        RenderTarget & setClearDepthStencil(float d, uint32_t s = 0) {
            clear.depthStencil.depth   = d;
            clear.depthStencil.stencil = s;
            return *this;
        }
    };

    struct BeginParameters {
        ConstRange<RenderTarget> targets;
        VkRect2D                 renderArea = {}; /// set to (0, 0, 0, 0) to use size of render targets.
    };

    virtual auto handle() const -> VkRenderPass PH_RPS_PURE_METHOD();
    virtual bool cmdBegin(CommandRecorder &, const BeginParameters &) PH_RPS_PURE_METHOD();
    virtual void cmdEnd(CommandRecorder &) PH_RPS_PURE_METHOD();

protected:
    using Root::Root;
};

// /// root item that can be put into a scene.
// class Entity : public Root {

// };

// class Mesh : public Entity {

// };

// class Material : public Entity {

// };

// class Model : public Entity {

// };

// ///
// class Scene : public Root {
// public:
//     struct CreateParameters {
//         //
//     };
// };

/// this is the factory of the rest of the RPS object
class Factory : public Root {
public:
    PH_NO_COPY_NO_MOVE(Factory);

    /// Parameters to create new factory instance
    struct CreateParameters {
        /// main submission queue that is able to handle all type of workloads (graphics, compute, transfer)
        va::VulkanSubmissionProxy * main = nullptr;

        /// Optional async-compute queue.
        va::VulkanSubmissionProxy * asyncCompute = nullptr;

        /// Optional async transfer (DMA) queue.
        va::VulkanSubmissionProxy * asyncTransfer = nullptr;
    };

    /// Create new RPS factory instance.
    ///
    /// The factory instance is deleted automatically after all items in the factory are released
    /// and after all references to the factory pointer are released, which ever comes later.
    static auto createFactory(const CreateParameters &) -> Ref<Factory>;

    // virtual auto deserialize(ConstRange<uint8_t>) -> Ref<Root> PH_RPS_PURE_METHOD();
    // virtual auto loadFromAsset(Asset const char * assetPath) -> Ref<Root> PH_RPS_PURE_METHOD((void) assetPath;);

    // factory methods
    virtual auto createBuffer(const Buffer::CreateParameters &, const char * = nullptr) -> Ref<Buffer> PH_RPS_PURE_METHOD();
    virtual auto importBuffer(const Buffer::ImportParameters &, const char * = nullptr) -> Ref<Buffer> PH_RPS_PURE_METHOD();
    virtual auto createImage(const Image::CreateParameters1 &, const char * = nullptr) -> Ref<Image> PH_RPS_PURE_METHOD();
    virtual auto createImage(const Image::CreateParameters2 &, const char * = nullptr) -> Ref<Image> PH_RPS_PURE_METHOD();
    virtual auto importImage(const Image::ImportParameters &, const char * = nullptr) -> Ref<Image> PH_RPS_PURE_METHOD();
    virtual auto createSampler(const Sampler::CreateParameters &, const char * = nullptr) -> Ref<Sampler> PH_RPS_PURE_METHOD();
    virtual auto createShader(const Shader::CreateParameters &, const char * = nullptr) -> Ref<Shader> PH_RPS_PURE_METHOD();
    virtual auto createComputeProgram(const ComputeProgram::CreateParameters &, const char * = nullptr) -> Ref<ComputeProgram> PH_RPS_PURE_METHOD();
    virtual auto createGraphicsProgram(const GraphicsProgram::CreateParameters &, const char * = nullptr) -> Ref<GraphicsProgram> PH_RPS_PURE_METHOD();
    virtual auto createArgumentSet(const ArgumentSet::CreateParameters &, const char * = nullptr) -> Ref<ArgumentSet> PH_RPS_PURE_METHOD();
    virtual auto createPass(const Pass::CreateParameters &, const char * = nullptr) -> Ref<Pass> PH_RPS_PURE_METHOD();

    Ref<Shader> createGLSLShader(VkShaderStageFlagBits stage, const char * source, const char * entry = nullptr) {
        return createShader(Shader::CreateParameters {stage, Shader::GLSL, source, 0, entry});
    }

protected:
    using Root::Root;
};

/// Command recorder represents single command buffer that RPS module uses to record device command.
/// This class is not managed by Factory class, since it has one pure virtual method: deferUntilGPUWorkIsDone(...).
/// RPS module user will need to subclass CommandRecorder and provides implementation of that method.
class CommandRecorder : public va::DeferredHostOperation {
public:
    virtual ~CommandRecorder() {}

    /// The command buffer that GPU commands will be recorded to. Must be assigned to a valid handle to Vulkan command
    /// buffer before calling any other
    VkCommandBuffer commands() {
        PH_ASSERT(_commands);
        return _commands;
    }

    /// Update the command buffer that the command recorder attaches to
    CommandRecorder & setCommands(VkCommandBuffer cb) {
        // PH_ASSERT2(!_pass, "Can't change command buffer in the middle of render pass.");
        _commands = cb;
        return *this;
    }

protected:
    CommandRecorder(const va::VulkanGlobalInfo & vgi): va::DeferredHostOperation(vgi) {}

private:
    VkCommandBuffer _commands = 0;
};

/// An sample command recorder implementation that records and executes GPU commands synchronously.
class SynchronousCommandRecorder : public rps::CommandRecorder {
    ph::va::SingleUseCommandPool     _pool;
    std::list<std::function<void()>> _jobs;

    void runAllPendingJobs() {
        for (auto & f : _jobs) f();
        _jobs.clear();
    }

public:
    SynchronousCommandRecorder(va::VulkanSubmissionProxy & vsp): CommandRecorder(vsp.vgi()), _pool(vsp) {}

    ~SynchronousCommandRecorder() { runAllPendingJobs(); }

    template<typename FUNC>
    void syncExec(FUNC f) {
        auto cb = _pool.create();
        setCommands(cb.cb);
        f();
        _pool.submit(cb);
        _pool.finish();
        runAllPendingJobs();
    }

    void deferUntilGPUWorkIsDone(std::function<void()> job) override { _jobs.push_back(std::move(job)); }
};

/// Another sample command recorder implementation to work with va::SimpleRenderLoop class.
class RenderLoopCommandRecorder : public rps::CommandRecorder {
    va::SimpleRenderLoop & _loop;

public:
    RenderLoopCommandRecorder(va::SimpleRenderLoop & loop): rps::CommandRecorder(loop.cp().dev.vgi()), _loop(loop) {}

    void deferUntilGPUWorkIsDone(std::function<void()> job) override { _loop.deferUntilGPUWorkIsDone(std::move(job)); }
};

} // namespace ph::rps

#include "rps/rps.inl"
