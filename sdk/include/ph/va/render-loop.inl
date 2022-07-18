// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph::va {

// ---------------------------------------------------------------------------------------------------------------------
//
class SimpleRenderLoop {
private:
    /// define resources per in-flight frame.
    struct Frame {
        std::string                       name; // name of the frame (for debugging only)
        VkDevice                          device = 0;
        ph::va::AutoHandle<VkSemaphore>   renderFinished;
        ph::va::AutoHandle<VkFence>       fence;
        ph::va::AutoHandle<VkCommandPool> pool;
        std::vector<VkCommandBuffer>      commandBuffers;
        size_t                            nextIdle = 0;

        PH_NO_COPY(Frame);
        PH_NO_MOVE(Frame);

        Frame() = default;

        ~Frame() {
            clear();
            PH_LOGI("[SimpleRenderLoop::Frame] destroyed.");
        }

        void init(const ph::va::VulkanGlobalInfo & vgi, uint32_t queueFamilyIndex, const char * name_);

        void clear();

        void resetCommandPool() {
            vkResetCommandPool(device, pool, 0);
            nextIdle = 0;
        }

        void acquireCommandBuffers(VkCommandBuffer * data, size_t size);
    };

public:
    PH_NO_COPY(SimpleRenderLoop);

    struct ConstructParameters {
        SimpleVulkanDevice & dev;
        SimpleSwapchain &    sw;

        // Defines max number of frames in flight. Minimal allowed value is 2. One for CPU recording, one for GPU
        // rendering.
        uint32_t maxInFlightFrames = 2;

        /// When set to false, FrameDuration.gpu will return undefined value.
        bool gatherGpuTimestamp = true;
    };

    SimpleRenderLoop(const ConstructParameters & cp);

    ~SimpleRenderLoop();

    const ConstructParameters & cp() const { return _cp; }

    struct RecordParameters {
        VkCommandBuffer cb {};
        uint32_t        backBufferIndex = 0;
    };
    typedef std::function<void(const RecordParameters &)> RecordFunction;

    /// \return If false, then either there's an error, or someone has called requestForQuit(). Either way,
    ///         we need to quit the rendering loop and stop calling tick() method again.
    bool tick(RecordFunction rec);

    /// Request to quit after the end of current frame. This will cause the next tick() call returns false.
    void requestForQuit() { _running = false; };

    struct FrameDuration {
        NumericalAverager<uint64_t> all; ///< Frame duration (nanoseconds) that includes both CPU and GPU. Use this one to calculate FPS.
        NumericalAverager<uint64_t> cpu; ///< CPU busy time in nanoseconds, excluding present/wait-for-gpu time.
        NumericalAverager<uint64_t> gpu; ///< GPU busy time in nanoseconds.
    };

    const FrameDuration & frameDuration() const { return _frameDuration; }
    uint64_t              frameCounter() const { return _frameCounter; }

private:
    typedef std::chrono::high_resolution_clock::time_point FrameTimePoint;

    ConstructParameters                      _cp;
    bool                                     _running = true;
    ph::Blob<Frame>                          _frames;
    size_t                                   _activeFrame = 0;
    std::vector<VkFence>                     _imageFences;
    FrameDuration                            _frameDuration {};
    FrameTimePoint                           _lastFrameTime;
    uint64_t                                 _frameCounter = 0;
    std::unique_ptr<ph::va::AsyncTimestamps> _gpuTimestamps;
};

} // namespace ph::va