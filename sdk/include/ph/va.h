/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This is the main public interface of physray-va module
#pragma once

#include <ph/base.h>

// ---------------------------------------------------------------------------------------------------------------------
// this header must be included before the standard vulkan header.
#ifdef VULKAN_H_
    #error "Can't include <vulkan.h> before <ph/va.h>"
#endif

// ---------------------------------------------------------------------------------------------------------------------
/// On 32-bit platform, vulkan.h defines all handle types as uint64_t, which prevents compile time type traits on
/// handles from working properly. This macro refines all vulkan handles to unique structure that encapsulate
/// a 64-bit integer value.
#if !defined(__LP64__) && !defined(_WIN64) && (!defined(__x86_64__) || defined(__ILP32__)) && !defined(_M_X64) && !defined(__ia64) && !defined(_M_IA64) && \
    !defined(__aarch64__) && !defined(__powerpc64__)
    #define PH_SDK_CUSTOM_VK_HANDLE 1
    #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object)                        \
        typedef struct object##_T {                                          \
            uint64_t u64;                                                    \
            constexpr object##_T(): u64(0) {}                                \
            constexpr object##_T(uint64_t u): u64(u) {}                      \
            constexpr object##_T(int u): u64((uint64_t) u) {}                \
            constexpr object##_T(nullptr_t): u64(0) {}                       \
            constexpr object##_T(const object##_T &) = default;              \
            constexpr object##_T & operator=(const object##_T &) = default;  \
            constexpr object##_T(object##_T &&)                  = default;  \
            constexpr object##_T & operator=(object##_T &&) = default;       \
            constexpr              operator uint64_t() const { return u64; } \
        } object;
#endif

// ---------------------------------------------------------------------------------------------------------------------
// Include volk.h
#if PH_MSWIN && !defined(VK_USE_PLATFORM_WIN32_KHR)
    #define VK_USE_PLATFORM_WIN32_KHR
#elif PH_ANDROID && !defined(VK_USE_PLATFORM_ANDROID_KHR)
    #define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <volk/volk.h>
// All contents in <vulkan.h> are already defined by volk.h. So we need to prevent <vulkan.h> from being included.
#ifndef VULKAN_H_
    #define VULKAN_H_
#endif

// Note: We don't include Vulkan C++ binding header (vulkan.hpp) here, since it slows down the build speed a lot.

// ---------------------------------------------------------------------------------------------------------------------
// Include Eigen headers
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #if !PH_BUILD_DEBUG && !PH_ANDROID
        #pragma GCC diagnostic ignored "-Wmaybe-uninitialized" // Eigen 3.3.9 generate this warning on release build.
    #endif
    #ifdef __clang__
        #pragma GCC diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
    #endif
#endif
#define EIGEN_MPL2_ONLY // Disable LGPL related features.
#include <Eigen/Eigen>  // Eigne is our official 3D math library.
#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

// ---------------------------------------------------------------------------------------------------------------------
// Include VMA allocator header
#if PH_MSWIN
    #pragma warning(push, 0)
#elif PH_UNIX_LIKE
    #pragma GCC diagnostic push
    #if PH_ANDROID
        #pragma GCC diagnostic ignored "-Wnullability-completeness"
    #endif
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wtype-limits"
    #pragma GCC diagnostic ignored "-Wformat"
    #pragma GCC diagnostic ignored "-Wundef"
#endif

// Enable these defines to help debug VK memory corruption.
// #ifndef VMA_DEBUG_DETECT_CORRUPTION
//     #define VMA_DEBUG_DETECT_CORRUPTION 1
//     #define VMA_DEBUG_MARGIN            32
// #endif

#ifndef VMA_DEBUG_ERROR_LOG
    #define VMA_DEBUG_ERROR_LOG PH_LOGE
#endif

#include <vk_mem_alloc.h>
#if PH_MSWIN
    #pragma warning(pop)
#elif PH_UNIX_LIKE
    #pragma GCC diagnostic pop
#endif

#include <cassert>
#include <functional>
#include <type_traits>

// ---------------------------------------------------------------------------------------------------------------------
// Error check helpers
#define PH_VKCHK(func, actionOnFailure)                                            \
    if (true) {                                                                    \
        auto result__ = (func);                                                    \
        /* there are a few positive success code other than VK_SUCCESS */          \
        if (result__ < 0) {                                                        \
            PH_LOGE("%s failed: %s", #func, ::ph::va::VkResultToString(result__)); \
            actionOnFailure;                                                       \
        }                                                                          \
    } else                                                                         \
        void(0)

/// Check on Vulkan function call, call the 'actionOnFailure' when the function returns negative failure code.
#define PH_VA_CHK(func, actionOnFailure) PH_VKCHK(func, actionOnFailure)

/// Check on vulkan functions call, throw exception when function failed (returned negative VkResult)
#define PH_VA_REQUIRE(x) PH_VKCHK(x, PH_THROW(#x))

// ---------------------------------------------------------------------------------------------------------------------
/// main name space for Vulkan Accelerator module
namespace ph {
namespace va {

// ---------------------------------------------------------------------------------------------------------------------
/// convert VKResult to string
PH_API const char * VkResultToString(VkResult);

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Generate Vulkan compatible perspective projection matrix for right handed system.
///
/// Note that according to Vulkan spec, clip space is always right handed:
///     X (-1, 1) points left
///     Y (-1, 1) points down
///     Z ( 0, 1) points into screen (away from camera)
///
/// The "RH" in the function name indicates the handedness of the source coordinate system: x -> right, y -> up, z -> back
///
/// \param fovy     Vertical field of view in radian
/// \param aspect   Aspect ratio (width : height)
/// \param zNear    Distance of near plane
/// \param zFar     Distance of far plane
PH_API Eigen::Matrix4f perspectiveRH(float fovy, float aspect, float zNear, float zFar);

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Generate Vulkan compatible perspective projection matrix for left handed system.
///
/// Note that according to Vulkan spec, clip space is always right handed:
///     X (-1, 1) points left
///     Y (-1, 1) points down
///     Z ( 0, 1) points into screen (away from camera)
///
/// The "LH" in the function name indicates the handedness of the source coordinate system: x -> right, y -> up, z -> front
///
/// \param fovy     Vertical field of view in radian
/// \param aspect   Aspect ratio (width : height)
/// \param zNear    Distance of near plane
/// \param zFar     Distance of far plane
PH_API Eigen::Matrix4f perspectiveLH(float fovy, float aspect, float zNear, float zFar);

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Build Vulkan compatible orthographic matrix for right handed system.
///
/// In Vulkan spec, clip space is always right handed:
///     X (-1, 1) -> right
///     Y (-1, 1) -> down
///     Z ( 0, 1) -> front (in to screen, away from viewer)
///
/// The "RH" in the function name indicates the handedness of the source coordinate system: x -> right, y -> up, z -> back
PH_API Eigen::Matrix4f orthographicRH(float l, float r, float b, float t, float zn, float zf);
PH_API Eigen::Matrix4f orthographicRH(float w, float h, float zn, float zf);

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Build Vulkan compatible orthographic matrix for right handed system.
///
/// In Vulkan spec, clip space is always right handed:
///     X (-1, 1) -> right
///     Y (-1, 1) -> down
///     Z ( 0, 1) -> front (in to screen, away from viewer)
///
/// The "LH" in the function name indicates the handedness of the source coordinate system: x -> right, y -> up, z -> front
PH_API Eigen::Matrix4f orthographicLH(float l, float r, float b, float t, float zn, float zf);
PH_API Eigen::Matrix4f orthographicLH(float w, float h, float zn, float zf);

// ---------------------------------------------------------------------------------------------------------------------
//
inline Eigen::Matrix4f orthographic(float w, float h, float zn, float zf, bool leftHanded = false) {
    return leftHanded ? orthographicLH(w, h, zn, zf) : orthographicRH(w, h, zn, zf);
}

// ---------------------------------------------------------------------------------------------------------------------
/// Build a right handed look at view matrix.
/// \param eye          Position of the camera
/// \param center       Position where the camera is looking at
/// \param up           Normalized up vector, how the camera is oriented. Typically (0, 0, 1)
/// \param leftHanded   True when rendering in left-handed mode. False by default.
PH_API Eigen::Matrix4f lookAt(Eigen::Vector3f const & eye, Eigen::Vector3f const & center, Eigen::Vector3f const & up, bool leftHanded = false);
// ---------------------------------------------------------------------------------------------------------------------
/// Build a right handed look at view matrix.
/// \param eye      Position of the camera
/// \param center   Position where the camera is looking at
/// \param up       Normalized up vector, how the camera is oriented. Typically (0, 0, 1)
PH_API Eigen::Matrix4f lookAtRH(Eigen::Vector3f const & eye, Eigen::Vector3f const & center, Eigen::Vector3f const & up);

// ---------------------------------------------------------------------------------------------------------------------
/// Build a left handed look at view matrix.
/// \param eye      Position of the camera
/// \param center   Position where the camera is looking at
/// \param up       Normalized up vector, how the camera is oriented. Typically (0, 0, 1)
PH_API Eigen::Matrix4f lookAtLH(Eigen::Vector3f const & eye, Eigen::Vector3f const & center, Eigen::Vector3f const & up);

// ---------------------------------------------------------------------------------------------------------------------
//
inline VkRect2D viewportToScissor(const VkViewport vp, uint32_t maxWidth, uint32_t maxHeight) {
    float l = vp.x, t = vp.y, r = vp.x + vp.width, b = vp.y + vp.height;
    if (l > r) std::swap(l, r);
    if (t > b) std::swap(t, b);
    auto x = clamp<int32_t>((int32_t) std::max(l, .0f), 0, maxWidth - 1);
    auto y = clamp<int32_t>((int32_t) std::max(t, .0f), 0, maxHeight - 1);
    auto w = clamp<uint32_t>((uint32_t) std::max(r, .0f), 0, maxWidth) - (uint32_t) x;
    auto h = clamp<uint32_t>((uint32_t) std::max(b, .0f), 0, maxHeight) - (uint32_t) y;
    return {VkOffset2D {x, y}, VkExtent2D {w, h}};
}

// ---------------------------------------------------------------------------------------------------------------------
/// A helper function to insert resource/memory barriers to command buffer
struct SimpleBarriers {
    VkPipelineStageFlags               srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkPipelineStageFlags               dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    std::vector<VkMemoryBarrier>       memory;
    std::vector<VkBufferMemoryBarrier> buffers;
    std::vector<VkImageMemoryBarrier>  images;

    void clear() {
        memory.clear();
        buffers.clear();
        images.clear();
    }

    SimpleBarriers & reserveBuffers(size_t count) {
        buffers.reserve(count);
        return *this;
    }

    SimpleBarriers & addMemory(VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
        memory.emplace_back(VkMemoryBarrier {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            nullptr,
            srcAccess,
            dstAccess,
        });
        return *this;
    }

    SimpleBarriers & addBuffer(VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE,
                               VkAccessFlags srcAccess = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
                               VkAccessFlags dstAccess = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT) {
        if (!buffer) return *this;
        buffers.emplace_back(VkBufferMemoryBarrier {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, srcAccess, dstAccess, VK_QUEUE_FAMILY_IGNORED,
                                                    VK_QUEUE_FAMILY_IGNORED, buffer, offset, size});
        return *this;
    }

    SimpleBarriers & addImage(VkImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout,
                              VkImageSubresourceRange subresourceRange) {
        if (!image) return *this;
        images.emplace_back(VkImageMemoryBarrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, srcAccess, dstAccess, oldLayout, newLayout,
                                                  VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, subresourceRange});
        return *this;
    }

    SimpleBarriers & addImage(VkImage image, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout,
                              VkImageAspectFlags aspect) {
        VkImageSubresourceRange range = {aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        return addImage(image, srcAccess, dstAccess, oldLayout, newLayout, range);
    }

    SimpleBarriers & setStages(VkPipelineStageFlags src, VkPipelineStageFlags dst) {
        srcStage = src;
        dstStage = dst;
        return *this;
    }

    void write(VkCommandBuffer cb) const {
        if (memory.empty() && buffers.empty() && images.empty()) return;
        vkCmdPipelineBarrier(cb, srcStage, dstStage, {}, (uint32_t) memory.size(), memory.data(), (uint32_t) buffers.size(), buffers.data(),
                             (uint32_t) images.size(), images.data());
    }
};

// ---------------------------------------------------------------------------------------------------------------------
/// You should ONLY USE THIS FOR DEBUGGING - this is not something that should ever ship in real code,
/// this will flush and invalidate all caches and stall everything. It is a tool not to be used lightly!
/// That said, it can be really handy if you think you have a race condition in your app and you just want
/// to serialize everything so you can debug it.
///
/// Note that this does not take care of image layouts - if you're debugging you can set the layout of all
/// your images to GENERAL to overcome this, but again - do not do this in release code!
///
/// (code and comments are referenced from: https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples)
inline void fullPipelineBarrier(VkCommandBuffer cb) {
    VkMemoryBarrier memoryBarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        nullptr,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
    };
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, {}, 1, &memoryBarrier, 0, nullptr, // buffer barrier
                         0, nullptr);                                                                                                   // image barrier
}

// ---------------------------------------------------------------------------------------------------------------------
/// @brief Get object type of a VK handle.
/// The base function is intentionally left undefined. See va.inl for all specialized versions.
/// \todo simplify this with "if constexpr" expression in c++17.
template<typename HANDLE>
inline VkObjectType getHandleObjectType();

// ---------------------------------------------------------------------------------------------------------------------
/// A little struct to hold all essential Vulkan global objects. This is to make it easier to reference key Vulkan
/// information across the library.
struct VulkanGlobalInfo {
    const VkAllocationCallbacks * allocator    = nullptr;
    VkInstance                    instance     = nullptr;
    VkPhysicalDevice              phydev       = nullptr;
    VkDevice                      device       = nullptr;
    VmaAllocator                  vmaAllocator = nullptr;

    bool operator==(const VulkanGlobalInfo & rhs) const {
        return allocator == rhs.allocator && instance == rhs.instance && phydev == rhs.phydev && device == rhs.device;
    }

    bool operator!=(const VulkanGlobalInfo & rhs) const { return !operator==(rhs); }

    template<typename HANDLE>
    void safeDestroy(HANDLE &, VmaAllocation & = DUMMY_ALLOCATION()) const;

private:
    static VmaAllocation & DUMMY_ALLOCATION() {
        static VmaAllocation a {0};
        return a;
    };
};

/// calling vkDeviceWaitIdle() in thread safe manner (protected by a mutex).
PH_API VkResult threadSafeDeviceWaitIdle(VkDevice device);

#ifndef PH_ALLOW_VK_DEVICE_WAIT_IDLE
    #undef vkDeviceWaitIdle
    #define vkDeviceWaitIdle ph::va::threadSafeDeviceWaitIdle
#endif

/// Pooled allocator to deal with dynamic memory allocation in AutoHandle class.
//@{
PH_API void * allocateAutoHandle(size_t);
PH_API void   deallocateAutoHandle(void *, size_t);
//@}

// ---------------------------------------------------------------------------------------------------------------------
/// Helper class to manage Vulkan handle life time. Automaticaly delete it when it is not referenced.
template<typename T>
class AutoHandle {

public:
    /// default constructor
    AutoHandle() noexcept = default;

    /// copy constructor
    AutoHandle(const AutoHandle & rhs) noexcept {
        _handle = rhs._handle;
        _ref    = rhs._ref;
        if (_ref) _ref->add();
    }

    /// move constructor
    AutoHandle(AutoHandle && rhs) noexcept {
        _handle     = rhs._handle;
        _ref        = rhs._ref;
        rhs._handle = 0;
        rhs._ref    = 0;
    }

    /// destructor
    ~AutoHandle() noexcept {
        try {
            clear();
        } catch (...) {
            // log?
        }
    }

    /// copy operator
    AutoHandle & operator=(const AutoHandle & rhs) noexcept {
        if (this == rhs.addr()) { return *this; }
        if (_handle == rhs._handle) {
            PH_ASSERT(_ref == rhs._ref);
            return *this;
        }
        clear();
        _handle = rhs._handle;
        _ref    = rhs._ref;
        if (_ref) _ref->add();
        return *this;
    }

    /// move operator
    AutoHandle & operator=(AutoHandle && rhs) noexcept {
        if (this == rhs.addr()) { return *this; }
        clear();
        _handle     = rhs._handle;
        _ref        = rhs._ref;
        rhs._handle = 0;
        rhs._ref    = 0;
        return *this;
    }

    bool operator==(const AutoHandle & rhs) const noexcept { return _handle == rhs.handle; }
    bool operator==(T rhs) const noexcept { return _handle == rhs; }
    bool operator!=(const AutoHandle & rhs) const noexcept { return _handle == rhs.handle; }
    bool operator!=(T rhs) const noexcept { return _handle == rhs; }
    bool operator!() const noexcept { return !_handle; }

    // Only use this when referencing a valid handle for read. If you need to modify/recreate
    // the handle, call prepare().
    const T * operator&() const {
        // If your code triggers this assert failure, and you are creating new handle, please use
        // prepare() method instead.
        PH_ASSERT(_handle);

        return &_handle;
    }

    operator const T &() const { return _handle; }

    operator bool() const { return !!_handle; }

    const T & get() const { return _handle; }

    bool empty() const { return !_handle; }

    /// delete the underlying handle.
    void clear() {
        if (_ref && _ref->dec()) {
            PH_ASSERT(0 == _ref->c);
            if (_handle) _ref->d(_handle);
            delete _ref;
        }
        _handle = 0;
        _ref    = 0;
    }

    // TODO: void attach(T );

    /// Detach from underlying handle. Note that detach is only allowed if ths is the last reference to the handle.
    ///
    /// If there are more than one reference to the handle, detach() function will behave just like clear() and return
    /// a null handle.
    T detach() {
        T detached = 0;
        if (_ref) {
            if (_ref->dec()) {
                PH_ASSERT(0 == _ref->c);
                delete _ref;
                detached = _handle;
            } else {
                PH_LOGE("Can't detach from VK handle, when it is referenced more than once.");
            }
            _handle = 0;
            _ref    = 0;
        }
        PH_ASSERT(!_handle);
        return detached;
    }

    /// Clear any preexisting handles. Return a pointer that is ready to pass to
    /// Vulkan's create function for creating a new handle.
    ///
    /// \param deleter This can be either a reference to VulkanGlobalInfo object, or a function that accepts
    ///               calling signature of: "void (T)". It'll be used delete the underlying handle.
    ///
    /// Here's an example of how it should be used:
    ///
    ///     AutoHandle<VkCommandPool> pool;
    ///     VkCreateCommandPool(device, &ci, allocator, pool.prepare([](auto h){ vkDestroyCommandPool(h); }));
    ///
    template<typename Deleter>
    T * prepare(Deleter deleter) {
        clear(); // release old handle.
        PH_ASSERT(0 == _handle && 0 == _ref);
        _ref    = new RefCounter();
        _ref->d = initDeleter<Deleter>(deleter);
        return &_handle;
    }

    /// Clear any preexisting handles. Return a pointer that is ready to pass to
    /// Vulkan's create function for creating a new handle.
    T * prepare(const VulkanGlobalInfo & vgi) {
        return prepare([&vgi](auto & h) { vgi.safeDestroy(h); });
    }

private:
    template<typename Deleter>
    Deleter & initDeleter(Deleter & d) {
        return d;
    }

    template<typename VulkanGlobalInfo>
    auto initDeleter(const VulkanGlobalInfo & vgi) {
        return [&](auto & h) { vgi.safeDestroy(h); };
    }

    // because & operator is overloaded. So we need alternative way to get the address of the class.
    const AutoHandle * addr() const { return this; }
    AutoHandle *       addr() { return this; }

    struct RefCounter {
        std::atomic<int>         c {1};
        std::function<void(T &)> d;

        void add() { ++c; }

        // Return true, means the refcountre reaches zero. thus, time to release it.
        bool dec() {
            int oldValue = c.fetch_sub(1);
            PH_ASSERT(oldValue > 0);
            return 1 == oldValue;
        }

        static void * operator new(size_t size) { return allocateAutoHandle(size); }
        static void   operator delete(void * ptr, size_t size) { return deallocateAutoHandle(ptr, size); }
    };

    T            _handle = 0;
    RefCounter * _ref    = 0;
};

} // namespace va
} // namespace ph

// Do _not_ let clang-format reorder these headers
// clang-format off
#include "va/va.inl"
#include "va/debug.inl"
#include "va/memory.inl"
#include "va/shader.inl"
#include "va/buffer.inl"
#include "va/image.inl"
#include "va/deferred-host-operation.inl"
#include "va/command.inl"
#include "va/initializers.inl"
#include "va/info.inl"
#include "va/async-timestamp.inl"
#include "va/device.inl"
#include "va/swapchain.inl"
#include "va/render-loop.inl"
#include "va/descriptor.inl"
#include "va/compute.inl"
#include "va/pipeline.inl"
// clang-format on