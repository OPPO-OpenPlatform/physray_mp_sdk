#include <map>

namespace ph {
namespace va {

// ---------------------------------------------------------------------------------------------------------------------
/// A mini compute shader pipeline
class SimpleCompute {
public:
    struct Descriptors {
        VkDescriptorType type;
        size_t           count;
    };

    struct ConstructParameters {
        const VulkanGlobalInfo &      vgi;
        AutoHandle<VkShaderModule>    cs;
        std::map<size_t, Descriptors> bindings; // key is binding point index. value is resource type and count.
        uint32_t                      pushConstantsSize = 0;
        uint32_t                      workGroupSizes[3] = {32, 1, 1};
    };

    SimpleCompute(const ConstructParameters &);

    ~SimpleCompute();

    struct DescriptorArray {
        enum Type {
            BUFFER,
            IMAGE,
        };

        Type                                type = BUFFER;
        std::vector<VkDescriptorBufferInfo> buffer;
        std::vector<VkDescriptorImageInfo>  image;

        DescriptorArray & operator=(std::vector<VkDescriptorBufferInfo> && data) {
            type   = BUFFER;
            buffer = std::move(data);
            image.clear();
            return *this;
        }

        DescriptorArray & operator=(std::vector<VkDescriptorImageInfo> && data) {
            type  = IMAGE;
            image = std::move(data);
            buffer.clear();
            return *this;
        }

        /// templated info array getter
        template<typename T>
        const std::vector<T> * array() const;

        size_t size() const { return (BUFFER == type) ? buffer.size() : image.size(); }
    };

    typedef std::map<size_t, DescriptorArray> DescriptorBindings;

    struct DispatchParameters {
        DeferredHostOperation & dop;

        VkCommandBuffer cb = 0;

        DescriptorBindings bindings;

        size_t width  = 1;
        size_t height = 1;
        size_t depth  = 1;

        const void * pushConstants       = nullptr;
        size_t       pushConstantsOffset = 0;
        size_t       pushConstantsSize   = 0;

        DispatchParameters & setDimension(size_t w, size_t h = 1, size_t d = 1) {
            width  = w;
            height = h;
            depth  = d;
            return *this;
        }

        DispatchParameters & addBuffer(size_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
            auto & a = bindings[binding];
            PH_ASSERT(DescriptorArray::BUFFER == a.type || (a.buffer.empty() && a.image.empty()));
            a.type = DescriptorArray::BUFFER;
            a.buffer.push_back({buffer, offset, range});
            return *this;
        }

        DispatchParameters & addImage(size_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout) {
            auto & a = bindings[binding];
            PH_ASSERT(DescriptorArray::IMAGE == a.type || (a.buffer.empty() && a.image.empty()));
            a.type = DescriptorArray::IMAGE;
            a.image.push_back({sampler, imageView, imageLayout});
            return *this;
        }

        template<typename T>
        DispatchParameters & setPushConstants(const T & t) {
            pushConstants     = &t;
            pushConstantsSize = sizeof(T);
            return *this;
        }

        template<typename T>
        DispatchParameters & setPushConstants(const ConstRange<T> & data) {
            pushConstants     = data.data();
            pushConstantsSize = data.size() * sizeof(T);
            return *this;
        }
    };

    void dispatch(const DispatchParameters &);

private:
    class Impl;
    Impl * _impl = nullptr;
};

template<>
inline const std::vector<VkDescriptorBufferInfo> * SimpleCompute::DescriptorArray::array<VkDescriptorBufferInfo>() const {
    return BUFFER == type ? &buffer : nullptr;
}

template<>
inline const std::vector<VkDescriptorImageInfo> * SimpleCompute::DescriptorArray::array<VkDescriptorImageInfo>() const {
    return IMAGE == type ? &image : nullptr;
}

} // namespace va
} // namespace ph