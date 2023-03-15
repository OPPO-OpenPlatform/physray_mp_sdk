// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

#include <set>

namespace ph {
namespace va {

/// Extensible structure chain used by Vulkan API.
struct StructureChain {
    /// data buffer that stores the VK feature structure.
    std::vector<uint8_t> buffer;

    /// construct new feature
    template<typename T>
    StructureChain(const T & feature) {
        buffer.resize(sizeof(feature));
        memcpy(buffer.data(), &feature, sizeof(feature));
    }

    template<typename T>
    T * reset() {
        buffer.resize(sizeof(T));
        return (T *) buffer.data();
    }
};

class SimpleVulkanInstance {
public:
    /// Define level of validation on Vulkan error.
    enum Validation {
        VALIDATION_DISABLED = 0,
        LOG_ON_VK_ERROR,
        LOG_ON_VK_ERROR_WITH_CALL_STACK,
        THROW_ON_VK_ERROR,
        BREAK_ON_VK_ERROR,
    };

    enum Verbosity {
        SILENCE = 0,
        BRIEF,
        VERBOSE,
    };

    struct ConstructParameters {
        /// The Vulkan API version. Default is 1.1
        uint32_t apiVersion = VK_MAKE_VERSION(1, 1, 0);

        /// Specify extra layers to initialize VK instance. The 2nd value indicates if the layer is required or not.
        /// We have to use vector instead of map here, because layer loading order matters.
        std::vector<std::pair<std::string, bool>> layers;

        /// Specify extra extension to initialize VK instance. Value indicate if the extension is required or not.
        std::map<std::string, bool> instanceExtensions;

        /// structure chain passed to VkInstanceCreateInfo::pNext
        std::vector<StructureChain> instanceCreateInfo;

        /// Set to true to enable validation layer.
        Validation validation = PH_BUILD_DEBUG ? LOG_ON_VK_ERROR_WITH_CALL_STACK : VALIDATION_DISABLED;

        /// Creation log output verbosity
        Verbosity printVkInfo = BRIEF;
    };

    SimpleVulkanInstance(ConstructParameters);

    ~SimpleVulkanInstance();

    const ConstructParameters & cp() const { return _cp; }

    VkInstance get() const { return _instance; }

    operator VkInstance() const { return _instance; }

private:
    ConstructParameters      _cp;
    VkInstance               _instance    = nullptr;
    VkDebugReportCallbackEXT _debugReport = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
//
class SimpleVulkanDevice {
public:
    struct ConstructParameters {
        /// pointer to Vulkan instance.
        SimpleVulkanInstance * instance = nullptr;

        /// Specify extra extension to initialize VK device. Value indicate if the extension is required or not.
        std::map<std::string, bool> deviceExtensions;

        /// Set to true to defer to VMA for device memory allocations.
        bool useVmaAllocator = false;

        /// set to false to make the creation log less verbose.
        SimpleVulkanInstance::Verbosity printVkInfo = SimpleVulkanInstance::BRIEF;

        /// Basic VK device feature list defined by Vulkan 1.0
        VkPhysicalDeviceFeatures features1 {};

        /// Extensible VK device feature list defined Vulkan 1.1
        std::vector<StructureChain> features2;

        /// Add new feature to the feature2 list.
        template<typename T>
        T & addFeature(const T & feature) {
            features2.emplace_back(feature);
            return *(T *) features2.back().buffer.data();
        }
    };

    SimpleVulkanDevice(ConstructParameters);

    ~SimpleVulkanDevice();

    const ConstructParameters & cp() const { return _cp; }

    const ph::va::VulkanGlobalInfo & vgi() const { return _vgi; }

    VulkanSubmissionProxy & graphicsQ() const { return *_queues[_gfxQueueFamilyIndex].get(); }
    VulkanSubmissionProxy & transferQ() const { return *_queues[_tfrQueueFamilyIndex].get(); }
    VulkanSubmissionProxy & computeQ() const { return *_queues[_cmpQueueFamilyIndex].get(); }

    VulkanSubmissionProxy * searchForPresentQ(VkSurfaceKHR) const;

    void resize(VkSurfaceKHR, VkFormat, uint32_t, uint32_t);

    VkResult waitIdle() const { return _vgi.device ? threadSafeDeviceWaitIdle(_vgi.device) : VK_SUCCESS; }

    class Details;
    Details & details() const { return *_details; }

private:
    Details *                                           _details = nullptr;
    ConstructParameters                                 _cp;
    ph::va::VulkanGlobalInfo                            _vgi {};
    std::vector<std::unique_ptr<VulkanSubmissionProxy>> _queues; // one for each queue family
    uint32_t                                            _gfxQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32_t                                            _tfrQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32_t                                            _cmpQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bool                                                _lost                = false;
};

/// This is used to temporarily mute error log of Vulkan validation error, when you are expecting some VK errors and don't want to those pollute the log output.
PH_API void muteValidationErrorLog();

/// Call this to unmute the VK validation error log.
PH_API void unmuteValidationErrorLog();

/// A helper class to mute VK error log within the current execution scope.
template<bool ENABLED = true>
class MuteValidationErrorWithinCurrentScope {
public:
    MuteValidationErrorWithinCurrentScope() {
        if (ENABLED) muteValidationErrorLog();
    }
    ~MuteValidationErrorWithinCurrentScope() {
        if (ENABLED) unmuteValidationErrorLog();
    }
};

} // namespace va
} // namespace ph