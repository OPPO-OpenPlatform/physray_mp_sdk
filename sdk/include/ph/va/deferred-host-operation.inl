// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.
#include <map>

namespace ph {
namespace va {

struct DeferredHostOperation {
    const VulkanGlobalInfo & vgi() const { return _vgi; }

    /// Pass a deferred function that will called after GPU finishes processing current frame of data.
    virtual void deferUntilGPUWorkIsDone(std::function<void()>) = 0;

    /// Release the parameter item after GPU is done processing curent frame.
    template<typename T>
    void releaseAfterGPUWorkIsDone(T && t) {
        struct Any {
            T value;
        };
        auto any = new Any {std::move(t)};
        deferUntilGPUWorkIsDone([any]() mutable {
            // delete the object.
            delete any;
        });
    }

    /// Allocate a shared temporary GPU buffer. The buffer will be release only after GPU operation is done AND all external references are released.
    auto allocateSharedScratchBuffer(uint64_t size, VkBufferUsageFlags u = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     DeviceMemoryUsage m = DeviceMemoryUsage::CPU_ONLY, VkMemoryAllocateFlags a = 0) -> std::shared_ptr<BufferObject> {
        auto bufferObject = std::make_shared<BufferObject>(u, m, a);
        bufferObject->allocate(_vgi, size, "shared scratch buffer");
        auto result = bufferObject;
        releaseAfterGPUWorkIsDone(std::move(bufferObject));
        PH_ASSERT(!bufferObject.get());
        return result;
    }

    /// Allocate a temporary GPU buffer. The buffer will be automatically released after GPU work is done.
    auto allocateScratchBuffer(uint64_t size, VkBufferUsageFlags u = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               DeviceMemoryUsage m = DeviceMemoryUsage::CPU_ONLY, VkMemoryAllocateFlags a = 0) -> BufferObject * {
        auto bufferObject = std::make_unique<BufferObject>(u, m, a);
        bufferObject->allocate(_vgi, size, "scratch buffer");
        auto result = bufferObject.get();
        releaseAfterGPUWorkIsDone(std::move(bufferObject));
        PH_ASSERT(!bufferObject.get());
        return result;
    }

    /// Record command buffer to upload data from CPU to GPU. The buffer content will remain unchanged, until the GPU
    /// commands are submitted to and executed by GPU.
    void cmdUploadToGpu(VkCommandBuffer cb, VkBuffer dstBuffer, size_t dstOffset, const void * data, size_t dataSize) {
        if (0 == data || 0 == dataSize) return;
        auto src = allocateScratchBuffer(dataSize);
        {
            const auto & mapped = src->map<uint8_t>();
            memcpy(mapped.range.data(), data, dataSize);
        }
        auto region      = VkBufferCopy {};
        region.srcOffset = 0;
        region.dstOffset = dstOffset;
        region.size      = dataSize;
        vkCmdCopyBuffer(cb, src->buffer, dstBuffer, 1, &region);
    }

    /// Upload data from CPU to GPU. Note that destOffset is in unit of byte.
    template<typename T>
    void cmdUploadToGpu(VkCommandBuffer cb, VkBuffer dest, size_t destOffset, const ConstRange<T> & source) {
        return cmdUploadToGpu(cb, dest, destOffset, source.data(), source.size() * sizeof(T));
    }

    /// Upload data from CPU to GPU. Note that destOffset is in unit of byte.
    template<typename T>
    void cmdUploadToGpu(VkCommandBuffer cb, VkBuffer dest, size_t destOffset, const std::vector<T> & source) {
        return cmdUploadToGpu(cb, dest, destOffset, source.data(), source.size() * sizeof(T));
    }

    std::shared_ptr<BufferObject> downloadFromGPU(VkCommandBuffer cb, VkBuffer buffer, size_t offset, size_t size) {
        if (!buffer || !size) return {};
        auto scratch     = allocateSharedScratchBuffer(size);
        auto region      = VkBufferCopy {};
        region.srcOffset = offset;
        region.dstOffset = 0;
        region.size      = size;
        vkCmdCopyBuffer(cb, buffer, scratch->buffer, 1, &region);
        return scratch;
    }

protected:
    DeferredHostOperation(const VulkanGlobalInfo & vgi): _vgi(vgi) {}
    virtual ~DeferredHostOperation() {}

private:
    const VulkanGlobalInfo & _vgi;
};

class SimpleDeferredFrameOperation : public va::DeferredHostOperation {
public:
    PH_NO_COPY_NO_MOVE(SimpleDeferredFrameOperation); // todo: make it moveable.

    SimpleDeferredFrameOperation(const va::VulkanGlobalInfo & vgi): va::DeferredHostOperation(vgi) {}

    void reset(const int64_t * newFrameID = nullptr) {
        for (auto iter = _deferredJobs.begin(); iter != _deferredJobs.end(); ++iter) {
            for (auto & f : iter->second) { f(); }
        }
        _deferredJobs.clear();
        if (newFrameID) _currentFrame = *newFrameID;
    }

    void updateFrameCounter(int64_t currentFrame, int64_t safeFrame) {
        // store new frame number
        PH_REQUIRE((currentFrame - _currentFrame) >= 0);
        PH_REQUIRE((currentFrame - safeFrame) > 0);
        _currentFrame = currentFrame;

        // perform jobs that are considered safe then remove them from job list.
        for (auto iter = _deferredJobs.begin(); iter != _deferredJobs.end();) {
            if ((iter->first - safeFrame) > 0) break; // encountered a job that is newer than the safe frame. time to stop.
            for (auto & f : iter->second) { f(); }
            iter = _deferredJobs.erase(iter);
        }
    }

    void deferUntilGPUWorkIsDone(std::function<void()> func) override {
        if (func) {
            _deferredJobs[_currentFrame].push_back(func);
        } else {
            PH_LOGW("It is unusual to defer an empty function.");
        }
    }

private:
    std::map<int64_t, std::vector<std::function<void()>>> _deferredJobs;
    int64_t                                               _currentFrame = 0;
};

} // namespace va
} // namespace ph