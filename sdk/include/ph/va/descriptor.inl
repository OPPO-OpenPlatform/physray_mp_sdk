/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include <map>

namespace ph {
namespace va {

// ---------------------------------------------------------------------------------------------------------------------
/// A descriptor pool manager
class SimpleDescriptorPool {
public:
    struct ConstructParameters {
        std::string                        name;
        VkDescriptorSetLayout              layout  = 0;
        size_t                             maxSets = 0;
        std::map<VkDescriptorType, size_t> poolSizes;
    };
    SimpleDescriptorPool(const ConstructParameters &);

    ~SimpleDescriptorPool() {}

    VkDescriptorSet allocateDescSet(DeferredHostOperation & dehop);

private:
    ConstructParameters              _cp;
    va::AutoHandle<VkDescriptorPool> _pool;
    VkDescriptorSet                  _dummySet = 0;
};

class SimpleDescriptorTable {
public:
    struct Descriptor {
        uint32_t         count = 0; // The struct is considered empty when count is zero. All other fields, except binding, are ignored in this case.
        VkDescriptorType type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        std::vector<VkDescriptorImageInfo>      images;
        std::vector<VkDescriptorBufferInfo>     buffers;
        std::vector<VkAccelerationStructureKHR> accelerationStructures;

        bool empty() const { return 0 == count; }

        void clear() {
            count = 0;
            images.clear();
            buffers.clear();
            accelerationStructures.clear();
        }

        bool operator==(const Descriptor & rhs) const;

        VkWriteDescriptorSet toVkWriteDescriptorSet(VkDescriptorSet set, size_t binding, std::list<StructureChain> & chain) const;
    };

public:
    SimpleDescriptorTable() {}
    ~SimpleDescriptorTable() {}

    PH_NO_COPY(SimpleDescriptorTable);

    // can move
    SimpleDescriptorTable(SimpleDescriptorTable && rhs) { _descriptors = std::move(rhs._descriptors); }
    SimpleDescriptorTable & operator=(SimpleDescriptorTable && rhs) {
        if (this == &rhs) return *this;
        _descriptors = std::move(rhs._descriptors);
        return *this;
    }

    bool empty() const { return _descriptors.empty(); }
    void clear() { _descriptors.clear(); }
    void bind(uint32_t binding, const Descriptor & desc) { _descriptors[binding] = desc; }
    void bindUniformBuffer(uint32_t binding, const BufferObject & buffer);
    void bindStorageBuffer(uint32_t binding, const BufferObject & buffer);
    void bindInputAttachment(uint32_t binding, VkImageView view);
    void bindTexture(uint32_t binding, VkSampler sampler, VkImageView view);
    void bindAccelerationStructure(uint32_t binding, VkAccelerationStructureKHR);
    bool flush(VkDevice device, VkDescriptorSet set) const;

    SimpleDescriptorTable operator-(const SimpleDescriptorTable &) const;

    template<typename PROC>
    void bindTextureArray(uint32_t binding, size_t count, PROC p) {
        Descriptor & desc = _descriptors[binding];
        desc.clear();
        desc.type  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        desc.count = (uint32_t) count;
        for (size_t i = 0; i < count; ++i) {
            std::pair<VkSampler, VkImageView> t = p(i);
            PH_ASSERT(t.first);
            PH_ASSERT(t.second);
            desc.images.push_back({t.first, t.second, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        }
    }

#ifndef UNIT_TEST
private:
#endif

    std::map<uint32_t, Descriptor> _descriptors; ///< key is binding index, value is list of descriptors.
};

} // namespace va
} // namespace ph
