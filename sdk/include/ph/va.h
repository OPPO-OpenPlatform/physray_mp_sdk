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
    #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) \
        typedef struct object##_T {                   \
            uint64_t u64;                             \
            object##_T() = default;                   \
            object##_T(uint64_t u): u64(u) {}         \
            object##_T(int u): u64((uint64_t) u) {}   \
            object##_T(nullptr_t): u64(0) {}          \
            PH_DEFAULT_COPY(object##_T);              \
            PH_DEFAULT_MOVE(object##_T);              \
            operator uint64_t() const { return u64; } \
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
#if !PH_BUILD_DEBUG && defined(__GNUC__) && !PH_ANDROID
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized" // Eigen 3.3.9 generate this warning on release build.
#endif
#define EIGEN_MPL2_ONLY // Disable LGPL related features.
#include <Eigen/Eigen>  // Eigne is our official 3D math library.
#if !PH_BUILD_DEBUG && defined(__GNUC__) && !PH_ANDROID
    #pragma GCC diagnostic pop
#endif

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
//#if PH_BUILD_DEBUG
//#ifndef VMA_DEBUG_DETECT_CORRUPTION
//#define VMA_DEBUG_DETECT_CORRUPTION 1
//#define VMA_DEBUG_MARGIN 32
//#endif
//#endif
#include <vk_mem_alloc.h>
#if PH_MSWIN
    #pragma warning(pop)
#elif PH_UNIX_LIKE
    #pragma GCC diagnostic pop
#endif

#include <atomic>
#include <cassert>
#include <functional>
#include <set>
#include <type_traits>

// ---------------------------------------------------------------------------------------------------------------------
// Error check helpers
#define PH_VKCHK(func, actionOnFailure)                                            \
    if constexpr (true) {                                                          \
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
namespace ph::va {

// ---------------------------------------------------------------------------------------------------------------------
/// convert VKResult to string
PH_API const char * VkResultToString(VkResult);

// ---------------------------------------------------------------------------------------------------------------------
/// Generate right handed perspective projection matrix for Vulkan
/// \param fovy     Vertical field of view in radian
/// \param aspect   Aspect ratio (width : height)
/// \param zNear    Distance of near plane
/// \param zFar     Distance of far plane
PH_API Eigen::Matrix4f perspectiveRH(float fovy, float aspect, float zNear, float zFar);

// ---------------------------------------------------------------------------------------------------------------------
/// Generate right handed perspective projection matrix for Vulkan
/// \param fovy     Vertical field of view in radian
/// \param aspect   Aspect ratio (width : height)
/// \param zNear    Distance of near plane
/// \param zFar     Distance of far plane
PH_API Eigen::Matrix4f perspectiveLH(float fovy, float aspect, float zNear, float zFar);

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

PH_API Eigen::Matrix4f orthographic(float w, float h, float zn, float zf, bool leftHanded = false);
PH_API Eigen::Matrix4f orthographicLH(float w, float h, float zn, float zf);
PH_API Eigen::Matrix4f orthographicRH(float w, float h, float zn, float zf);
PH_API Eigen::Matrix4f orthographicLH(float l, float r, float b, float t, float zn, float zf);
PH_API Eigen::Matrix4f orthographicRH(float l, float r, float b, float t, float zn, float zf);

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
/// A helper function to insert a Vulkan memory barrier to command buffer
inline void memoryBarrier(VkCommandBuffer cb, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess) {
    VkMemoryBarrier barrier {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        nullptr,
        srcAccess,
        dstAccess,
    };
    vkCmdPipelineBarrier(cb, srcStage, dstStage, {}, // dependency flags
                         1, &barrier,                // memory barrier
                         0, nullptr,                 // buffer barrier
                         0, nullptr);                // image barrier
}

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
// Get object type of a VK handle.
template<typename HANDLE>
inline VkObjectType getHandleObjectType() {
    if constexpr (std::is_same_v<HANDLE, uint64_t>) {
        static_assert(AlwaysFalse<HANDLE>, "Can't use built-in integer as handle type. If you are on 32-bit platform, "
                                           "please override VK_DEFINE_NON_DISPATCHABLE_HANDLE to give each Vulkan handle an unique type.");
        return VK_OBJECT_TYPE_UNKNOWN;
    } else if constexpr (std::is_same_v<HANDLE, VkBuffer>) {
        return VK_OBJECT_TYPE_BUFFER;
    } else if constexpr (std::is_same_v<HANDLE, VkCommandPool>) {
        return VK_OBJECT_TYPE_COMMAND_POOL;
    } else if constexpr (std::is_same_v<HANDLE, VkCommandBuffer>) {
        return VK_OBJECT_TYPE_COMMAND_BUFFER;
    } else if constexpr (std::is_same_v<HANDLE, VkDescriptorPool>) {
        return VK_OBJECT_TYPE_DESCRIPTOR_POOL;
    } else if constexpr (std::is_same_v<HANDLE, VkDescriptorSetLayout>) {
        return VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
    } else if constexpr (std::is_same_v<HANDLE, VkDeviceMemory>) {
        return VK_OBJECT_TYPE_DEVICE_MEMORY;
    } else if constexpr (std::is_same_v<HANDLE, VkFence>) {
        return VK_OBJECT_TYPE_FENCE;
    } else if constexpr (std::is_same_v<HANDLE, VkFramebuffer>) {
        return VK_OBJECT_TYPE_FRAMEBUFFER;
    } else if constexpr (std::is_same_v<HANDLE, VkImage>) {
        return VK_OBJECT_TYPE_IMAGE;
    } else if constexpr (std::is_same_v<HANDLE, VkImageView>) {
        return VK_OBJECT_TYPE_IMAGE_VIEW;
    } else if constexpr (std::is_same_v<HANDLE, VkPipeline>) {
        return VK_OBJECT_TYPE_PIPELINE;
    } else if constexpr (std::is_same_v<HANDLE, VkPipelineLayout>) {
        return VK_OBJECT_TYPE_PIPELINE_LAYOUT;
    } else if constexpr (std::is_same_v<HANDLE, VkRenderPass>) {
        return VK_OBJECT_TYPE_RENDER_PASS;
    } else if constexpr (std::is_same_v<HANDLE, VkSemaphore>) {
        return VK_OBJECT_TYPE_SEMAPHORE;
    } else if constexpr (std::is_same_v<HANDLE, VkShaderModule>) {
        return VK_OBJECT_TYPE_SHADER_MODULE;
    } else if constexpr (std::is_same_v<HANDLE, VkSurfaceKHR>) {
        return VK_OBJECT_TYPE_SURFACE_KHR;
    } else if constexpr (std::is_same_v<HANDLE, VkSwapchainKHR>) {
        return VK_OBJECT_TYPE_SWAPCHAIN_KHR;
    } else if constexpr (std::is_same_v<HANDLE, VkSampler>) {
        return VK_OBJECT_TYPE_SAMPLER;
    } else if constexpr (std::is_same_v<HANDLE, VkQueryPool>) {
        return VK_OBJECT_TYPE_QUERY_POOL;
    } else if constexpr (std::is_same_v<HANDLE, VkAccelerationStructureKHR>) {
        return VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
    } else {
        static_assert(AlwaysFalse<HANDLE>, "unsupported handle type.");
        return VK_OBJECT_TYPE_UNKNOWN;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
/// A little struct to hold all essential Vuklan global objects. This is to make it easier to reference key Vulkan
/// information across the library.
struct VulkanGlobalInfo {
    const VkAllocationCallbacks * allocator    = nullptr;
    VkInstance                    instance     = nullptr;
    VkPhysicalDevice              phydev       = nullptr;
    VkDevice                      device       = nullptr;
    VmaAllocator                  vmaAllocator = nullptr;

    VulkanGlobalInfo(const VkAllocationCallbacks * a = nullptr): allocator(a) {}

    ~VulkanGlobalInfo() = default;

    bool operator==(const VulkanGlobalInfo & rhs) const {
        return allocator == rhs.allocator && instance == rhs.instance && phydev == rhs.phydev && device == rhs.device;
    }

    bool operator!=(const VulkanGlobalInfo & rhs) const { return !operator==(rhs); }

    template<typename HANDLE>
    void safeDestroy(HANDLE & h, VmaAllocation & a = DUMMPY_ALLOCATION) const {
        if (!h) {
            return;
            // Note: there will be lots of if branches here. Try keep it sorted by then handle type.
        } else if constexpr (std::is_same_v<HANDLE, uint64_t>) {
            static_assert(AlwaysFalse<HANDLE>, "Can't use built-in integer as handle type. If you are on 32-bit platform, "
                                               "please override VK_DEFINE_NON_DISPATCHABLE_HANDLE to give each Vulkan handle an unique type.");
        } else if constexpr (std::is_same_v<HANDLE, VkBuffer>) {
            if (a) {
                PH_REQUIRE(vmaAllocator);
                vmaDestroyBuffer(vmaAllocator, h, a);
            } else {
                vkDestroyBuffer(device, h, allocator);
            }
        } else if constexpr (std::is_same_v<HANDLE, VkCommandPool>) {
            vkDestroyCommandPool(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkDescriptorPool>) {
            vkDestroyDescriptorPool(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkDescriptorSetLayout>) {
            vkDestroyDescriptorSetLayout(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkDeviceMemory>) {
            vkFreeMemory(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkFence>) {
            vkDestroyFence(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkFramebuffer>) {
            vkDestroyFramebuffer(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkImage>) {
            if (a) {
                PH_REQUIRE(vmaAllocator);
                vmaDestroyImage(vmaAllocator, h, a);
            } else {
                vkDestroyImage(device, h, allocator);
            }
        } else if constexpr (std::is_same_v<HANDLE, VkImageView>) {
            vkDestroyImageView(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkPipeline>) {
            vkDestroyPipeline(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkPipelineLayout>) {
            vkDestroyPipelineLayout(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkRenderPass>) {
            vkDestroyRenderPass(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkSemaphore>) {
            vkDestroySemaphore(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkShaderModule>) {
            vkDestroyShaderModule(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkSurfaceKHR>) {
            vkDestroySurfaceKHR(instance, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkSwapchainKHR>) {
            vkDestroySwapchainKHR(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkSampler>) {
            vkDestroySampler(device, h, allocator);
        } else if constexpr (std::is_same_v<HANDLE, VkQueryPool>) {
            vkDestroyQueryPool(device, h, allocator);
        } else {
            static_assert(AlwaysFalse<HANDLE>, "unsupported handle type.");
        }
        h = 0;
        a = 0;
    }

private:
    static inline VmaAllocation DUMMPY_ALLOCATION = 0;
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
PH_API void   deaallocateAutoHandle(void *, size_t);
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

    operator T() const { return _handle; }

    operator bool() const { return !!_handle; }

    T get() const { return _handle; }

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
    /// \param deletor This can be either a reference to VulkanGlobalInfo object, or a function that accepts
    ///               calling signature of: "void (T)". It'll be used delete the underlying handle.
    ///
    /// Here's an example of how it should be used:
    ///
    ///     AutoHandle<VkCommandPool> pool;
    ///     VkCreateCommandPool(device, &ci, allocator, pool.prepare([](auto h){ vkDestroyCommandPool(h); }));
    ///
    template<typename Deletor>
    T * prepare(Deletor deletor) {
        clear(); // release old handle.
        PH_ASSERT(0 == _handle && 0 == _ref);
        _ref = new RefCounter();
        if constexpr (std::is_same_v<Deletor, VulkanGlobalInfo> || std::is_same_v<Deletor, const VulkanGlobalInfo &> ||
                      std::is_same_v<Deletor, VulkanGlobalInfo &>) {
            _ref->d = [&](auto & h) { deletor.safeDestroy(h); };
        } else {
            _ref->d = deletor;
        }
        return &_handle;
    }

    /// Clear any preexisting handles. Return a pointer that is ready to pass to
    /// Vulkan's create function for creating a new handle.
    T * prepare(const VulkanGlobalInfo & vgi) {
        return prepare([&vgi](auto & h) { vgi.safeDestroy(h); });
    }

private:
    // becuase & operator is overloaded. So we need alternative way to get the address of the class.
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
        static void   operator delete(void * ptr, size_t size) { return deaallocateAutoHandle(ptr, size); }
    };

    T            _handle = 0;
    RefCounter * _ref    = 0;
};

} // namespace ph::va

// Do _not_ let clang-format reorder these headers
// clang-format off
#include "va/debug.inl"
#include "va/command.inl"
#include "va/memory.inl"
#include "va/shader.inl"
#include "va/buffer.inl"
#include "va/image.inl"
#include "va/fatmesh.inl"
#include "va/initializers.inl"
#include "va/info.inl"
#include "va/misc.inl"
#include "va/device.inl"
#include "va/swapchain.inl"
#include "va/render-loop.inl"
// clang-format on