/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph {
namespace va {

struct VulkanSubmissionProxy;

// -----------------------------------------------------------------------------
/// Helper class of Vulkan Image
struct ImageObject {
    struct CreateInfo : public VkImageCreateInfo {
        VkMemoryPropertyFlags memory = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        /// Default is 0. Means determine the aspect automatically based on format and usage.
        VkImageAspectFlags aspect = 0;

        CreateInfo() {
            sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            pNext                 = nullptr;
            flags                 = 0;
            imageType             = VK_IMAGE_TYPE_2D;
            format                = VK_FORMAT_R8G8B8A8_UNORM;
            extent                = {1, 1, 1};
            mipLevels             = 1;
            arrayLayers           = 1;
            samples               = VK_SAMPLE_COUNT_1_BIT;
            tiling                = VK_IMAGE_TILING_OPTIMAL;
            usage                 = VK_IMAGE_USAGE_SAMPLED_BIT;
            sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
            queueFamilyIndexCount = 0;
            pQueueFamilyIndices   = nullptr;
            initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        CreateInfo & fromVk(const VkImageCreateInfo & ci) {
            *(VkImageCreateInfo *) this = ci;
            return *this;
        }

        CreateInfo & fromImageDesc(const ImageDesc & desc);

        CreateInfo & set2D(size_t w, size_t h) {
            flags         = 0;
            imageType     = VK_IMAGE_TYPE_2D;
            extent.width  = (uint32_t) w;
            extent.height = (uint32_t) h;
            extent.depth  = 1;
            arrayLayers   = 1;
            PH_ASSERT(!isCube());
            return *this;
        }

        CreateInfo & setCube(size_t w) {
            imageType     = VK_IMAGE_TYPE_2D;
            extent.width  = (uint32_t) w;
            extent.height = (uint32_t) w;
            extent.depth  = 1;
            arrayLayers   = 6;
            flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            PH_ASSERT(isCube());
            return *this;
        }

        CreateInfo & setFormat(VkFormat f) {
            format = f;
            return *this;
        }
        CreateInfo & setLevels(size_t l) {
            mipLevels = (uint32_t) l;
            return *this;
        }
        CreateInfo & setLayers(size_t f) {
            arrayLayers = (uint32_t) f;
            return *this;
        }
        CreateInfo & setTiling(VkImageTiling t) {
            tiling = t;
            return *this;
        }
        CreateInfo & setUsage(VkImageUsageFlags flags) {
            usage = flags;
            return *this;
        }
        CreateInfo & addUsage(VkImageUsageFlags flags) {
            usage |= flags;
            return *this;
        }
        CreateInfo & setInitialLayout(VkImageLayout l) {
            initialLayout = l;
            return *this;
        }
        CreateInfo & setMemoryUsage(DeviceMemoryUsage flags) {
            memory = toVkMemoryPropertyFlags(flags);
            return *this;
        }
        CreateInfo & setMemoryProperties(VkMemoryPropertyFlags flags) {
            memory = flags;
            return *this;
        }
        CreateInfo & setAspect(VkImageAspectFlags flags) {
            aspect = flags;
            return *this;
        }

        bool isCube() const {
            return VK_IMAGE_TYPE_2D == imageType && extent.width == extent.height && 1 == extent.depth && 6 == arrayLayers &&
                   (VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT & flags);
        }

        bool isCubeOrCubeArray() const {
            return VK_IMAGE_TYPE_2D == imageType && extent.width == extent.height && 1 == extent.depth && 6 <= arrayLayers && 0 == (arrayLayers % 6) &&
                   (VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT & flags);
        }
    };

    const VulkanGlobalInfo *   global = 0;
    CreateInfo                 ci {};
    VkImage                    image = 0;
    AutoHandle<VkDeviceMemory> memory;
    VkImageView                view       = 0; ///< default view of the whole image.
    VkImageViewType            viewType   = VK_IMAGE_VIEW_TYPE_2D;
    VkSampler                  sampler    = 0;
    VmaAllocation              allocation = 0;

    PH_NO_COPY(ImageObject);

    /// construct an empty image object
    ImageObject() = default;

    /// construct an non-empty image object. The name argument is not required (can be set null), but highly recommended for debugging.
    ImageObject(const char * name, const VulkanGlobalInfo & vgi, const CreateInfo & ci) { create(name, vgi, ci); }

    ~ImageObject() { clear(); }

    /// move constructor
    ImageObject(ImageObject && rhs) noexcept {
        global         = rhs.global;
        rhs.global     = nullptr;
        image          = rhs.image;
        rhs.image      = VK_NULL_HANDLE;
        memory         = ::std::move(rhs.memory);
        view           = rhs.view;
        rhs.view       = VK_NULL_HANDLE;
        viewType       = rhs.viewType;
        allocation     = rhs.allocation;
        rhs.allocation = VK_NULL_HANDLE;
        sampler        = rhs.sampler;
        rhs.sampler    = VK_NULL_HANDLE;
    }

    /// move operator
    ImageObject & operator=(ImageObject && rhs) noexcept {
        if (this == &rhs) return *this;
        global         = rhs.global;
        rhs.global     = nullptr;
        image          = rhs.image;
        rhs.image      = VK_NULL_HANDLE;
        memory         = ::std::move(rhs.memory);
        view           = rhs.view;
        rhs.view       = VK_NULL_HANDLE;
        viewType       = rhs.viewType;
        allocation     = rhs.allocation;
        rhs.allocation = VK_NULL_HANDLE;
        sampler        = rhs.sampler;
        rhs.sampler    = VK_NULL_HANDLE;
        return *this;
    }

    /// clear the image object. Release all resources
    ImageObject & clear() {
        if (global) {
            global->safeDestroy(image, allocation);
            global->safeDestroy(view);
            global->safeDestroy(sampler);
        }
        PH_ASSERT(0 == image);
        PH_ASSERT(0 == view);
        PH_ASSERT(0 == allocation);
        PH_ASSERT(0 == sampler);
        memory.clear();
        global   = 0;
        viewType = VK_IMAGE_VIEW_TYPE_2D;
        return *this;
    }

    bool empty() const { return 0 == global || !image; }

    /// create an image object. The name argument is not required (can be set null), but highly recommended for debugging.
    ImageObject & create(const char * name, const VulkanGlobalInfo &, const CreateInfo &);

    /// Create an image object from a Vulkan image description.
    ///
    /// If the raw image contains 6 faces and each face is a square (width == height), then this function will
    /// create a cube texture.
    ///
    ImageObject & createFromImageProxy(const char * name, VulkanSubmissionProxy & vsp, VkImageUsageFlags usage, DeviceMemoryUsage memoryUsage,
                                       const ImageProxy & ip);

    /// Immediately reset the whole image into specific layout.
    void resetLayout(VulkanSubmissionProxy & vsp, VkImageLayout layout);
};

// ---------------------------------------------------------------------------------------------------------------------
/// A helper function to create a VkImageSubresourceRange for the whole image
inline constexpr VkImageSubresourceRange wholeImage(VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
    return {aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
};

// ---------------------------------------------------------------------------------------------------------------------
/// A helper function to create a VkImageSubresourceRange that represents the base map of the first face.
inline constexpr VkImageSubresource firstSubImage(VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) { return {aspect, 0, 0}; }

// ---------------------------------------------------------------------------------------------------------------------
/// A helper function to create a VkImageSubresourceRange that represents the base map of the first face.
inline constexpr VkImageSubresourceRange firstSubImageRange(VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) { return {aspect, 0, 1, 0, 1}; }

// ---------------------------------------------------------------------------------------------------------------------
/// Convert ColorFormat to its VK counterpart. Returns VK_FORMAT_UNKNOWN, if the conversion failed.
PH_API VkFormat colorFormat2VK(const ph::ColorFormat & colorFormat);

// ---------------------------------------------------------------------------------------------------------------------
/// Convert VkFormat to it's ColorFormat counterpart. Returns ColorFormat::UNKNOWN, if the conversion failed.
PH_API ph::ColorFormat colorFormatFromVK(VkFormat);

// ---------------------------------------------------------------------------------------------------------------------
// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
// \Note: this is obsolete. Use SimpleBarrier class instead.
void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                    const VkImageSubresourceRange & subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

// ---------------------------------------------------------------------------------------------------------------------
// Read base level pixels of a VkImage into RawImage
RawImage readBaseImagePixels(VulkanSubmissionProxy & vsp, VkImage image, VkImageLayout layout, VkFormat format, size_t width, size_t height);

} // namespace va
} // namespace ph