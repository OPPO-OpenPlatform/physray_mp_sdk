// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.
#include <unordered_map>

namespace ph::va {

// ---------------------------------------------------------------------------------------------------------------------
enum class DeviceMemoryUsage : uint32_t {
    GPU_ONLY   = 1,
    CPU_ONLY   = 2,
    CPU_TO_GPU = 3,
};

// ---------------------------------------------------------------------------------------------------------------------
//
inline constexpr VkMemoryPropertyFlags toVkMemoryPropertyFlags(DeviceMemoryUsage u) {
    switch (u) {
    case DeviceMemoryUsage::GPU_ONLY:
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    case DeviceMemoryUsage::CPU_ONLY:
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    case DeviceMemoryUsage::CPU_TO_GPU:
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    default:
        PH_LOGE("DeviceMemoryUsage type not found!");
        return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
PH_API AutoHandle<VkDeviceMemory> allocateDeviceMemory(const VulkanGlobalInfo & g, const VkMemoryRequirements & memRequirements, DeviceMemoryUsage memoryUsage,
                                                       const VkMemoryAllocateFlags allocFlags = 0);

// ---------------------------------------------------------------------------------------------------------------------
//
inline AutoHandle<VkDeviceMemory> allocateDeviceMemory(const VulkanGlobalInfo & g, const VkMemoryRequirements2 & memRequirements, DeviceMemoryUsage memoryUsage,
                                                       const VkMemoryAllocateFlags allocFlags = 0) {
    return allocateDeviceMemory(g, memRequirements.memoryRequirements, memoryUsage, allocFlags);
}

// ---------------------------------------------------------------------------------------------------------------------
/// Enable/Disable device memory allocation tracking.
/// This is a very time consuming process, thus should only be enabled when you are debugging possible memory leak.
enum class DeviceMemoryTrackLevel {
    DISABLED  = 0, // tracking is disabled. this is the default mode.
    COUNT     = 1, // tracking allocation count.
    CALLSTACK = 2, // tracking allocation count and callstack. this mode is extremely slow.
};
PH_API void trackDeviceMemoryAllocation(DeviceMemoryTrackLevel level);

// ---------------------------------------------------------------------------------------------------------------------
//
PH_API std::unordered_map<VkDeviceMemory, std::string> getDeviceMemoryAllocationInfo();

} // namespace ph::va

namespace std {

template<>
struct hash<VkDeviceMemory> {
    size_t operator()(VkDeviceMemory memory) const { return hash<uint64_t>()((uint64_t) memory); }
};

} // namespace std
