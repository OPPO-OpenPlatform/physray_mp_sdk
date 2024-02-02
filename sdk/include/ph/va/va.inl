/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

namespace ph {
namespace va {

// template<>
// inline VkObjectType getHandleObjectType<uint64_t>() {
//     static_assert(AlwaysFalse<uint64_t>, "Can't use built-in integer as handle type. If you are on 32-bit platform, "
//                                          "please override VK_DEFINE_NON_DISPATCHABLE_HANDLE to give each Vulkan handle an unique type.");
//     return VK_OBJECT_TYPE_UNKNOWN;
// }

template<>
inline VkObjectType getHandleObjectType<VkBuffer>() {
    return VK_OBJECT_TYPE_BUFFER;
}

template<>
inline VkObjectType getHandleObjectType<VkCommandPool>() {
    return VK_OBJECT_TYPE_COMMAND_POOL;
}

template<>
inline VkObjectType getHandleObjectType<VkCommandBuffer>() {
    return VK_OBJECT_TYPE_COMMAND_BUFFER;
}

template<>
inline VkObjectType getHandleObjectType<VkDescriptorPool>() {
    return VK_OBJECT_TYPE_DESCRIPTOR_POOL;
}

template<>
inline VkObjectType getHandleObjectType<VkDescriptorSetLayout>() {
    return VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
}

template<>
inline VkObjectType getHandleObjectType<VkDescriptorSet>() {
    return VK_OBJECT_TYPE_DESCRIPTOR_SET;
}

template<>
inline VkObjectType getHandleObjectType<VkDeviceMemory>() {
    return VK_OBJECT_TYPE_DEVICE_MEMORY;
}

template<>
inline VkObjectType getHandleObjectType<VkFence>() {
    return VK_OBJECT_TYPE_FENCE;
}

template<>
inline VkObjectType getHandleObjectType<VkFramebuffer>() {
    return VK_OBJECT_TYPE_FRAMEBUFFER;
}

template<>
inline VkObjectType getHandleObjectType<VkImage>() {
    return VK_OBJECT_TYPE_IMAGE;
}

template<>
inline VkObjectType getHandleObjectType<VkImageView>() {
    return VK_OBJECT_TYPE_IMAGE_VIEW;
}

template<>
inline VkObjectType getHandleObjectType<VkPipeline>() {
    return VK_OBJECT_TYPE_PIPELINE;
}

template<>
inline VkObjectType getHandleObjectType<VkPipelineLayout>() {
    return VK_OBJECT_TYPE_PIPELINE_LAYOUT;
}

template<>
inline VkObjectType getHandleObjectType<VkRenderPass>() {
    return VK_OBJECT_TYPE_RENDER_PASS;
}

template<>
inline VkObjectType getHandleObjectType<VkSemaphore>() {
    return VK_OBJECT_TYPE_SEMAPHORE;
}

template<>
inline VkObjectType getHandleObjectType<VkShaderModule>() {
    return VK_OBJECT_TYPE_SHADER_MODULE;
}

template<>
inline VkObjectType getHandleObjectType<VkSurfaceKHR>() {
    return VK_OBJECT_TYPE_SURFACE_KHR;
}

template<>
inline VkObjectType getHandleObjectType<VkSwapchainKHR>() {
    return VK_OBJECT_TYPE_SWAPCHAIN_KHR;
}

template<>
inline VkObjectType getHandleObjectType<VkSampler>() {
    return VK_OBJECT_TYPE_SAMPLER;
}

template<>
inline VkObjectType getHandleObjectType<VkQueryPool>() {
    return VK_OBJECT_TYPE_QUERY_POOL;
}

template<>
inline VkObjectType getHandleObjectType<VkAccelerationStructureKHR>() {
    return VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkBuffer>(VkBuffer & h, VmaAllocation & a) const {
    if (!h) return;
    if (a) {
        PH_REQUIRE(vmaAllocator);
        vmaDestroyBuffer(vmaAllocator, h, a);
    } else {
        vkDestroyBuffer(device, h, allocator);
    }
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkCommandPool>(VkCommandPool & h, VmaAllocation & a) const {
    if (!h) return;
    vkResetCommandPool(device, h, 0);
    vkDestroyCommandPool(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkDescriptorPool>(VkDescriptorPool & h, VmaAllocation & a) const {
    if (!h) return;
    vkResetDescriptorPool(device, h, 0);
    vkDestroyDescriptorPool(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkDescriptorSetLayout>(VkDescriptorSetLayout & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyDescriptorSetLayout(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkDeviceMemory>(VkDeviceMemory & h, VmaAllocation & a) const {
    if (!h) return;
    vkFreeMemory(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkFence>(VkFence & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyFence(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkFramebuffer>(VkFramebuffer & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyFramebuffer(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkImage>(VkImage & h, VmaAllocation & a) const {
    if (!h) return;
    if (a) {
        PH_REQUIRE(vmaAllocator);
        vmaDestroyImage(vmaAllocator, h, a);
    } else {
        vkDestroyImage(device, h, allocator);
    }
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkImageView>(VkImageView & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyImageView(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkPipeline>(VkPipeline & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyPipeline(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkPipelineLayout>(VkPipelineLayout & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyPipelineLayout(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkRenderPass>(VkRenderPass & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyRenderPass(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkSemaphore>(VkSemaphore & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroySemaphore(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkShaderModule>(VkShaderModule & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyShaderModule(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkSurfaceKHR>(VkSurfaceKHR & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroySurfaceKHR(instance, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkSwapchainKHR>(VkSwapchainKHR & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroySwapchainKHR(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkSampler>(VkSampler & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroySampler(device, h, allocator);
    h = 0;
    a = 0;
}

template<>
inline void VulkanGlobalInfo::safeDestroy<VkQueryPool>(VkQueryPool & h, VmaAllocation & a) const {
    if (!h) return;
    vkDestroyQueryPool(device, h, allocator);
    h = 0;
    a = 0;
}

} // namespace va
} // namespace ph