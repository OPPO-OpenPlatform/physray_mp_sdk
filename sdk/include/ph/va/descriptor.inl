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

} // namespace va
} // namespace ph