/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/va.h>

#include "ui.h"

#include <list>
#include <moodycamel/readerwriterqueue.h>
#include <set>

class SimpleApp;

ph::va::AutoHandle<VkRenderPass> createRenderPass(const ph::va::VulkanGlobalInfo & vgi, VkFormat colorFormat, bool clearColor,
                                                  VkFormat depthFormat = VK_FORMAT_UNDEFINED, bool clearDepth = true);

// ---------------------------------------------------------------------------------------------------------------------
/// simple in game clock
struct SimpleGameTime {
    /// accumulated game time minors paused time
    std::chrono::microseconds sinceBeginning {};

    /// Elapsed duration from last frame. This duration does NOT affected by pause and resume.
    std::chrono::microseconds sinceLastUpdate {};
};

// ---------------------------------------------------------------------------------------------------------------------
//
struct SimpleScene {

    PH_NO_COPY(SimpleScene);
    PH_NO_MOVE(SimpleScene);

    /// constructor
    explicit SimpleScene(SimpleApp & app);

    /// destructor
    virtual ~SimpleScene() {}

    SimpleApp &                  app() const { return _app; }
    ph::va::SimpleVulkanDevice & dev() const;
    ph::va::SimpleSwapchain &    sw() const;
    ph::va::SimpleRenderLoop &   loop() const;

    /// This method is called when SimpleApp is about to recreate the swapchain with new dimension.
    /// This is the chance to release any data that is associated with the old swapchain images.
    virtual void resizing() {}

    /// This method is called after SimpleApp finishes resizing/recreating the swapchain.
    virtual void resized() {}

    /// Main per-frame entry point to update/animate the scene. Called once per frame when the scene
    /// is animated.
    virtual void update() {}

    /// Main per-frame entry point that records graphics commands into the frame's command buffer.
    /// \return The final layout of the back buffer.
    virtual VkImageLayout record(const ph::va::SimpleRenderLoop::RecordParameters &) = 0;

    bool animated() const { return _animated; }

    void setAnimated(bool b) {
        using namespace std::chrono;
        if (b == animated()) return;
        _animated = b;
    }

    void toggleAnimated() { setAnimated(!animated()); }

    virtual void onKeyPress(int key, bool down) {
        (void) key;
        (void) down;
    }

    virtual void onMouseMove(float x, float y) {
        (void) x;
        (void) y;
    }

    virtual void onMouseWheel(float delta) { (void) delta; }

private:
    SimpleApp & _app;
    bool        _animated = true;
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
        bool                        rayQuery     = true;
        bool                        offscreen    = false;
        bool                        vsync        = true;
        bool                        asyncLoading = true;

        /// Minimum number of frames per second.
        /// If the actual frame rate is lower, then it will clamp delta
        /// time to match the minimum frame rate, preventing animations and
        /// the like from progressing too quickly.
        float minFrameRate = 10.0f;

        /// Minimum number of frames per second.
        /// If the actual frame rate is higher, then it will clamp delta
        /// time to match the maximum frame rate, preventing animations and
        /// the like from progressing too slowly.
        /// Default value is infinity, meaning this will allow frames
        /// be as short as they like.
        float maxFrameRate = std::numeric_limits<float>::infinity();

        SurfaceCreator createSurface;
        SceneCreator   createScene;
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

    ph::SimpleCpuFrameTimes & cpuTimes() const { return _cpuFrameTimes; }
    ph::va::AsyncTimestamps & gpuTimes() const { return *_gpuTimestamps; }
    const SimpleGameTime &    gameTime() const { return _gameTime; }

protected:
    SimpleApp() {
#if PH_BUILD_DEBUG
        ph::registerSignalHandlers();
#endif
    }

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
    ph::sigslot::signal<> sceneLoaded;

#if PH_ANDROID
    void handleAndroidSimpleTouchEvent(bool down, float x, float y) { _ui->handleAndroidSimpleTouchEvent(down, x, y); }
#endif

private:
    ConstructParameters                            _cp;
    std::unique_ptr<ph::va::SimpleVulkanInstance>  _inst;
    std::unique_ptr<ph::va::SimpleVulkanDevice>    _dev;
    ph::va::AutoHandle<VkSurfaceKHR>               _surface; // null when doing offscreen rendering.
    std::unique_ptr<ph::va::SimpleSwapchain>       _sw;
    std::unique_ptr<ph::va::SimpleRenderLoop>      _loop;
    std::unique_ptr<SimpleScene>                   _scene;
    ph::va::AutoHandle<VkRenderPass>               _renderPass;   // render pass used to render loading screen.
    std::vector<ph::va::AutoHandle<VkFramebuffer>> _framebuffers; // frame buffers used to render loading screen.
    std::unique_ptr<SimpleUI>                      _ui;
    mutable ph::SimpleCpuFrameTimes                _cpuFrameTimes;
    std::unique_ptr<ph::va::AsyncTimestamps>       _gpuTimestamps;
    SimpleGameTime                                 _gameTime;
    std::chrono::high_resolution_clock::time_point _lastFrameTime = std::chrono::high_resolution_clock::now();
    bool                                           _firstFrame    = true;
    bool                                           _tickError     = false;
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

inline ph::va::SimpleVulkanDevice & SimpleScene::dev() const { return app().dev(); }
inline ph::va::SimpleSwapchain &    SimpleScene::sw() const { return app().sw(); }
inline ph::va::SimpleRenderLoop &   SimpleScene::loop() const { return app().loop(); }
