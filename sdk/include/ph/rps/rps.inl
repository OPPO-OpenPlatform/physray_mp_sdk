// implement inline functions in rps.h
namespace ph::rps {

inline void RefBase::release(Root * p) {
    PH_ASSERT(p);
    if (1 == p->_ref.fetch_sub(1)) {
        PH_ASSERT(0 == p->_ref);
        p->onDestructing(p);
        Ref<Factory> f;
        if (p->_fac) f.reset(p->_fac); // this is to ensure the factory class is released after the pointer p.
        delete p;
    }
}

} // namespace ph::rps

// Define operators in global namespace to make them usable everywhere (like within std namespace)

#define PH_RPS_REFL ph::rps::Program::Reflection

inline bool operator==(const ph::rps::Buffer::View & lhs, const ph::rps::Buffer::View & rhs) {
    return lhs.buffer == rhs.buffer && lhs.offset == rhs.offset && lhs.size == rhs.size;
}
inline bool operator!=(const ph::rps::Buffer::View & lhs, const ph::rps::Buffer::View & rhs) { return !(lhs == rhs); }

inline bool operator==(const ph::rps::Image::View & lhs, const ph::rps::Image::View & rhs) {
    return lhs.image == rhs.image && 0 == memcmp(&lhs.range, &rhs.range, sizeof(lhs.range)) && lhs.format == rhs.format;
}
inline bool operator!=(const ph::rps::Image::View & lhs, const ph::rps::Image::View & rhs) { return !(lhs == rhs); }

inline bool operator==(const ph::rps::ImageSampler & lhs, const ph::rps::ImageSampler & rhs) { return lhs.image == rhs.image && lhs.sampler == rhs.sampler; }
inline bool operator!=(const ph::rps::ImageSampler & lhs, const ph::rps::ImageSampler & rhs) { return !(lhs == rhs); }

inline bool operator==(const PH_RPS_REFL::Descriptor & lhs, const PH_RPS_REFL::Descriptor & rhs) {
    return lhs.binding == rhs.binding && lhs.descriptorType == rhs.descriptorType && lhs.descriptorCount == rhs.descriptorCount &&
           lhs.stageFlags == rhs.stageFlags &&
           // TODO compare sampler content, not pointer.
           lhs.pImmutableSamplers == rhs.pImmutableSamplers;
}
inline bool operator!=(const PH_RPS_REFL::Descriptor & lhs, const PH_RPS_REFL::Descriptor & rhs) { return !(lhs == rhs); }
inline bool operator<(const PH_RPS_REFL::Descriptor & lhs, const PH_RPS_REFL::Descriptor & rhs) {
    if (&lhs == &rhs) return false;
    if (lhs.binding != rhs.binding) return lhs.binding < rhs.binding;
    if (lhs.descriptorType != rhs.descriptorType) return lhs.descriptorType < rhs.descriptorType;
    if (lhs.descriptorCount != rhs.descriptorCount) return lhs.descriptorCount < rhs.descriptorCount;
    if (lhs.stageFlags != rhs.stageFlags) return lhs.stageFlags < rhs.stageFlags;
    // immutable sampler field is ignored for now.
    return false;
}

inline bool operator==(const PH_RPS_REFL::VertexShaderInput & lhs, const PH_RPS_REFL::VertexShaderInput & rhs) {
    return lhs.location == rhs.location && lhs.format;
}
inline bool operator!=(const PH_RPS_REFL::VertexShaderInput & lhs, const PH_RPS_REFL::VertexShaderInput & rhs) { return !(lhs == rhs); }
inline bool operator<(const PH_RPS_REFL::VertexShaderInput & lhs, const PH_RPS_REFL::VertexShaderInput & rhs) {
    if (lhs.location != rhs.location) return lhs.location < rhs.location;
    return lhs.format < rhs.format;
}

inline bool operator==(const PH_RPS_REFL::Constant & lhs, const PH_RPS_REFL::Constant & rhs) {
    return lhs.stageFlags == rhs.stageFlags && lhs.offset == rhs.offset && lhs.size == rhs.size;
}
inline bool operator!=(const PH_RPS_REFL::Constant & lhs, const PH_RPS_REFL::Constant & rhs) { return !(lhs == rhs); }
inline bool operator<(const PH_RPS_REFL::Constant & lhs, const PH_RPS_REFL::Constant & rhs) {
    if (lhs.stageFlags != rhs.stageFlags) return lhs.stageFlags < rhs.stageFlags;
    if (lhs.offset != rhs.offset) return lhs.offset < rhs.offset;
    return lhs.size < rhs.size;
}

namespace ph::rps::detail {

template<typename>
struct is_std_vector : std::false_type {};

template<typename T, typename A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

template<typename MAP>
inline bool sameMap(const MAP & lhs, const MAP & rhs) {
    if (lhs.size() != rhs.size()) return false;
    auto a = lhs.begin();
    auto b = rhs.begin();
    for (size_t i = 0; i < lhs.size(); ++i, ++a, ++b) {
        if constexpr (is_std_vector<MAP>::value) {
            if (*a != *b) return false;
        } else {
            if (a->first != b->first) return false;
            if (a->second != b->second) return false;
        }
    }
    return true;
}

template<typename MAP>
inline bool lessMap(const MAP & lhs, const MAP & rhs) {
    if (lhs.size() != rhs.size()) return lhs.size() < rhs.size();
    auto a = lhs.begin();
    auto b = rhs.begin();
    for (size_t i = 0; i < lhs.size(); ++i, ++a, ++b) {
        if constexpr (is_std_vector<MAP>::value) {
            if (*a != *b) return *a < *b;
        } else {
            if (a->first != b->first) return a->first < b->first;
            if (a->second != b->second) return a->second < b->second;
        }
    }
    return false;
}

} // namespace ph::rps::detail

inline bool operator==(const PH_RPS_REFL::DescriptorSet & lhs, const PH_RPS_REFL::DescriptorSet & rhs) { return ph::rps::detail::sameMap(lhs, rhs); }
inline bool operator!=(const PH_RPS_REFL::DescriptorSet & lhs, const PH_RPS_REFL::DescriptorSet & rhs) { return !(lhs == rhs); }
inline bool operator<(const PH_RPS_REFL::DescriptorSet & lhs, const PH_RPS_REFL::DescriptorSet & rhs) { return ph::rps::detail::lessMap(lhs, rhs); }

inline bool operator==(const PH_RPS_REFL::DescriptorLayout & lhs, const PH_RPS_REFL::DescriptorLayout & rhs) { return ph::rps::detail::sameMap(lhs, rhs); }
inline bool operator!=(const PH_RPS_REFL::DescriptorLayout & lhs, const PH_RPS_REFL::DescriptorLayout & rhs) { return !(lhs == rhs); }
inline bool operator<(const PH_RPS_REFL::DescriptorLayout & lhs, const PH_RPS_REFL::DescriptorLayout & rhs) { return ph::rps::detail::lessMap(lhs, rhs); }

inline bool operator==(const PH_RPS_REFL::ConstantLayout & lhs, const PH_RPS_REFL::ConstantLayout & rhs) { return ph::rps::detail::sameMap(lhs, rhs); }
inline bool operator!=(const PH_RPS_REFL::ConstantLayout & lhs, const PH_RPS_REFL::ConstantLayout & rhs) { return !(lhs == rhs); }
inline bool operator<(const PH_RPS_REFL::ConstantLayout & lhs, const PH_RPS_REFL::ConstantLayout & rhs) { return ph::rps::detail::lessMap(lhs, rhs); }

inline bool operator==(const PH_RPS_REFL::VertexLayout & lhs, const PH_RPS_REFL::VertexLayout & rhs) { return ph::rps::detail::sameMap(lhs, rhs); }
inline bool operator!=(const PH_RPS_REFL::VertexLayout & lhs, const PH_RPS_REFL::VertexLayout & rhs) { return !(lhs == rhs); }
inline bool operator<(const PH_RPS_REFL::VertexLayout & lhs, const PH_RPS_REFL::VertexLayout & rhs) { return ph::rps::detail::lessMap(lhs, rhs); }

#undef PH_RPS_REFL