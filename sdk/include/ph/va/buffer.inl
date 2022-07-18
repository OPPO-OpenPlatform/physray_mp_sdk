// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph::va {

// ---------------------------------------------------------------------------------------------------------------------
/// Helper class of Vulkan Buffer
template<VkBufferUsageFlags USAGE, DeviceMemoryUsage MEMORY_USAGE, VkMemoryAllocateFlags ALLOC_FLAGS = 0>
struct BufferObject {
    const VulkanGlobalInfo *   global = 0;
    VkBuffer                   buffer = 0;
    AutoHandle<VkDeviceMemory> memory;
    VmaAllocation              allocation = 0;
    size_t                     size       = 0; // buffer length in bytes.
    bool                       mapped     = false;

    PH_NO_COPY(BufferObject);

    BufferObject() = default;

    ~BufferObject() { clear(); }

    /// move constructore
    BufferObject(BufferObject && rhs) noexcept {
        global         = rhs.global;
        rhs.global     = nullptr;
        buffer         = rhs.buffer;
        rhs.buffer     = VK_NULL_HANDLE;
        memory         = std::move(rhs.memory);
        allocation     = rhs.allocation;
        rhs.allocation = VK_NULL_HANDLE;
        size           = rhs.size;
        rhs.size       = 0;
    }

    /// move operator
    BufferObject & operator=(BufferObject && rhs) noexcept {
        if (this == &rhs) return *this;
        global         = rhs.global;
        rhs.global     = nullptr;
        buffer         = rhs.buffer;
        rhs.buffer     = VK_NULL_HANDLE;
        memory         = std::move(rhs.memory);
        allocation     = rhs.allocation;
        rhs.allocation = VK_NULL_HANDLE;
        size           = rhs.size;
        rhs.size       = 0;
        return *this;
    }

    BufferObject & clear() {
        if (buffer) {
            PH_ASSERT(global);
            global->safeDestroy(buffer, allocation);
            buffer = 0;
        }
        memory.clear();
        global = nullptr;
        size   = 0;
        return *this;
    }

    bool good() const {
        if (global->vmaAllocator) {
            return !!buffer && !!allocation;
        } else {
            return !!buffer && !!memory;
        }
    }

    bool empty() const {
        if (global->vmaAllocator) {
            return !buffer || !allocation;
        } else {
            return !buffer || !memory;
        }
    }

    // TODO: consider moving name to the first parameter to encourage explicit naming of buffer objects.
    BufferObject & allocate(const VulkanGlobalInfo & g, size_t size_, const char * name = nullptr, uint32_t extraUsage = 0) {
        // check for redundant allocation call
        if (global && *global == g && size == size_) return *this;

        // clear old buffer
        clear();

        // create buffer
        auto ci  = VkBufferCreateInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr};
        ci.size  = size_;
        ci.usage = USAGE | extraUsage;
        AutoHandle<VkBuffer> b;

        // TODO: Rethink how we're going to map the memory property flags onto the allocation creation usage flags
        if (g.vmaAllocator) {
            VmaAllocationCreateInfo aci {};
            aci.flags         = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
            aci.requiredFlags = toVkMemoryPropertyFlags(MEMORY_USAGE);
            aci.pUserData     = const_cast<char *>(name);

            PH_VA_REQUIRE(vmaCreateBuffer(g.vmaAllocator, &ci, &aci, b.prepare(g), &allocation, nullptr));
        } else {
            PH_VA_REQUIRE(vkCreateBuffer(g.device, &ci, g.allocator, b.prepare(g)));

            VkMemoryRequirements requirements;
            vkGetBufferMemoryRequirements(g.device, b.get(), &requirements);

            // allocate memory
            auto m = allocateDeviceMemory(g, requirements, MEMORY_USAGE, ALLOC_FLAGS);
            PH_VA_REQUIRE(vkBindBufferMemory(g.device, b.get(), m.get(), 0));

            memory = std::move(m);
        }

        // done
        global = &g;
        buffer = b.detach();
        size   = size_;
        return *this;
    }

    template<typename T = uint8_t>
    struct MappedResult {
        BufferObject *  owner  = nullptr;
        MutableRange<T> range  = {}; ///< the mapped data buffer.
        size_t          offset = 0;  ///< offset of the mapped area, to the beginning of the buffer, in unit of T.

        PH_NO_COPY(MappedResult);
        PH_NO_MOVE(MappedResult);

        MappedResult() = default;

        MappedResult(BufferObject * owner_, MutableRange<T> range_, size_t offset_) {
            owner  = owner_;
            range  = range_;
            offset = offset_;
        }

        ~MappedResult() { unmap(); }

        bool empty() const { return range.empty(); }

        void unmap() {
            if (owner) {
                if (owner->allocation) {
                    PH_REQUIRE(owner->global->vmaAllocator);
                    vmaUnmapMemory(owner->global->vmaAllocator, owner->allocation);
                } else {
                    vkUnmapMemory(owner->global->device, owner->memory);
                }
                // TODO: need to flush the memory.
                owner->mapped = false;
                owner         = nullptr;
            }
            range.clear();
            PH_ASSERT(empty());
        }

        ConstRange<T> constRange() const { return range; }

        operator bool() const { return !empty(); }
    };

    template<typename T>
    bool validateRange(size_t & offsetInUnitOfT, size_t & lengthInUnitOfT) const {
        if (0 == lengthInUnitOfT) lengthInUnitOfT = size / sizeof(T);
        size_t end      = offsetInUnitOfT + lengthInUnitOfT;
        offsetInUnitOfT = clamp<size_t>(offsetInUnitOfT, 0, size / sizeof(T));
        end             = clamp<size_t>(end, offsetInUnitOfT, size / sizeof(T));
        if (offsetInUnitOfT >= end) return false;
        lengthInUnitOfT = end - offsetInUnitOfT;
        return true;
    }

    template<typename T>
    MappedResult<T> map(size_t offsetInUnitOfT = 0, size_t lengthInUnitOfT = 0) {
        PH_REQUIRE(!empty());
        PH_REQUIRE(!mapped);
        if (!validateRange<T>(offsetInUnitOfT, lengthInUnitOfT)) return {};
        mapped  = true;
        T * dst = nullptr;
        if (allocation) {
            PH_REQUIRE(global->vmaAllocator);
            PH_VA_REQUIRE(vmaMapMemory(global->vmaAllocator, allocation, (void **) &dst));
            dst += offsetInUnitOfT;
        } else {
            PH_VA_REQUIRE(vkMapMemory(global->device, memory, offsetInUnitOfT * sizeof(T), lengthInUnitOfT * sizeof(T), {}, (void **) &dst));
        }
        return {this, {dst, lengthInUnitOfT}, offsetInUnitOfT};
    }

    operator VkBuffer &() { return buffer; }
    operator const VkBuffer &() const { return buffer; }
};

// ---------------------------------------------------------------------------------------------------------------------
/// Helper class of Vulkan Buffer with staging data. This buffer is used to hold data that are mostly static and
/// only changed occationally.
template<VkBufferUsageFlags USAGE, typename T, DeviceMemoryUsage GPU_BUFFER_MEMORY_USAGE = DeviceMemoryUsage::GPU_ONLY,
         VkMemoryAllocateFlags GPU_BUFFER_ALLOC_FLAGS = 0>
struct StagedBufferObject {

    typedef BufferObject<VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, DeviceMemoryUsage::CPU_ONLY> CpuBuffer;

    typedef BufferObject<VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | USAGE, GPU_BUFFER_MEMORY_USAGE, GPU_BUFFER_ALLOC_FLAGS>
        GpuBuffer;

    CpuBuffer s; ///< Staging/CPU data
    GpuBuffer g; ///< GPU data

    PH_NO_COPY(StagedBufferObject);

    /// Default constructor
    StagedBufferObject() = default;

    /// move constructor
    StagedBufferObject(StagedBufferObject && rhs) {
        s = std::move(rhs.s);
        g = std::move(rhs.g);
    }

    /// move operator
    StagedBufferObject & operator=(StagedBufferObject && rhs) {
        if (this == &rhs) return *this;
        s = std::move(rhs.s);
        g = std::move(rhs.g);
        return *this;
    }

    void clear() {
        s.clear();
        g.clear();
    }

    StagedBufferObject & allocate(const VulkanGlobalInfo & gi, size_t length, const char * name = nullptr) {
        s.allocate(gi, sizeof(T) * length, name);
        g.allocate(gi, sizeof(T) * length, name);
        return *this;
    }

    StagedBufferObject & allocate(const VulkanGlobalInfo & gi, const ConstRange<T> & data, const char * name = nullptr) {
        return allocate(gi, data.size(), name).update(0, data);
    }

    typedef CpuBuffer::MappedResult<T> MappedResult;

    MappedResult map(size_t offsetInUnitOfT = 0, size_t lengthInUnitOfT = 0) { return s.map<T>(offsetInUnitOfT, lengthInUnitOfT); }

    StagedBufferObject & update(size_t offsetInUnitOfT, const ConstRange<T> & data) {
        auto mapped = map(offsetInUnitOfT, data.size());
        memcpy(mapped.range.data(), data.data(), (size_t) (mapped.range.size() * sizeof(T)));
        return *this;
    }

    template<typename PROC>
    StagedBufferObject & update(size_t offsetInUnitOfT, size_t lenghInUnitOfT, PROC p) {
        auto mapped = map(offsetInUnitOfT, lenghInUnitOfT);
        for (size_t i = 0; i < mapped.range.size(); ++i) {
            p(i, mapped.range[i]);
        }
        return *this;
    }

    StagedBufferObject & sync2gpu(VkCommandBuffer cb) {
        PH_ASSERT(s.size == g.size);
        if (s.empty()) return *this; // do nothing if the CPU buffer is empty.
        auto region = VkBufferCopy {0, 0, s.size};
        vkCmdCopyBuffer(cb, s, g, 1, &region);
        return *this;
    }

    StagedBufferObject & sync2gpu(VkCommandBuffer cb, size_t offsetInUnitOfT, size_t lengthInUnitOfT) {
        PH_ASSERT(s.size == g.size);
        if (s.empty()) return *this; // do nothing if the CPU buffer is empty.
        if (!s.validateRange<T>(offsetInUnitOfT, lengthInUnitOfT)) return *this;
        PH_ASSERT(lengthInUnitOfT > 0);
        auto offset = offsetInUnitOfT * sizeof(T);
        auto length = lengthInUnitOfT * sizeof(T);
        auto region = VkBufferCopy {offset, offset, length};
        vkCmdCopyBuffer(cb, s, g, 1, &region);
        return *this;
    }

    StagedBufferObject & sync2gpu(VkCommandBuffer cb, const VkBufferCopy * regions, size_t count) {
        PH_ASSERT(s.size == g.size);
        if (s.empty()) return *this; // do nothing if the CPU buffer is empty.
        if (regions && count > 0) { vkCmdCopyBuffer(cb, s, g, (uint32_t) count, regions); }
        return *this;
    }

    // Synchronously copy buffer content from GPU to CPU.
    // Note that this call is EXTREMELY expensive, since it stalls both CPU and GPU.
    StagedBufferObject & syncToCpu(VkCommandBuffer cb) {
        PH_ASSERT(s.size == g.size);
        if (g.empty()) return *this; // do nothing if the GPU buffer is empty.
        auto region = VkBufferCopy {0, 0, g.size};
        vkCmdCopyBuffer(cb, g, s, 1, &region);
        return *this;
    }

    bool empty() const { return s.empty(); }

    /// return size of the buffer in unit of T.
    size_t size() const { return s.size / sizeof(T); }
};

// ---------------------------------------------------------------------------------------------------------------------
/// Helper class of Vulkan Buffer for sending dynamic data to GPU. This class is suitalbe for data that updated once
/// or a few times per frame.
template<VkBufferUsageFlags USAGE, typename T, DeviceMemoryUsage GPU_BUFFER_MEMORY_USAGE = DeviceMemoryUsage::GPU_ONLY,
         VkMemoryAllocateFlags GPU_BUFFER_ALLOC_FLAGS = 0>
class DynamicBufferObject {
public:
    typedef BufferObject<VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, DeviceMemoryUsage::CPU_ONLY> CpuBuffer;

    typedef BufferObject<VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | USAGE, GPU_BUFFER_MEMORY_USAGE, GPU_BUFFER_ALLOC_FLAGS>
        GpuBuffer;

    PH_NO_COPY(DynamicBufferObject);
    PH_NO_MOVE(DynamicBufferObject);

    DynamicBufferObject() = default;

    DynamicBufferObject & allocate(const VulkanGlobalInfo & vgi, size_t maxSimultenousCopy, size_t length, const char * name = nullptr) {
        _stagings.resize(maxSimultenousCopy);
        for (auto & s : _stagings)
            s.allocate(vgi, sizeof(T) * length, name);
        _gpu.allocate(vgi, sizeof(T) * length, name);
        _stagingIndex = _whereTheLatestDataAre = 0;
        return *this;
    }

    using MappedResult = CpuBuffer::MappedResult<T>;

    /// map the cpu buffer and preserve previously filled data.
    MappedResult map() {
        auto mapped = _stagings[_stagingIndex].map<T>();
        if (_whereTheLatestDataAre != _stagingIndex) {
            auto source = _stagings[_whereTheLatestDataAre].map<T>();
            memcpy(mapped.range.data(), source.range().data(), mapped.range.size() * sizeof(T));
        }
        _whereTheLatestDataAre = _stagingIndex;
        return mapped;
    }

    /// map cpu buffer but discard any previous data.
    MappedResult mapDiscard() {
        _whereTheLatestDataAre = _stagingIndex;
        return _stagings[_stagingIndex].map<T>();
    }

    DynamicBufferObject & update(const ConstRange<T> & data) {
        PH_ASSERT(data.size() == size());
        auto mapped = mapDiscard();
        memcpy(mapped.range.data(), data.data(), (size_t) (mapped.range.size() * sizeof(T)));
        return *this;
    }

    DynamicBufferObject & sync2gpu(VkCommandBuffer cb) {
        if (_gpu.empty()) return *this; // do nothing if the GPU buffer is empty.
        auto & s      = _stagings[_stagingIndex];
        auto   region = VkBufferCopy {0, 0, s.size};
        vkCmdCopyBuffer(cb, s, _gpu, 1, &region);
        _stagingIndex = (_stagingIndex + 1) % _stagings.size();
        return *this;
    }

    bool empty() const { return _gpu.empty(); }

    size_t size() const { return _gpu.size / sizeof(T); }

    /// Returns reference to the GPU buffer.
    const GpuBuffer & g() const { return _gpu; }

private:
    std::vector<CpuBuffer> _stagings; ///< Streaming/CPU data
    GpuBuffer              _gpu;      ///< GPU data
    size_t                 _stagingIndex          = 0;
    size_t                 _whereTheLatestDataAre = 0;
};

} // namespace ph::va
