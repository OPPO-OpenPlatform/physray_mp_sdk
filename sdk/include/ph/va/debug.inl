// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.
namespace ph::va {

// ---------------------------------------------------------------------------------------------------------------------
/// Helper function to set Vulkan opaque handle's name (VK_EXT_debug_utils).
template<typename T>
inline void setVkObjectName(VkDevice device, VkObjectType type, T handle, const char * name) {
    if (!vkSetDebugUtilsObjectNameEXT || !handle || !name) return;
    auto info = VkDebugUtilsObjectNameInfoEXT {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, {}, type, (uint64_t) handle, name,
    };
    vkSetDebugUtilsObjectNameEXT(device, &info);
}

// ---------------------------------------------------------------------------------------------------------------------
/// Helper function to set Vulkan opaque handle's name (VK_EXT_debug_utils).
template<typename T>
inline void setVkObjectName(VkDevice device, T handle, const char * name) {
    setVkObjectName(device, getHandleObjectType<T>(), handle, name);
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline void beginVkDebugLable(VkCommandBuffer cb, const char * label) {
    if (!vkCmdBeginDebugUtilsLabelEXT) return;
    auto info = VkDebugUtilsLabelEXT {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        nullptr,
        (label && *label) ? label : "<unnamed>",
    };
    vkCmdBeginDebugUtilsLabelEXT(cb, &info);
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline void endVkDebugLable(VkCommandBuffer cb) {
    if (!vkCmdEndDebugUtilsLabelEXT) return;
    vkCmdEndDebugUtilsLabelEXT(cb);
}

// ---------------------------------------------------------------------------------------------------------------------
//
inline void insertVkDebugLable(VkCommandBuffer cb, const char * label) {
    if (!vkCmdInsertDebugUtilsLabelEXT || !label || !*label) return;
    auto info = VkDebugUtilsLabelEXT {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        nullptr,
        label,
    };
    vkCmdInsertDebugUtilsLabelEXT(cb, &info);
}

// ---------------------------------------------------------------------------------------------------------------------
// check if the app is loaded by renderdoc
bool isRenderDocPresent();

} // namespace ph::va