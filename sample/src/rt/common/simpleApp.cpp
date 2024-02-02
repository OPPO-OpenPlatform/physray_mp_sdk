/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"

#include "simpleApp.h"

#include <stdlib.h>
#if PH_ANDROID
    #include <android/trace.h>
#endif

using namespace ph;
using namespace ph::va;

// ---------------------------------------------------------------------------------------------------------------------
//
AutoHandle<VkRenderPass> createRenderPass(const VulkanGlobalInfo & vgi, VkFormat colorFormat, bool clearColor, VkFormat depthFormat, bool clearDepth) {
    std::vector<VkAttachmentDescription> attachments;

    // color attachment
    attachments.push_back(VkAttachmentDescription {
        VkAttachmentDescriptionFlags(), // flags
        colorFormat,
        VK_SAMPLE_COUNT_1_BIT,
        clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE, // need to store the render result for presenting.
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        clearColor ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    });

    // depth attachment
    bool hasDepth = VK_FORMAT_UNDEFINED != depthFormat;
    if (hasDepth) {
        attachments.push_back(VkAttachmentDescription {
            VkAttachmentDescriptionFlags(),                                                            // flags
            depthFormat,                                                                               // format
            VK_SAMPLE_COUNT_1_BIT,                                                                     // samples
            clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,                     // load
            VK_ATTACHMENT_STORE_OP_STORE,                                                              // store
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,                                                           // stencil load
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                                                          // stencil store
            clearDepth ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // initial layout
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,                                          // final layout
        });
    }

    auto                 c0ref = VkAttachmentReference {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    auto                 dsref = VkAttachmentReference {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass {
        {},
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        nullptr, // inputs
        1,
        &c0ref,                      // colors
        nullptr,                     // resolve
        hasDepth ? &dsref : nullptr, // depth
    };
    VkSubpassDependency dependency {VK_SUBPASS_EXTERNAL,
                                    0,
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    0,
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    auto                ci = VkRenderPassCreateInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.pAttachments        = attachments.data();
    ci.attachmentCount     = (uint32_t) attachments.size();
    ci.pSubpasses          = &subpass;
    ci.subpassCount        = 1;
    ci.pDependencies       = &dependency;
    ci.dependencyCount     = 1;
    AutoHandle<VkRenderPass> pass;
    PH_VA_REQUIRE(vkCreateRenderPass(vgi.device, &ci, vgi.allocator, pass.prepare(vgi)));

    // done
    return pass;
}

// ---------------------------------------------------------------------------------------------------------------------
//
SimpleScene::SimpleScene(SimpleApp & app): _app(app) {}

// ---------------------------------------------------------------------------------------------------------------------
//
SimpleApp::~SimpleApp() {
    // if the loading thread is still running, wait for it to finish.
    if (_loading.valid()) { _loading.wait(); }

    // wait for idle before destruction.
    if (_dev && _dev->vgi().device) {
        PH_LOGI("Shutting down...wait for GPU to be idle.");
        threadSafeDeviceWaitIdle(_dev->vgi().device);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
/// Construct the app. Should be called once and only once in subclass's constructor.
/// The reason we don't put it into SimpleApp() is because subclass usually need to
/// do some extra preparation (like initialize window system), before calling this.
void SimpleApp::construct(const ConstructParameters & cp) {
    _cp = cp;

    if (_cp.dcp.instance) {
        // use external VK instance.
        _cp.rayQuery = _cp.dcp.setupForRayQuery(_cp.rayQuery);
    } else {
        // Override validation level using environment/system variable.
#if PH_ANDROID
        auto validationLevel = getJediProperty("validation");
#else
        auto validationLevel = getJediEnv("validation");
#endif
        if (!validationLevel.empty()) {
            try {
                int i = std::stoi(validationLevel);
                if (0 <= i && i <= SimpleVulkanInstance::THROW_ON_VK_ERROR) { _cp.icp.validation = (SimpleVulkanInstance::Validation) i; }
            } catch (...) {
                // do nothing.
            }
        }

        // Setup constructions parameters for ray query.
        _cp.rayQuery = _cp.dcp.setupForRayQuery(_cp.rayQuery);

        // create instance
        _inst.reset(new SimpleVulkanInstance(_cp.icp));
        _cp.dcp.instance = _inst.get();
    }

    // create device
    _dev.reset(new SimpleVulkanDevice(_cp.dcp));

    // create surface
    PH_REQUIRE(_cp.createSurface);
    const auto & vgi = _dev->vgi();
    _surface         = _cp.createSurface(vgi);

    _gpuTimestamps.reset(new AsyncTimestamps({_dev->graphicsQ()}));

    // store initial window size
    PH_LOGI("[SimpleApp] constructed.");
}

// ---------------------------------------------------------------------------------------------------------------------
//
void SimpleApp::resize(void * window, uint32_t w, uint32_t h) {
    PH_REQUIRE(_cp.createScene);

    // make sure nothing is pending
    const auto & vgi = _dev->vgi();
    threadSafeDeviceWaitIdle(vgi.device);

    // if the loading thread is still running, wait for it to finish.
    if (_loading.valid()) { _loading.wait(); }

    // call pre-resizing method of the scene. This is to give the scene a chance to release any data associated with
    // the current swapchain.
    if (_scene) _scene->resizing();

    // (Re)create swapchain
    // On MTK1200, 3 back buffers give us best perf with CPU and GPU runtime overlapped with each other.
    const uint32_t BACKBUFFER_COUNT    = 3;
    const uint32_t MAX_IN_FLIGH_FRAMES = 2; // this need to be less than number of back buffers.
    auto           presentQueue        = _dev->searchForPresentQ(_surface);
    PH_REQUIRE(presentQueue);
    _sw.reset(); // has to release old swapchain before creating new one. Or else, the creation function will fail.
    _sw.reset(ph::va::SimpleSwapchain::create(
        {*presentQueue, window, _surface, _cp.backBufferFormat, w, h, BACKBUFFER_COUNT, _cp.vsync, _dev->graphicsQ().queueFamilyIndex()}));
    PH_ASSERT(_sw->initParameters().width == w);
    PH_ASSERT(_sw->initParameters().height == h);

    // (Re)create render loop
    _loop.reset(new ph::va::SimpleRenderLoop({*_dev, *_sw, MAX_IN_FLIGH_FRAMES}));

    // create render pass used to render UI
    _renderPass = createRenderPass(dev().vgi(), _cp.backBufferFormat, true);
    PH_REQUIRE(_renderPass);

    // Create frame buffer object the current back buffer, if not created already.
    _framebuffers.resize(_sw->backBufferCount());
    for (size_t i = 0; i < _framebuffers.size(); ++i) {
        auto &      bb = _sw->backBuffer(i);
        VkImageView views[] {bb.view};
        auto        ci = va::util::framebufferCreateInfo(_renderPass, std::size(views), views, w, h);
        PH_VA_REQUIRE(vkCreateFramebuffer(vgi.device, &ci, vgi.allocator, _framebuffers[i].prepare(vgi)));
    }

    // Must release old UI instance before creating new one. Or else, the destructor of SimpleUI class will
    // reset and clear the global ImGui context.
    _ui.reset();

    // create UI
    _ui.reset(new SimpleUI({
        .vsp               = _dev->graphicsQ(),
        .window            = _sw->initParameters().window,
        .width             = w,
        .height            = h,
        .maxInFlightFrames = _loop->cp().maxInFlightFrames,
    }));

    // create/resize scene in a background thread to avoid blocking the main thread.
    _loaded  = false;
    _loading = std::async(std::launch::async, [this, w = w, h = h]() {
        auto logCallback = registerLogCallback({staticLogCallback, this});
        auto scopeExit   = ScopeExit([&] { unregisterLogCallback(logCallback); });
        // Do not re-create the scene, only the FBO if we resize or change the surface
        // Scene can be complex and have a lot of resources so we want to avoid to re-upload
        // and re-initialize everything constantly
        if (!_scene) { _scene.reset(_cp.createScene({*this})); }
        _scene->resized();
        PH_LOGI("[SimpleApp] resized to %dx%d.", w, h);

        _loaded = true;

        // fire the scene loaded signal.
        sceneLoaded();
    });

    // For offsreen rendering, we'll wait for loading to finish.
    if (_cp.offscreen || !_cp.asyncLoading) {
        _loading.wait();
        PH_ASSERT(_loaded);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
bool SimpleApp::render() {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    // do nothing if the scene is not ready
    if (_tickError) { return false; }

    // for offscreen app, the scene should already be loaded by the time render() is called.
    PH_ASSERT(!_cp.offscreen || _loaded);

    // update the scene
    if (_loaded) {
        _cpuFrameTimes.begin("update");

        // update game time.
        auto now       = high_resolution_clock::now();
        auto elapsed   = _firstFrame ? 0ms : duration_cast<microseconds>(now - _lastFrameTime);
        _lastFrameTime = now;

        // If min frame rate is a positive number.
        if (_cp.minFrameRate > 0) {
            // Calculate the maximum amount of time that is allowed to elapse each frame from the min frame rate.
            auto maxTimeElapsed = duration_cast<microseconds>(duration<float>(1.0f / _cp.minFrameRate));

            // Make sure elapsed time is not bigger than the min frame rate.
            elapsed = std::min(elapsed, maxTimeElapsed);
        }

        // If we have a max frame rate.
        if (std::isfinite(_cp.maxFrameRate)) {
            // Calculate the minimum amount of time that needs to elapse from the max frame rate.
            auto minTimeElapsed = duration_cast<microseconds>(duration<float>(1.0f / _cp.maxFrameRate));

            // Make sure elapsed time is not smaller than max frame rate.
            elapsed = std::max(minTimeElapsed, elapsed);
        }

        if (_firstFrame) {
            _firstFrame              = false;
            _gameTime.sinceBeginning = 0ms;
        } else if (_scene->animated()) {
            _gameTime.sinceBeginning += elapsed;
        }

        _gameTime.sinceLastUpdate = elapsed;
        _scene->update();
        _cpuFrameTimes.end();
    }

    // render the scene (or loading screen)
    if (!_loop->tick([this](const ph::va::SimpleRenderLoop::RecordParameters & rp) {
            VkImageLayout finalLayout;
            if (_loaded) {
                _cpuFrameTimes.begin("record");
                _gpuTimestamps->refresh(rp.cb); // refresh timestamp value once per frame.
                finalLayout = _scene->record(rp);
                _cpuFrameTimes.end();
                _cpuFrameTimes.frame();
            } else {
                recordLoadingScreen(rp);
                finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
            return finalLayout;
        })) {
        _tickError = true;
        return false;
    }

    // done
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void SimpleApp::recordLoadingScreen(const ph::va::SimpleRenderLoop::RecordParameters & rp) {
    // begin the render pass
    VkClearColorValue clearColor = {{52.0f / 256.0f, 128.0f / 256.0f, 235.0f / 256.0f, 1.0f}};
    VkClearValue      clearValues[1];
    clearValues[0].color       = clearColor;
    VkRenderPassBeginInfo info = {};
    info.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass            = _renderPass;
    info.framebuffer           = _framebuffers[rp.backBufferIndex];
    info.renderArea.extent     = _sw->backBuffer(rp.backBufferIndex).extent;
    info.clearValueCount       = 1;
    info.pClearValues          = clearValues;
    vkCmdBeginRenderPass(rp.cb, &info, VK_SUBPASS_CONTENTS_INLINE);

    // determine log window size
    ImVec2 extent = {(float) _sw->initParameters().width, (float) _sw->initParameters().height};
#if PH_ANDROID
    // UI is magnified 2x on Android.
    extent.x /= 2.f;
    extent.y /= 2.f;
#endif

    // render UI
    _ui->record({_renderPass, rp.cb,
                 [&](void *) {
                     ImGui::SetNextWindowPos(ImVec2(0, 0));
                     ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
                     ImGui::SetNextWindowBgAlpha(0.3f);
                     if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                         ImGui::BeginChild(" ", extent, false);
                         auto lock = std::lock_guard(_logMutex);
                         for (const auto & r : _logRecords) { ImGui::TextUnformatted(r.text.c_str()); }
                         ImGui::SetScrollHereY(1.0);
                         ImGui::EndChild();
                     }
                     ImGui::End();
                 },
                 this});

    // end of main color pass
    vkCmdEndRenderPass(rp.cb);
}