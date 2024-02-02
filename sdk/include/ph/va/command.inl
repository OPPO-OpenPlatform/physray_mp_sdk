/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph {
namespace va {

//----------------------------------------------------------------------------------------------------------------------
/// A proxy class to make sure the VkQueue is used in thread safe manner.
struct VulkanSubmissionProxy {
    virtual ~VulkanSubmissionProxy() = default;

    const VulkanGlobalInfo & vgi() const { return _vgi; }

    uint32_t queueFamilyIndex() const { return _queueFamilyIndex; }

    struct SubmitInfo {
        ConstRange<VkSemaphore>          waitSemaphores;
        ConstRange<VkPipelineStageFlags> waitStages;
        ConstRange<VkCommandBuffer>      commandBuffers;
        ConstRange<VkSemaphore>          signalSemaphores;
    };

    virtual VkResult submit(const ConstRange<SubmitInfo> &, VkFence signalFence = VK_NULL_HANDLE, const char * errorMessagePromptForDeviceLost = nullptr) = 0;

    VkResult submit(const SubmitInfo & si, VkFence signalFence = VK_NULL_HANDLE) { return submit({&si, 1}, signalFence); }

    struct PresentInfo {
        ConstRange<VkSemaphore>    waitSemaphores;
        ConstRange<VkSwapchainKHR> swapchains;
        ConstRange<uint32_t>       imageIndices;
    };

    virtual VkResult present(const PresentInfo &) = 0;

    // wait for the queue to be completely idle (including both CPU and GPU)
    virtual VkResult waitIdle(const char * deviceLostErrorPrompt = nullptr) = 0;

protected:
    VulkanSubmissionProxy(const VulkanGlobalInfo & vgi_, uint32_t queueFamilyIndex): _vgi(vgi_), _queueFamilyIndex(queueFamilyIndex) {}

private:
    const VulkanGlobalInfo & _vgi;
    const uint32_t           _queueFamilyIndex;
};

//----------------------------------------------------------------------------------------------------------------------
/// This is the most simple form of one time use command buffer.
///
/// Note that due to its blocking nature, it is not recommended for performance critical scenario.
///
/// With this class, you create the the command buffer by calling `create()` and submit all the work by calling
/// `finish()`. The `finish()` call will block the caller until all work in the command buffer is completely done.
///
/// Example usage:
///
///     SingleUseCommandPool pool(vgi);
///
///     for(auto & work : all_work_items) {
///         auto cb = pool.create();
///
///         do_the_work(cb, w); // push commands to the command buffer
///
///         // submit the work to GPU. Wait for it to finish. Then release the cb.
///         pool.finish(cb);
///
///         // Accessing cb after calling finish() will cause undefined behavior.
///     }
class SingleUseCommandPool {
public:
    PH_NO_COPY(SingleUseCommandPool);
    PH_NO_MOVE(SingleUseCommandPool);

    struct CommandBuffer {
        enum State {
            BEGUN,
            SUBMITTED,
            FINISHED,
        };
        SingleUseCommandPool * pool {};
        VkCommandBuffer        cb {};
        State                  state = BEGUN;

        /// cast to Vulkan's native type.
        operator VkCommandBuffer() const { return cb; }
    };

    /// constructor
    /// \param proxy Specify the command submission proxy.
    SingleUseCommandPool(VulkanSubmissionProxy & vsp): _vsp(vsp) {
        auto & vgi            = vsp.vgi();
        auto   cpci           = VkCommandPoolCreateInfo {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpci.queueFamilyIndex = vsp.queueFamilyIndex();
        PH_VA_REQUIRE(vkCreateCommandPool(vgi.device, &cpci, vgi.allocator, _pool.prepare(vgi)));
    }

    ~SingleUseCommandPool() { finish(); }

    const VulkanGlobalInfo & vgi() const { return _vsp.vgi(); }

    CommandBuffer create(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) {
        auto cbai               = VkCommandBufferAllocateInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbai.level              = level;
        cbai.commandBufferCount = 1;
        cbai.commandPool        = _pool;
        VkCommandBuffer cb      = 0;
        PH_VA_REQUIRE(vkAllocateCommandBuffers(vgi().device, &cbai, &cb));
        auto cbbi = VkCommandBufferBeginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        _buffers.push_back(cb);
        PH_VA_REQUIRE(vkBeginCommandBuffer(cb, &cbbi));
        return {this, cb, CommandBuffer::BEGUN};
    }

    VkResult submit(CommandBuffer & cb) {
        PH_REQUIRE(cb.pool == this && CommandBuffer::BEGUN == cb.state);
        PH_VA_REQUIRE(vkEndCommandBuffer(cb.cb));
        VulkanSubmissionProxy::SubmitInfo si;
        si.commandBuffers = {&cb.cb, 1};
        cb.state          = CommandBuffer::SUBMITTED;
        auto r            = _vsp.submit(si);
        if (r >= 0) _pending = true;
        return r;
    }

    /// finish and reset all previously created command buffers.
    void finish(const char * deviceLostErrorPrompt = nullptr) {
        if (_pending) {
            // TODO: wait for fence
            _vsp.waitIdle(deviceLostErrorPrompt);
            _pending = {};
        }
        if (!_buffers.empty()) {
            PH_VA_CHK(vkResetCommandPool(vgi().device, _pool, 0), );
            vkFreeCommandBuffers(vgi().device, _pool, (uint32_t) _buffers.size(), _buffers.data());
            _buffers.clear();
        }
    }

    /// Submit a command buffer, wait for it, and everything submitted before it, to finish.
    void finish(CommandBuffer & cb, const char * deviceLostErrorPrompt = nullptr) {
        submit(cb);
        finish(deviceLostErrorPrompt);
    }

    /// Asynchronously running GPU work. No waiting for it to finish.
    template<typename FUNC>
    void exec(FUNC f) {
        auto cb = create();
        f(cb.cb);
        submit(cb);
    }

    /// Synchronously running GPU work. Wait for the GPU work to finish before returning.
    template<typename FUNC>
    void syncExec(FUNC f) {
        exec(f);
        finish();
    }

private:
    VulkanSubmissionProxy &      _vsp;
    AutoHandle<VkCommandPool>    _pool;
    std::vector<VkCommandBuffer> _buffers;
    bool                         _pending = false;
};

} // namespace va
} // namespace ph