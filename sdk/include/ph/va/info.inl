/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

#include <vector>
#include <sstream>
#include <algorithm>

namespace ph {
namespace va {
// ---------------------------------------------------------------------------------------------------------------------
/// Utility function to enumerate vulkan item/feature/extension list.
template<typename T, typename Q>
inline std::vector<T> completeEnumerate(Q query) {
    // It's possible, though very rare, that the number of items
    // could change. For example, installing something
    // could include new layers that the loader would pick up
    // between the initial query for the count and the
    // request for VkLayerProperties. The loader indicates that
    // by returning a VK_INCOMPLETE status and will update the
    // the count parameter.
    // The count parameter will be updated with the number of
    // entries loaded into the data pointer - in case the number
    // of layers went down or is smaller than the size given.
    std::vector<T> result;
    // if constexpr (std::is_same_v<void, std::result_of_t<Q>>) {
    //     uint32_t count;
    //     query(&count, nullptr);
    //     result.resize(count);
    //     query(&count, result.data());
    // } else {
    VkResult res;
    do {
        uint32_t count;
        PH_VA_CHK(query(&count, nullptr), return {});
        if (0 == count) return {};
        result.resize(count);
        res = query(&count, result.data());
    } while (res == VK_INCOMPLETE);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline std::vector<VkPhysicalDevice> enumeratePhysicalDevices(VkInstance instance) {
    return completeEnumerate<VkPhysicalDevice>([&](uint32_t * count, VkPhysicalDevice * data) { return vkEnumeratePhysicalDevices(instance, count, data); });
}

// ---------------------------------------------------------------------------------------------------------------------
// This function currently selects the devie with the longest extension list.
VkPhysicalDevice selectTheMostPowerfulPhysicalDevice(const ConstRange<VkPhysicalDevice> & phydevs);

// ---------------------------------------------------------------------------------------------------------------------
//
inline std::vector<VkExtensionProperties> enumerateDeviceExtenstions(VkPhysicalDevice dev) {
    auto extensions = completeEnumerate<VkExtensionProperties>(
        [&](uint32_t * count, VkExtensionProperties * data) { return vkEnumerateDeviceExtensionProperties(dev, nullptr, count, data); });
    std::sort(extensions.begin(), extensions.end(), [](const auto & a, const auto & b) -> bool { return strcmp(a.extensionName, b.extensionName) < 0; });
    return extensions;
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline std::string printVulkanVersion(uint32_t v) {
    std::stringstream ss;
    ss << "v" << VK_VERSION_MAJOR(v) << "." << VK_VERSION_MINOR(v) << "." << VK_VERSION_PATCH(v);
    return ss.str();
};

} // namespace va
} // namespace ph