/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph {
namespace va {

// ---------------------------------------------------------------------------------------------------------------------
//
class SimpleRenderLoop : public DeferredHostOperation {
private:
    /// define resources per in-flight frame.
    struct Frame {
        std::string                        name; // name of the frame (for debugging only)
        VkDevice                           device = 0;
        ph::va::AutoHandle<VkSemaphore>    renderFinished;
        ph::va::AutoHandle<VkFence>        fence;
        ph::va::AutoHandle<VkCommandPool>  pool;
        std::vector<VkCommandBuffer>       commandBuffers;
        size_t                             nextIdle     = 0;
        int64_t                            frameCounter = -1;
        std::vector<std::function<void()>> deferredJobs;

        PH_NO_COPY_NO_MOVE(Frame);

        Frame() = default;

        ~Frame() {
            clear();
            PH_LOGV("[SimpleRenderLoop::Frame] destroyed.");
        }

        void init(const ph::va::VulkanGlobalInfo & vgi, uint32_t queueFamilyIndex, const char * name_);

        void clear();

        void resetCommandPool();

        void acquireCommandBuffers(VkCommandBuffer * data, size_t size);
    };

public:
    struct ConstructParameters {
        SimpleVulkanDevice & dev;
        SimpleSwapchain &    sw;

        // Defines max number of frames in flight. Minimal allowed value is 2. One for CPU recording, one for GPU
        // rendering.
        uint32_t maxInFlightFrames = 2;

        /// When set to false, FrameDuration.gpu will return undefined value.
        bool gatherGpuTimestamp = true;
    };

    struct RecordParameters {
        VkCommandBuffer cb {};
        uint32_t        backBufferIndex = 0;
    };

    /// Returns the final layout of the back buffer image.
    typedef std::function<VkImageLayout(const RecordParameters &)> RecordFunction;

    struct FrameDuration {
        NumericalAverager<uint64_t> all; ///< Frame duration (nanoseconds) that includes both CPU and GPU. Use this one to calculate FPS.
        NumericalAverager<uint64_t> cpu; ///< CPU busy time in nanoseconds, excluding present/wait-for-gpu time.
        NumericalAverager<uint64_t> gpu; ///< GPU busy time in nanoseconds.
    };

public:
    PH_NO_COPY(SimpleRenderLoop);

    SimpleRenderLoop(const ConstructParameters & cp);

    ~SimpleRenderLoop();

    void deferUntilGPUWorkIsDone(std::function<void()> func) override;

    const ConstructParameters & cp() const { return _cp; }

    /// \return If false, then either there's an error, or someone has called requestForQuit(). Either way,
    ///         we need to quit the rendering loop and stop calling tick() method again.
    bool tick(RecordFunction rec);

    /// Request to quit after the end of current frame. This will cause the next tick() call returns false.
    void requestForQuit() { _running = false; };

    const FrameDuration & frameDuration() const { return _frameDuration; }
    int64_t               frameCounter() const { return _frameCounter; }
    int64_t               safeFrame() const { return _safeFrame; } ///< Return counter of the latest frame of which the GPU work is completely done.

private:
    typedef std::chrono::high_resolution_clock::time_point FrameTimePoint;

    ConstructParameters              _cp;
    bool                             _running = true;
    MutableRange<Frame>              _frames;
    size_t                           _activeFrame = 0;
    std::vector<VkFence>             _imageFences;
    FrameDuration                    _frameDuration {};
    FrameTimePoint                   _lastFrameTime;
    int64_t                          _frameCounter = 0;
    int64_t                          _safeFrame    = -1;
    std::unique_ptr<AsyncTimestamps> _gpuTimestamps;
};

} // namespace va
} // namespace ph