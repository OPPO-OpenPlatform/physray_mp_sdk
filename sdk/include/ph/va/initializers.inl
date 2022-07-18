#pragma once

// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

// TODO: move back to ph::va namespace.
namespace ph::va::util {

inline VkMemoryAllocateInfo memoryAllocateInfo() {
    VkMemoryAllocateInfo memAllocInfo {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    return memAllocInfo;
}

inline VkRenderPassBeginInfo renderPassBeginInfo(VkRenderPass renderPass, VkFramebuffer frameBuffer, const VkRect2D & renderArea, uint32_t clearValueCount,
                                                 const VkClearValue * pClearValues) {
    VkRenderPassBeginInfo renderPassBeginInfo {};
    renderPassBeginInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext           = nullptr;
    renderPassBeginInfo.renderPass      = renderPass;
    renderPassBeginInfo.framebuffer     = frameBuffer;
    renderPassBeginInfo.renderArea      = renderArea;
    renderPassBeginInfo.clearValueCount = clearValueCount;
    renderPassBeginInfo.pClearValues    = pClearValues;
    return renderPassBeginInfo;
}

inline VkFramebufferCreateInfo framebufferCreateInfo(VkRenderPass renderPass, size_t attachmentCount, const VkImageView * pAttachments, size_t width,
                                                     size_t height, size_t layers = 1) {
    VkFramebufferCreateInfo framebufferCreateInfo {};
    framebufferCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.pNext           = nullptr;
    framebufferCreateInfo.flags           = 0;
    framebufferCreateInfo.renderPass      = renderPass;
    framebufferCreateInfo.attachmentCount = (uint32_t) attachmentCount;
    framebufferCreateInfo.pAttachments    = pAttachments;
    framebufferCreateInfo.width           = (uint32_t) width;
    framebufferCreateInfo.height          = (uint32_t) height;
    framebufferCreateInfo.layers          = (uint32_t) layers;
    return framebufferCreateInfo;
}

inline VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t count = 1) {

    VkDescriptorSetLayoutBinding descSetLayoutBinding {};
    descSetLayoutBinding.descriptorType  = type;
    descSetLayoutBinding.stageFlags      = stageFlags;
    descSetLayoutBinding.binding         = binding;
    descSetLayoutBinding.descriptorCount = count;
    return descSetLayoutBinding;
}

inline VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(const ConstRange<VkDescriptorSetLayoutBinding> & bindings) {
    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo {};
    setLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutCreateInfo.pBindings    = bindings.data();
    setLayoutCreateInfo.bindingCount = (uint32_t) bindings.size();
    return setLayoutCreateInfo;
}

inline VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(const VkDescriptorSetLayout * pSetLayouts, uint32_t setLayoutCount = 1) {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
    pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pSetLayouts    = pSetLayouts;
    pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
    return pipelineLayoutCreateInfo;
}

inline VkDescriptorPoolSize descriptorPoolSize(VkDescriptorType type, uint32_t descriptorCount) {
    VkDescriptorPoolSize descriptorPoolSize {};
    descriptorPoolSize.type            = type;
    descriptorPoolSize.descriptorCount = descriptorCount;
    return descriptorPoolSize;
}

inline VkDescriptorPoolCreateInfo descriptorPoolCreateInfo(const ConstRange<VkDescriptorPoolSize> & poolSizes, uint32_t maxSets) {
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo {};
    descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes    = poolSizes.data();
    descriptorPoolCreateInfo.maxSets       = maxSets;
    descriptorPoolCreateInfo.flags         = 0;
    return descriptorPoolCreateInfo;
}

inline VkDescriptorSetAllocateInfo descriptorSetAllocateInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout * pSetLayouts,
                                                             uint32_t descriptorSetCount) {
    VkDescriptorSetAllocateInfo descSetAllocInfo {};
    descSetAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAllocInfo.descriptorPool     = descriptorPool;
    descSetAllocInfo.pSetLayouts        = pSetLayouts;
    descSetAllocInfo.descriptorSetCount = descriptorSetCount;
    return descSetAllocInfo;
}

inline VkWriteDescriptorSet writeDescriptorSet(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorBufferInfo * pBufferInfo,
                                               uint32_t count = 1) {
    VkWriteDescriptorSet writeDescSet {};
    writeDescSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescSet.dstSet          = dstSet;
    writeDescSet.descriptorType  = type;
    writeDescSet.dstBinding      = binding;
    writeDescSet.pBufferInfo     = pBufferInfo;
    writeDescSet.descriptorCount = count;
    return writeDescSet;
}

inline VkWriteDescriptorSet writeDescriptorSet(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorImageInfo * pImageInfo,
                                               uint32_t count = 1) {

    VkWriteDescriptorSet writeDescSet {};
    writeDescSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescSet.dstSet          = dstSet;
    writeDescSet.descriptorType  = type;
    writeDescSet.dstBinding      = binding;
    writeDescSet.pImageInfo      = pImageInfo;
    writeDescSet.descriptorCount = count;
    return writeDescSet;
}

inline VkCommandBufferBeginInfo commandBufferBeginInfo() {
    VkCommandBufferBeginInfo commandBufferBeginInfo {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    return commandBufferBeginInfo;
}

inline VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool cmdPool, VkCommandBufferLevel level, uint32_t count) {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
    commandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool        = cmdPool;
    commandBufferAllocateInfo.level              = level;
    commandBufferAllocateInfo.commandBufferCount = count;
    return commandBufferAllocateInfo;
}

inline VkViewport viewport(float width, float height, float minDepth, float maxDepth) {
    VkViewport viewport {};
    viewport.width    = width;
    viewport.height   = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

inline VkRect2D rect2d(int32_t width, int32_t height, int32_t offsetX, int32_t offsetY) {
    VkRect2D rect2D {};
    rect2D.extent.width  = width;
    rect2D.extent.height = height;
    rect2D.offset.x      = offsetX;
    rect2D.offset.y      = offsetY;
    return rect2D;
}

inline VkSubmitInfo submitInfo() {
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    return submitInfo;
}

inline VkDescriptorImageInfo descriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout) {
    VkDescriptorImageInfo descImageInfo = {};
    descImageInfo.sampler               = sampler;
    descImageInfo.imageView             = imageView;
    descImageInfo.imageLayout           = imageLayout;
    return descImageInfo;
}

inline VkSamplerCreateInfo samplerCreateInfo() {
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.maxAnisotropy       = 1.0f;
    return samplerCreateInfo;
}

inline VkMemoryBarrier memoryBarrier(VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkMemoryBarrier barrier = {};
    barrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask   = srcAccess;
    barrier.dstAccessMask   = dstAccess;
    return barrier;
}

inline VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask, uint32_t baseMipLevel = 0, uint32_t levelCount = 1,
                                                     uint32_t baseArrayLayer = 0, uint32_t layerCount = 1) {
    VkImageSubresourceRange range = {};
    range.aspectMask              = aspectMask;
    range.baseMipLevel            = baseMipLevel;
    range.levelCount              = levelCount;
    range.baseArrayLayer          = baseArrayLayer;
    range.layerCount              = layerCount;
    return range;
}

inline VkImageMemoryBarrier imageMemoryBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout,
                                               VkImage image, VkImageSubresourceRange subresourceRange, uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                               uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) {
    VkImageMemoryBarrier imgMemBarrier = {};
    imgMemBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgMemBarrier.srcAccessMask        = srcAccessMask;
    imgMemBarrier.dstAccessMask        = dstAccessMask;
    imgMemBarrier.oldLayout            = oldLayout;
    imgMemBarrier.newLayout            = newLayout;
    imgMemBarrier.srcQueueFamilyIndex  = srcQueueFamilyIndex;
    imgMemBarrier.dstQueueFamilyIndex  = dstQueueFamilyIndex;
    imgMemBarrier.image                = image;
    imgMemBarrier.subresourceRange     = subresourceRange;
    return imgMemBarrier;
}

inline VkPipelineShaderStageCreateInfo shaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits stage, const char * pName = "main") {
    VkPipelineShaderStageCreateInfo stageCreateInfo = {};
    stageCreateInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.flags                           = {};
    stageCreateInfo.module                          = module;
    stageCreateInfo.pName                           = pName;
    stageCreateInfo.stage                           = stage;
    return stageCreateInfo;
}

inline VkGraphicsPipelineCreateInfo
graphicsPipelineCreateInfo(uint32_t stageCount, const VkPipelineShaderStageCreateInfo * pStages, VkPipelineLayout layout, VkRenderPass renderPass,
                           uint32_t subpass, const VkPipelineVertexInputStateCreateInfo * pVertexInputState,
                           const VkPipelineInputAssemblyStateCreateInfo * pInputAssemblyState, const VkPipelineViewportStateCreateInfo * pViewportState,
                           const VkPipelineRasterizationStateCreateInfo * pRasterizationState, const VkPipelineMultisampleStateCreateInfo * pMultisampleState,
                           const VkPipelineDepthStencilStateCreateInfo * pDepthStencilState, const VkPipelineColorBlendStateCreateInfo * pColorBlendState,
                           const VkPipelineDynamicStateCreateInfo *      pDynamicState      = nullptr,
                           const VkPipelineTessellationStateCreateInfo * pTessellationState = nullptr) {
    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.stageCount                   = stageCount;
    createInfo.pStages                      = pStages;
    createInfo.layout                       = layout;
    createInfo.renderPass                   = renderPass;
    createInfo.subpass                      = subpass;
    createInfo.pVertexInputState            = pVertexInputState;
    createInfo.pInputAssemblyState          = pInputAssemblyState;
    createInfo.pViewportState               = pViewportState;
    createInfo.pTessellationState           = pTessellationState;
    createInfo.pRasterizationState          = pRasterizationState;
    createInfo.pMultisampleState            = pMultisampleState;
    createInfo.pDepthStencilState           = pDepthStencilState;
    createInfo.pColorBlendState             = pColorBlendState;
    createInfo.pDynamicState                = pDynamicState;
    return createInfo;
};

} // namespace ph::va::util