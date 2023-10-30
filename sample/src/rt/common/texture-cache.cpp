/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "texture-cache.h"

// Image returned when image does not exist.
static const ph::RawImage EMPTY_IMAGE;

TextureCache::TextureCache(ph::va::VulkanSubmissionProxy * vsp, ph::AssetSystem * assetSystem, const VkFormat defaultShadowMapFormat,
                           uint32_t defaultShadowMapSize)
    : _vsp(vsp), _assetSystem(assetSystem), _defaultShadowMapFormat(defaultShadowMapFormat), _defaultShadowMapSize(defaultShadowMapSize) {}

ph::rt::Material::TextureHandle TextureCache::loadFromAsset(const std::string & assetPath, VkImageUsageFlagBits usage) {
    // If no resource was selected.
    if (assetPath.empty()) {
        // Return an empty texture handle.
        return {};
    }

    // Check the cache and see if this asset is already loaded.
    auto iterator = _textureHandles.find(assetPath);

    // If this asset is already loaded.
    if (iterator != _textureHandles.end()) {
        // Return the entry for it.
        return ph::rt::toTextureHandle(iterator->second);
    }

    // Load the image bytes from the asset system.
    std::future<ph::Asset> futureAsset = _assetSystem->load(assetPath.c_str());
    ph::Asset              asset       = futureAsset.get();

    if (asset.content.i.empty()) {
        // Record what went wrong.
        PH_LOGE("Failed to load image file %s", assetPath.c_str());

        // Pass an empty texture handle back to user.
        return {};
    }

    // Create a texture out of the image.
    // Save image to the handle mapping.
    ph::va::ImageObject & imageObject = _textureHandles[assetPath];
    imageObject.createFromImageProxy(assetPath.c_str(), *_vsp, usage, ph::va::DeviceMemoryUsage::GPU_ONLY, asset.content.i.proxy());

    // done
    return ph::rt::toTextureHandle(imageObject);
}

std::string TextureCache::getAssetPath(const ph::rt::Material::TextureHandle & textureHandle) {
    auto iterator = _textureHandles.begin();
    while (iterator != _textureHandles.end()) {
        if (ph::rt::toTextureHandle(iterator->second) == textureHandle) return iterator->first;
    }
    return "";
}

ph::rt::Material::TextureHandle TextureCache::createFromImageProxy(const ph::ImageProxy & imageProxy) {
    // If the image is empty.
    if (imageProxy.empty()) {
        // Return an empty texture handle.
        return {};
    }

    // allocate a new image object
    _imageProxyHandles.emplace_back();
    ph::va::ImageObject & imageObject = _imageProxyHandles.back();

    // load from image proxy
    imageObject.createFromImageProxy("image proxy", *_vsp, VK_IMAGE_USAGE_SAMPLED_BIT, ph::va::DeviceMemoryUsage::GPU_ONLY, imageProxy);

    return ph::rt::toTextureHandle(imageObject);
}

ph::rt::Material::TextureHandle TextureCache::createFromImageProxy(const ph::ImageProxy & imageProxy, std::string imageAssetPath) {

    if (imageAssetPath.empty())
        return createFromImageProxy(imageProxy);
    else {
        auto itr = _textureHandles.find(imageAssetPath);
        if (itr != _textureHandles.end())
            return ph::rt::toTextureHandle(itr->second);
        else {
            // If the image is empty.
            if (imageProxy.empty()) {
                // Return an empty texture handle.
                return {};
            }

            ph::va::ImageObject & imageObject = _textureHandles[imageAssetPath];
            // load from image proxy
            imageObject.createFromImageProxy("image proxy", *_vsp, VK_IMAGE_USAGE_SAMPLED_BIT, ph::va::DeviceMemoryUsage::GPU_ONLY, imageProxy);

            return ph::rt::toTextureHandle(imageObject);
        }
    }
}

ph::rt::Material::TextureHandle TextureCache::createShadowMap2D(const char * name, VkFormat format, uint32_t size) {
    // allocate a new image object
    _imageProxyHandles.emplace_back();
    ph::va::ImageObject & shadowMap = _imageProxyHandles.back();

    uint32_t                 mipCount         = 1; // TODO: create more mips for cascade shadow
    uint32_t                 baseMip          = 0;
    uint32_t                 baseArrayLayer   = 0;
    uint32_t                 layerCount       = 1;
    uint32_t                 depth            = 1;
    VkImageSubresourceRange  subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, baseMip, mipCount, baseArrayLayer, layerCount};
    VkImageSubresourceLayers subresourceLayer = {VK_IMAGE_ASPECT_COLOR_BIT, baseMip, baseArrayLayer, layerCount};

    VkImageUsageFlags flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (mipCount > 1) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // To blit to all mips

    // create a shadow map texture
    shadowMap.create(name, _vsp->vgi(), ph::va::ImageObject::CreateInfo {}.set2D(size, size).setFormat(format).setUsage(flags).setLevels(mipCount));

    // clear shadow map to FLT_MAX
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(_vsp->vgi().device, shadowMap.image, &requirements);
    ph::va::BufferObjectT<VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ph::va::DeviceMemoryUsage::CPU_ONLY> sb;
    sb.allocate(_vsp->vgi(), requirements.size, "Shadow Map Staging Buffer");
    auto shadowMapPixels = sb.map<uint8_t>();
    for (size_t i = 0; i < requirements.size / sizeof(float); ++i) {
        auto smp = reinterpret_cast<float *>(shadowMapPixels.range.data());
        smp[i]   = std::numeric_limits<float>::max();
    }
    shadowMapPixels.unmap();
    ph::va::SingleUseCommandPool cmdpool(*_vsp);
    auto                         cb               = cmdpool.create();
    VkBufferImageCopy            bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource             = subresourceLayer;
    bufferCopyRegion.imageExtent.width            = size;
    bufferCopyRegion.imageExtent.height           = size;
    bufferCopyRegion.imageExtent.depth            = depth;
    bufferCopyRegion.bufferOffset                 = 0;
    ph::va::setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    vkCmdCopyBufferToImage(cb, sb.buffer, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

    // Populate mips
    subresourceRange.levelCount     = 1;  // Doing things one mip at a time
    VkImageMemoryBarrier mipBarrier = {}; // Reuse barrier when generating mips
    mipBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    mipBarrier.image                = shadowMap.image;
    mipBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    mipBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    mipBarrier.subresourceRange     = subresourceRange;
    int32_t mipSize                 = (int32_t) size;
    for (uint32_t i = 1; i < mipCount; i++) {
        // Transition src mip to SRC_OPTIMAL
        subresourceRange.baseMipLevel = i - 1;
        ph::va::setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

        // Blit from src mip to dst mip
        subresourceLayer.mipLevel = i - 1;
        VkImageBlit blit {};
        blit.srcOffsets[0]           = {0, 0, 0};
        blit.srcOffsets[1]           = {mipSize, mipSize, 1};
        blit.srcSubresource          = subresourceLayer;
        blit.dstOffsets[0]           = {0, 0, 0};
        blit.dstOffsets[1]           = {mipSize > 1 ? mipSize / 2 : 1, mipSize > 1 ? mipSize / 2 : 1, 1};
        blit.dstSubresource          = subresourceLayer;
        blit.dstSubresource.mipLevel = i;

        vkCmdBlitImage(cb, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                       VK_FILTER_LINEAR);

        // Transition src mip to READ_ONLY_OPTIMAL
        ph::va::setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

        if (mipSize > 1) mipSize /= 2;
    }

    // Transition last mip
    subresourceRange.baseMipLevel = mipCount - 1;
    ph::va::setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

    cmdpool.finish(cb);

    return ph::rt::toTextureHandle(shadowMap);
}

ph::rt::Material::TextureHandle TextureCache::createShadowMap2D(const char * name) {
    return createShadowMap2D(name, _defaultShadowMapFormat, _defaultShadowMapSize);
}

ph::rt::Material::TextureHandle TextureCache::createShadowMapCube(const char * name, VkFormat format, uint32_t size) {
    // allocate a new image object
    _imageProxyHandles.emplace_back();
    ph::va::ImageObject & shadowMap = _imageProxyHandles.back();

    // create a shadow map texture
    shadowMap.create(name, _vsp->vgi(),
                     ph::va::ImageObject::CreateInfo {}
                         .set2D(size, size)
                         .setLayers(6)
                         .setFormat(format)
                         .setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

    // clear shadow map to FLT_MAX
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(_vsp->vgi().device, shadowMap.image, &requirements);
    ph::va::BufferObjectT<VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ph::va::DeviceMemoryUsage::CPU_ONLY> sb;
    sb.allocate(_vsp->vgi(), requirements.size, "Shadow Map Staging Buffer");
    auto shadowMapPixels = sb.map<uint8_t>();
    for (size_t i = 0; i < requirements.size / sizeof(float); ++i) {
        auto smp = reinterpret_cast<float *>(shadowMapPixels.range.data());
        smp[i]   = std::numeric_limits<float>::max();
    }
    shadowMapPixels.unmap();
    ph::va::SingleUseCommandPool cmdpool(*_vsp);
    auto                         cb                  = cmdpool.create();
    VkBufferImageCopy            bufferCopyRegion    = {};
    bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel       = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount     = 6;
    bufferCopyRegion.imageExtent.width               = size;
    bufferCopyRegion.imageExtent.height              = size;
    bufferCopyRegion.imageExtent.depth               = 1;
    bufferCopyRegion.bufferOffset                    = 0;
    ph::va::setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6});
    vkCmdCopyBufferToImage(cb, sb.buffer, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
    ph::va::setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6});

    cmdpool.finish(cb);

    return ph::rt::toTextureHandle(shadowMap);
}

ph::rt::Material::TextureHandle TextureCache::createShadowMapCube(const char * name) {
    return createShadowMapCube(name, _defaultShadowMapFormat, _defaultShadowMapSize);
}
