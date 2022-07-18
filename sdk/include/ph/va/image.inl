// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph::va {

// -----------------------------------------------------------------------------
/// Helper class of Vulkan Image
struct ImageObject {
    struct CreateInfo : public VkImageCreateInfo {
        DeviceMemoryUsage memoryUsage;

        CreateInfo() {
            sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            pNext                 = nullptr;
            flags                 = 0;
            imageType             = VK_IMAGE_TYPE_2D;
            format                = VK_FORMAT_UNDEFINED;
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
            memoryUsage           = DeviceMemoryUsage::GPU_ONLY;
        }

        CreateInfo & set2D(uint32_t w, uint32_t h) {
            flags         = 0;
            imageType     = VK_IMAGE_TYPE_2D;
            extent.width  = w;
            extent.height = h;
            extent.depth  = 1;
            arrayLayers   = 1;
            PH_ASSERT(!isCube());
            return *this;
        }

        CreateInfo & setCube(uint32_t w) {
            imageType     = VK_IMAGE_TYPE_2D;
            extent.width  = w;
            extent.height = w;
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
        CreateInfo & setLevels(uint32_t l) {
            mipLevels = l;
            return *this;
        }
        CreateInfo & setLayers(uint32_t f) {
            arrayLayers = f;
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
        CreateInfo & setLayout(VkImageLayout l) {
            initialLayout = l;
            return *this;
        }
        CreateInfo & setMemoryUsage(DeviceMemoryUsage flags) {
            memoryUsage = flags;
            return *this;
        }

        bool isCube() const {
            return VK_IMAGE_TYPE_2D == imageType && extent.width == extent.height && 1 == extent.depth && 6 == arrayLayers &&
                   (VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT & flags);
        }
    };

    const VulkanGlobalInfo *   global = 0;
    CreateInfo                 ci {};
    VkImage                    image = 0;
    AutoHandle<VkDeviceMemory> memory;
    VkImageView                view       = 0; ///< default view of the whole image.
    VkImageViewType            viewType   = VK_IMAGE_VIEW_TYPE_2D;
    VmaAllocation              allocation = 0;

    PH_NO_COPY(ImageObject);

    ImageObject() = default;

    ~ImageObject() { clear(); }

    /// move constructore
    ImageObject(ImageObject && rhs) noexcept {
        global         = rhs.global;
        rhs.global     = nullptr;
        image          = rhs.image;
        rhs.image      = VK_NULL_HANDLE;
        memory         = std::move(rhs.memory);
        view           = rhs.view;
        rhs.view       = VK_NULL_HANDLE;
        viewType       = rhs.viewType;
        allocation     = rhs.allocation;
        rhs.allocation = VK_NULL_HANDLE;
    }

    /// move operator
    ImageObject & operator=(ImageObject && rhs) noexcept {
        if (this == &rhs) return *this;
        global         = rhs.global;
        rhs.global     = nullptr;
        image          = rhs.image;
        rhs.image      = VK_NULL_HANDLE;
        memory         = std::move(rhs.memory);
        view           = rhs.view;
        rhs.view       = VK_NULL_HANDLE;
        viewType       = rhs.viewType;
        allocation     = rhs.allocation;
        rhs.allocation = VK_NULL_HANDLE;
        return *this;
    }

    /// clear the image object. Release all resources
    ImageObject & clear() {
        if (global) {
            global->safeDestroy(image, allocation);
            global->safeDestroy(view);
        }
        PH_ASSERT(0 == image);
        PH_ASSERT(0 == view);
        PH_ASSERT(0 == allocation);
        memory.clear();
        global   = 0;
        viewType = VK_IMAGE_VIEW_TYPE_2D;
        return *this;
    }

    bool empty() const { return 0 == global || !image; }

    ImageObject & create(const char * name, const VulkanGlobalInfo &, const CreateInfo &);

    /// Create an image object from a Vulkan image description.
    ///
    /// If the raw image contains 6 faces and each face is a square (width == height), then this function will
    /// create a cube texture.
    ///
    ImageObject & createFromImageProxy(const char * name, VulkanSubmissionProxy & vsp, VkImageUsageFlags usage, DeviceMemoryUsage memoryUsage,
                                       const ImageProxy & ip);
};

// ---------------------------------------------------------------------------------------------------------------------
/// Convert ColorFormat to its VK counterpart. Returns VK_FORMAT_UNKNOWN, if the conversion failed.
PH_API VkFormat colorFormat2VK(const ph::ColorFormat & colorFormat);

// ---------------------------------------------------------------------------------------------------------------------
/// Convert VkFormat to it's ColorFormat counterpart. Returns ColorFormat::UNKNOWN, if the conversion failed.
PH_API ph::ColorFormat colorFormatFromVK(VkFormat);

// ---------------------------------------------------------------------------------------------------------------------
// Setup an image barrier structure to transfer image layout
PH_API void setupImageBarrier(VkImageMemoryBarrier & barrier, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                              const VkImageSubresourceRange & subresourceRange);

// ---------------------------------------------------------------------------------------------------------------------
// A simplified call to update image barriers
inline void simpleImageBarriers(VkCommandBuffer cmdbuffer, const ConstRange<VkImageMemoryBarrier> & barriers,
                                VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {
    vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, (uint32_t) barriers.size(), barriers.data());
}

// ---------------------------------------------------------------------------------------------------------------------
// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                    const VkImageSubresourceRange & subresourceRange, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

// ---------------------------------------------------------------------------------------------------------------------
// Read base level pixels of a VkImage into RawImage
RawImage readBaseImagePixels(VulkanSubmissionProxy & vsp, VkImage image, VkImageLayout layout, VkFormat format, uint32_t width, uint32_t height);

} // end of namespace ph::va
