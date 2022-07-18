#pragma once

#include <ph/va.h>

#include "ui.h"

#include <sigslot/signal.hpp>
#include <list>
#include <moodycamel/readerwriterqueue.h>
#include <set>

class SimpleApp;

// ---------------------------------------------------------------------------------------------------------------------
//
struct SimpleScene {

    PH_NO_COPY(SimpleScene);
    PH_NO_MOVE(SimpleScene);

    /// scene construction parameters
    struct ConstructParameters {
        SimpleApp & app;
        bool        animated               = true;
        bool        clearColorOnMainPass   = false;
        bool        clearDepthOnMainPass   = false;
        bool        showUI                 = true;
        bool        showFrameTimeBreakdown = true;
    };

    /// constructor
    explicit SimpleScene(const ConstructParameters & cp);

    /// destructor
    virtual ~SimpleScene() { PH_LOGI("[SimpleScene] destroyed."); }

    const ConstructParameters &  cp() const { return _cp; }
    SimpleApp &                  app() const { return _cp.app; }
    ph::va::SimpleVulkanDevice & dev() const;
    ph::va::SimpleSwapchain &    sw() const;
    ph::va::SimpleRenderLoop &   loop() const;

    VkRenderPass mainColorPass() const { return _colorPass; }

    /// frame timing data
    struct FrameTiming {
        /// accumulated game time minor paused time
        std::chrono::microseconds sinceBeginning {};

        /// Elapsed duration from last frame. This duration is not affected by pause/resume.
        std::chrono::microseconds sinceLastUpdate {};
    };

    virtual void resize();

    /// Main per-frame entry point to update the scene states. Called once per frame.
    virtual const FrameTiming & update();

    /// Get the latest frame timing information
    const FrameTiming & frameTiming() const { return _frameTiming; }

    // returns pause time in us
    uint64_t pauseTime() const {
        return (_cp.animated) ? 0 : uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(_lastFrameTime - _pausingTime).count());
    }

    /// Data preparation that only relies on transfer operations
    virtual void prepare(VkCommandBuffer) {}

    /// Main per-frame entry point that records graphics commands into the frame's command buffer.
    /// The final layout of the back buffer must be in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR (needed for presenting)
    virtual void record(const ph::va::SimpleRenderLoop::RecordParameters & rp);

    bool animated() const { return _cp.animated; }

    void setAnimated(bool b) {
        using namespace std::chrono;
        if (b == _cp.animated) return;
        _cp.animated = b;
        if (_cp.animated) {
            // resuming
            _startTime += high_resolution_clock::now() - _pausingTime;
        } else {
            // pausing
            _pausingTime = high_resolution_clock::now();
        }
    }

    void toggleAnimated() { setAnimated(!_cp.animated); }

    virtual void onKeyPress(int key, bool down) {
        (void) key;
        (void) down;
    }

    virtual void onMouseMove(float x, float y) {
        (void) x;
        (void) y;
    }

    virtual void onMouseWheel(float delta) { (void) delta; }

    // CPU frame times
    ph::SimpleCpuFrameTimes cpuFrameTimes;

protected:
    struct PassParameters {
        VkCommandBuffer                             cb {};
        const ph::va::SimpleSwapchain::BackBuffer & bb;
        VkImageView                                 depthView {};
        // ShadowRenderOptions                 shadowOptions;
    };

    /// Custom render pass(es) called before the main render pass. Override this method
    /// to do customized rendering to offscreen framebuffers.
    virtual void recordOffscreenPass(const PassParameters &) {}

    /// Main color pass. Render to default frame buffer.
    /// TODO: 1) allow customized subpass. 2) depth-stencil support.
    virtual void recordMainColorPass(const PassParameters &) {}

    /// Override this method to customize UI rendering.
    virtual void drawUI();

    /// Override this method to customize UI.
    virtual void describeImguiUI() {}

    /// Clear color buffer to black by default.
    VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};

    /// Clear value for depth and stencil buffer.
    VkClearDepthStencilValue clearDepthStencil = {1.0f, 0};

    /// async time stamp queries
    std::unique_ptr<ph::va::AsyncTimestamps> gpuTimestamps;

private:
    struct FrameBuffer {
        ph::va::AutoHandle<VkFramebuffer> colorFb; ///< frame buffer with color buffer only.
        // ph::va::ImageObject               depth;
        // ph::va::AutoHandle<VkImage>       stencilView;
        // ph::va::AutoHandle<VkRenderPass>  colorDepthPass;
        // ph::va::AutoHandle<VkRenderPass>  colorDepthStencilPass;
    };
    ConstructParameters                            _cp;
    FrameTiming                                    _frameTiming;
    std::chrono::high_resolution_clock::time_point _startTime     = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point _lastFrameTime = _startTime;
    std::chrono::high_resolution_clock::time_point _pausingTime;
    ph::va::AutoHandle<VkRenderPass>               _colorPass; // render pass with only color buffer only.
    VkFormat                                       _colorTargetFormat = VK_FORMAT_UNDEFINED;
    std::vector<FrameBuffer>                       _frameBuffers; // one for each back buffer image
    ph::va::ImageObject                            _depthBuffer;  // main depth and stencil buffer

    void recreateColorRenderPass();
};

// ---------------------------------------------------------------------------------------------------------------------
/// Simple app framework for both Desktop and Android environment.
class SimpleApp {
public:
    typedef std::function<SimpleScene *(SimpleApp &)> SceneCreator;

    // This is the method that (re)creates swap chain and render loop. The app will do
    // nothing unless this method is called at least once.
    void resize(void * window, uint32_t w, uint32_t h);

    bool render();

    template<typename T = SimpleScene>
    T & scene() const {
        PH_ASSERT(_scene);
        return (T &) *_scene;
    }

    typedef std::function<ph::va::AutoHandle<VkSurfaceKHR>(const ph::va::VulkanGlobalInfo &)> SurfaceCreator;
    typedef std::function<VkExtent2D()>                                                       GetSurfaceSize;
    struct ConstructParameters {
        typedef ph::va::SimpleVulkanInstance::ConstructParameters InstanceConstructParameters;
        typedef ph::va::SimpleVulkanDevice::ConstructParameters   DeviceConstructParameters;

        InstanceConstructParameters icp;
        DeviceConstructParameters   dcp;
        VkFormat                    backBufferFormat;
        bool                        rayQuery  = true;
        bool                        gpuBvh    = false;
        bool                        offscreen = false;
        bool                        vsync     = true;
        SurfaceCreator              createSurface;
        SceneCreator                createScene;
    };

    const ph::va::SimpleVulkanInstance & instance() const { return *_cp.dcp.instance; }

    const ConstructParameters & cp() const { return _cp; }

    ph::va::SimpleVulkanDevice & dev() const {
        PH_ASSERT(_dev);
        return *_dev;
    }

    ph::va::SimpleSwapchain & sw() const {
        PH_ASSERT(_sw);
        return *_sw;
    }

    ph::va::SimpleRenderLoop & loop() const {
        PH_ASSERT(_loop);
        return *_loop;
    }

    /// could be null if UI is disabled.
    SimpleUI & ui() const {
        PH_ASSERT(_ui);
        return *_ui;
    }

protected:
    SimpleApp() = default;

    virtual ~SimpleApp();

    void construct(const ConstructParameters & cp);

    virtual VkExtent2D getWindowSize() = 0;

    void onKeyPress(int key, bool down) {
        if (_scene && _loaded) { _scene->onKeyPress(key, down); }
    }

    void onMouseMove(float x, float y) {
        if (_scene && _loaded) { _scene->onMouseMove(x, y); }
    }

    void onMouseWheel(float delta) {
        if (_scene && _loaded) { _scene->onMouseWheel(delta); }
    }

    /// The signal fired after the scene is fully loaded.
    sigslot::signal<> sceneLoaded;

#if PH_ANDROID
    void handleAndroidSimpleTouchEvent(bool down, float x, float y) { _ui->handleAndroidSimpleTouchEvent(down, x, y); }
#endif

private:
    ConstructParameters                            _cp;
    std::unique_ptr<ph::va::SimpleVulkanInstance>  _inst;
    std::unique_ptr<ph::va::SimpleVulkanDevice>    _dev;
    ph::va::AutoHandle<VkSurfaceKHR>               _surface; // this could be null, then doing offscreen rendering.
    std::unique_ptr<ph::va::SimpleSwapchain>       _sw;
    std::unique_ptr<ph::va::SimpleRenderLoop>      _loop;
    std::unique_ptr<SimpleScene>                   _scene;
    ph::va::AutoHandle<VkRenderPass>               _renderPass; // render pass used to render loading screen.
    std::vector<ph::va::AutoHandle<VkFramebuffer>> _framebuffers;
    std::unique_ptr<SimpleUI>                      _ui;
    bool                                           _tickError = false;
    std::future<void>                              _loading;
    std::atomic<bool>                              _loaded = false;

    // data members for rendering log text
    struct LogRecord {
        ph::LogDesc desc;
        std::string text;
    };
    std::list<LogRecord> _logRecords;
    std::mutex           _logMutex;

private:
    void recordLoadingScreen(const ph::va::SimpleRenderLoop::RecordParameters & rp);

    static void staticLogCallback(void * context, const ph::LogDesc & desc, const char * text) { ((SimpleApp *) context)->logCallback(desc, text); }

    void logCallback(const ph::LogDesc & desc, const char * text) {
        // keep the last 100 lines of log
        auto lock = std::lock_guard(_logMutex);
        _logRecords.insert(_logRecords.end(), {desc, text});
        if (_logRecords.size() > 100) { _logRecords.erase(_logRecords.begin()); }
    }
};

inline ph::va::SimpleVulkanDevice & SimpleScene::dev() const { return cp().app.dev(); }
inline ph::va::SimpleSwapchain &    SimpleScene::sw() const { return cp().app.sw(); }
inline ph::va::SimpleRenderLoop &   SimpleScene::loop() const { return cp().app.loop(); }
