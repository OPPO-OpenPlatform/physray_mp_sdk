/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include "rt/common/simpleApp.h"

#include "rt/cornell/cornell.h"
#include "rt/garage/garage.h"
#include "rt/ring/ring.h"
#include "rt/shadow/shadow.h"
#include "rt/suzanne/suzanne.h"
#include "rt/ptdemo/ptdemo.h"
#include "rt/war/war.h"
#include "rt/empty_app/empty_app.h"
#include "rt/triangle/triangle.h"
#include "rt/blue_triangle/blue_triangle.h"
#include "rt/refl/refl.h"

#include "drag-motion-controller.h"
#include "touch-event.h"

#include <android/asset_manager.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <functional>
#include <jni.h>
#include <utility>

class AndroidDemoApp : public SimpleApp {
public:
    struct ConstructParameters {
        std::string     name;
        ANativeWindow * win = nullptr;

        // set to true to disable ray tracing effect and fallback to rasterized rendering.
        bool rasterized = false;

        // If using HW ray query or not. Set to false to use custom software solution.
        bool rayQuery = true;

        // If started the demo with animation.
        int animated = 1;

        // Set to true to defer to VMA for device memory allocations.
        bool useVmaAllocator = true;
    };

    AndroidDemoApp(const ConstructParameters & cp): _cp(cp), _last({}) {
        // override ray query option based on system property
        auto rayQueryProp = getJediProperty("ray-query");
        if (!rayQueryProp.empty()) { _cp.rayQuery = rayQueryProp == "yes" || rayQueryProp == "1"; }

        construct({.icp =
                       {
                           .instanceExtensions =
                               {
                                   {VK_KHR_SURFACE_EXTENSION_NAME, true},
                                   {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, true},
                               },
                       },
                   .dcp =
                       {
                           .useVmaAllocator = cp.useVmaAllocator,
                       },
                   .backBufferFormat = VK_FORMAT_R8G8B8A8_UNORM,
                   .rayQuery         = _cp.rayQuery,
                   .createSurface =
                       [this](const ph::va::VulkanGlobalInfo & vgi) {
                           // create a surface out of the native window
                           ph::va::AutoHandle<VkSurfaceKHR> s;
                           VkAndroidSurfaceCreateInfoKHR    sci {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
                           sci.window = _cp.win;
                           PH_VA_REQUIRE(vkCreateAndroidSurfaceKHR(vgi.instance, &sci, vgi.allocator, s.prepare(vgi)));
                           return s;
                       },
                   .createScene =
                       [this](auto & app) {
                           if (_cp.name == "Cornell Box") {
                               return createScene<CornellBoxScene>(app);
                           } else if (_cp.name == "Shadow") {
                               return createScene<ShadowScene>(app);
                           } else if (_cp.name == "Suzanne") {
                               return createScene<SuzanneScene>(app);
                           } else if (_cp.name == "Rocket") {
                               return createScene<SuzanneScene>(app, [this](auto & o) { o.model = "model/the-rocket/the-rocket.glb"; });
                           } else if (_cp.name == "Helmet") {
                               return createScene<SuzanneScene>(app, [this](auto & o) { o.model = "model/damaged-helmet/damaged-helmet.gltf"; });
                           } else if (_cp.name == "Glasses") {
                               return createScene<SuzanneScene>(app, [this](auto & o) { o.model = "model/cat-eye-glasses.gltf"; });
                           } else if (_cp.name == "Ring") {
                               return createScene<OppoRingScene>(app);
                           } else if (_cp.name == "Garage") {
                               return createScene<GarageScene>(app);
                           } else if (_cp.name == "PTDemo") {
                               return createScene<PathTracerDemo>(app);
                           } else if (_cp.name == "War") {
                               return createScene<WarScene>(app);
                           } else if (_cp.name == "Empty") {
                               return createScene<EmptyScene>(app);
                           } else if (_cp.name == "Triangle") {
                               return createScene<TriangleScene>(app);
                           } else if (_cp.name == "Blue-tri") {
                               return createScene<BlueTriangleScene>(app);
                           } else if (_cp.name == "Refl") {
                               return createScene<ReflScene>(app);
                           } else {
                               PH_LOGE("Unrecognized demo name: %s.", _cp.name.c_str());
                               return (SimpleScene *) nullptr;
                           }
                       }});

        // Hardcoded to 720P for better perf, assuming the activity is in landscape orientation.
        uint32_t h = 600;
        uint32_t w = (uint32_t) (h * ANativeWindow_getWidth(_cp.win) / ANativeWindow_getHeight(_cp.win));

        // Give the motion controller the window it needs the dimensions of to track the pointer's relative position.
        _dragMotionController.setWindow(cp.win);

        // connect to scene loading signal (need to be done before resize())
        sceneLoaded.connect([this]() {
            // Give the motion controller the first person controller it is manipulating.
            _dragMotionController.setFirstPersonController(&scene<ModelViewer>().firstPersonController);

            // Since translation only takes up half the screen, set it to double the first person controller's default move
            // speed.
            _dragMotionController.setSpeedMultiplier(scene<ModelViewer>().firstPersonController.moveSpeed() * 2);
        });

        resize(_cp.win, w, h);
    }

    const ConstructParameters & cp() const { return _cp; }

    void handleTouchEvent(TouchEvent curr) {
        // Check for redundancy
        if (_last == curr) { return; }

        // PH_LOGI("touch events: %s", curr.toString().c_str());

        // process UI events first.
        if (!curr.empty()) {
            const Touch & touch = curr[0];
            handleAndroidSimpleTouchEvent(true, touch.x(), touch.y());
        } else if (!_last.empty()) {
            const Touch & touch = _last[0];
            handleAndroidSimpleTouchEvent(false, touch.x(), touch.y());
        } else {
            handleAndroidSimpleTouchEvent(false, 0, 0);
        }

        // Update the camera with drag motion controller
        _dragMotionController.onTouch(curr);

        // store the latest event.
        _last = curr;
    }

    /// return 1 if the event is processed.
    int32_t handleInputEvent(const AInputEvent * event) {
        // we only care about touch events.
        if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION || AInputEvent_getSource(event) != AINPUT_SOURCE_TOUCHSCREEN) { return 0; }

        // Compose touch event
        std::vector<Touch> touches;

        // Only cares about move and down events.
        auto action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        if (AMOTION_EVENT_ACTION_MOVE == action || AMOTION_EVENT_ACTION_DOWN == action) {
            int32_t count = AMotionEvent_getPointerCount(event);
            for (int i = 0; i < count; ++i) {
                auto id = AMotionEvent_getPointerId(event, i);
                auto x  = AMotionEvent_getX(event, i);
                auto y  = AMotionEvent_getY(event, i);
                touches.emplace_back(id, x, y);
            }
        }

        // Process the touch event
        handleTouchEvent({std::move(touches)});

        // done
        return 1;
    }

protected:
    VkExtent2D getWindowSize() override {
        return {
            (uint32_t) ANativeWindow_getWidth(_cp.win),
            (uint32_t) ANativeWindow_getHeight(_cp.win),
        };
    }

private:
    ConstructParameters  _cp;
    TouchEvent           _last;
    DragMotionController _dragMotionController;

private:
    template<class Scene = CornellBoxScene>
    SimpleScene * createScene(SimpleApp & app, std::function<void(typename Scene::Options &)> func = {}) {
        auto &                  sw = app.sw();
        typename Scene::Options o;
        if (_cp.rasterized) o.rpmode = Scene::Options::RenderPackMode::RAST;
        o.animated = _cp.animated;
        if (func) func(o);
        return new Scene(app, o);
    }
};
