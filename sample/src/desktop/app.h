/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "../rt/common/simpleApp.h"
#include "../rt/common/first-person-controller.h"
#include "../rt/common/recorder.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <regex>

using namespace ph::va;

// ---------------------------------------------------------------------------------------------------------------------
//
class DesktopApp : public SimpleApp {
public:
    struct Options {
        uint32_t width           = 0;
        uint32_t height          = 0;
        bool     vsync           = false;
        bool     rayQuery        = true;
        bool     offscreen       = false;
        bool     useVmaAllocator = true;
        bool     asyncLoading    = true;
        bool     breakOnVkError  = false;

        /// If set to a folder path, this will output the app's screen to a series of images.
        std::string recordPath = "";

        /// Specify from which frame the recording starts.
        uint32_t recordStartFrame = 0;

        /// If greater than 0, then quit the app after recording certain number of frames.
        /// Set t0 0 to record indefinitely. Let the scene decide when to quit.
        uint32_t recordFrameCount = 1;

        float minFrameRate = 10.0f;
        float maxFrameRate = std::numeric_limits<float>::infinity();
    };

    DesktopApp(const Options & o, SceneCreator sc): _options(o) {
        std::map<std::string, bool> instanceExtensions;
        if (!o.offscreen) {
            // initialize GLFW
            PH_REQUIRE(glfwInit());

            // create an window. no need to destroy it explicitly. it'll get destroyed by glfwTerminate().
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            _window = glfwCreateWindow(o.width, o.height, "ph-sample-app", nullptr, nullptr);

            // setup extension list for the window system.
            uint32_t extcount = 0;
            auto     required = glfwGetRequiredInstanceExtensions(&extcount);
            for (uint32_t i = 0; i < extcount; ++i) { instanceExtensions[required[i]] = true; }
        }
        auto validation = o.breakOnVkError ? SimpleVulkanInstance::BREAK_ON_VK_ERROR
                          : PH_BUILD_DEBUG ? SimpleVulkanInstance::LOG_ON_VK_ERROR_WITH_CALL_STACK
                                           : SimpleVulkanInstance::VALIDATION_DISABLED;
        construct({
            {
                .instanceExtensions = instanceExtensions,
                .validation         = validation,
            },
            {
                .useVmaAllocator = o.useVmaAllocator,
            },
            VK_FORMAT_B8G8R8A8_UNORM,
            o.rayQuery,
            o.offscreen,
            o.vsync,
            o.asyncLoading,
            o.minFrameRate,
            o.maxFrameRate,
            [&](const ph::va::VulkanGlobalInfo & vgi) {
                ph::va::AutoHandle<VkSurfaceKHR> s;
                if (!o.offscreen) { PH_VA_REQUIRE(glfwCreateWindowSurface(vgi.instance, _window, vgi.allocator, s.prepare(vgi))); }
                return s;
            },
            sc,
        });

        // Pass recording path if any.
        if (o.recordPath.size() > 0) { _recorder.setOutputPath(o.recordPath); }
    }

    void run() {

        if (_options.offscreen) {
            resize(nullptr, _options.width, _options.height);
            bool running = true;
            while (running) {
                running &= render();
                running &= recordFrame(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
            return;
        }

        // a little hack to make run parameters available to GLFW callbacks.
        static DesktopApp * p = this;
        glfwSetCursorPosCallback(_window, [](GLFWwindow * window, double x, double y) {
            int w, h;
            glfwGetWindowSize(window, &w, &h);
            glfwSetWindowTitle(window, ph::formatstr("mouse: (%d, %d)", (int) x, h - (int) y));
            p->onMouseMove((float) x, (float) y);
        });

        glfwSetScrollCallback(_window, [](GLFWwindow *, double, double yoffset) { p->onMouseWheel((float) yoffset); });

        glfwSetKeyCallback(_window, [](GLFWwindow *, int key, int, int action, int) {
            if (GLFW_PRESS == action) {
                p->onKeyPress(key, true);
            } else if (GLFW_RELEASE == action) {
                p->onKeyPress(key, false);
            }
        });

        glfwSetMouseButtonCallback(_window, [](GLFWwindow *, int button, int action, int) {
            if (GLFW_PRESS == action) {
                p->onKeyPress(button, true);
            } else if (GLFW_RELEASE == action) {
                p->onKeyPress(button, false);
            }
        });

        bool       running    = true;
        VkExtent2D windowsize = {0, 0};
        while (!glfwWindowShouldClose(_window) && running) {
            // get current window size
            auto newWidnowSize = getWindowSize();

            // check if window is minimized.
            bool minimized = (0 == newWidnowSize.width) || (0 == newWidnowSize.height);

            // skip the frame when minimized.
            if (minimized) {
                // Window minimized. Skip the frame. Wait for the window to be restored.
                // Throttle the event loop to decrease CPU workload when minimized.
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(0.1s);
            } else {
                // deal with possible window resizing
                if (newWidnowSize.width != windowsize.width || newWidnowSize.height != windowsize.height) {
                    resize(_window, newWidnowSize.width, newWidnowSize.height);
                    windowsize = newWidnowSize;
                }
                running = render();
                recordFrame(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            }

            // poll window system events
            glfwPollEvents();
        }
    }

protected:
    VkExtent2D getWindowSize() override {
        VkExtent2D r;
        if (!_options.offscreen) {
            glfwGetWindowSize(_window, (int *) &r.width, (int *) &r.height);
        } else {
            r.width  = _options.width;
            r.height = _options.height;
        }
        return r;
    }

private:
    GLFWwindow * _window  = nullptr;
    Options      _options = {};

    /// Used to record application screen to video or a series of images.
    Recorder _recorder;
    size_t   _recorded = 0;

    /// Records current draw to an image.
    bool recordFrame(VkImageLayout layout) {
        // If we are recording.
        auto & loop    = this->loop();
        bool   running = true;
        if (_options.recordPath.size() > 0 && loop.frameCounter() >= _options.recordStartFrame) {
            // Write the image to the recorder.
            _recorder.write(readCurrentFrame(layout), loop.frameCounter());
            if (++_recorded == _options.recordFrameCount) running = false;
        }
        return running;
    }

    /// @param layout the back buffer layout.
    /// @return raw image read from current frame.
    ph::RawImage readCurrentFrame(VkImageLayout layout) {
        return readBaseImagePixels(dev().graphicsQ(), sw().backBuffer(sw().activeBackBufferIndex()).image, layout, sw().initParameters().colorFormat,
                                   sw().initParameters().width, sw().initParameters().height);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
//
inline void parseResolution(const std::string & s, uint32_t & w, uint32_t & h) {
    std::regex  re("([0-9]+)x([0-9]+)");
    std::smatch mr;

    if (!std::regex_match(s, mr, re)) { PH_THROW("screen resolution must be in form of WxH"); }

    w = std::stoul(mr[1].str());
    h = std::stoul(mr[2].str());
}

// ---------------------------------------------------------------------------------------------------------------------
//
template<typename SCENE, typename... ARGS>
inline void run(const DesktopApp::Options & dao, ARGS... args) {
    DesktopApp app(dao, [&](SimpleApp & app) { return new SCENE(app, args...); });
    app.run();
}

// ---------------------------------------------------------------------------------------------------------------------
/// Setup common command line options for desktop app
///
/// Note: this is a long list of options. When adding new options, please keep the list sorted alphabetically.
#define SETUP_DESKTOP_APP_OPTIONS(app, dao)                                                                                                          \
    std::string resolution__ = "1280x720";                                                                                                           \
    app.set_help_all_flag("--help-all", "Expand all help");                                                                                          \
    app.add_option("--async-loading", dao.asyncLoading, "Loading scene asynchronously. Default is on");                                              \
    app.add_flag("--break-on-vk-error", dao.breakOnVkError, "Break into debugger on VK error. Default is off.");                                     \
    app.add_flag("-o,--offscreen", dao.offscreen, ph::formatstr("Enable offscreen mode when specified."));                                           \
    app.add_flag("-q", dao.rayQuery, ph::formatstr("Enable HW ray query extension if supported. Default is %s.", dao.rayQuery ? "on" : "off"));      \
    app.add_flag(                                                                                                                                    \
        "-Q", [&](int64_t) { dao.rayQuery = false; }, ph::formatstr("Disable HW ray query extension. Default is %s.", dao.rayQuery ? "on" : "off")); \
    app.add_option("--record-path", dao.recordPath,                                                                                                  \
                   "File path you want to record application output to. Must be printf formatted string\n"                                           \
                   "accepting frame number, like %%d.jpg.");                                                                                         \
    app.add_option("--record-start-frame", dao.recordStartFrame, "Index of the first frame to start recording. Default is 0.");                      \
    app.add_option("--record-frame-count", dao.recordFrameCount,                                                                                     \
                   "Exit the app after recording certain number of frames. Default is 1.\n"                                                          \
                   "Set to 0 to record indefinitely, letting other parameters (like -a) to determine when to stop.");                                \
    app.add_option("--resolution", resolution__, "Specify resolution in form of \"wxh\". Default is 1280x720");                                      \
    app.add_option("-v,--vsync", dao.vsync, "Specify vsync state. Default is off.");                                                                 \
    app.add_option("--use-vma-allocator", dao.useVmaAllocator, ph::formatstr("Enable VMA for device memory allocations. Default is on."));           \
    app.add_option("--min-frame-rate", dao.minFrameRate, ph::formatstr("Minimum number of frames per second. Defaults to %f.", dao.minFrameRate));   \
    app.add_option("--max-frame-rate", dao.maxFrameRate, "Maximum number of frames per second. Defaults to infinity.");                              \
    app.add_option_function<float>(                                                                                                                  \
        "--fixed-frame-rate",                                                                                                                        \
        [&](const float & fixedFrameRate) {                                                                                                          \
            dao.minFrameRate = fixedFrameRate;                                                                                                       \
            dao.maxFrameRate = fixedFrameRate;                                                                                                       \
        },                                                                                                                                           \
        "Sets min and max framerate to argument.");

// ---------------------------------------------------------------------------------------------------------------------
/// Setup common command line options (required CLI11 library)
///
/// Note: this is a long list of options. When adding new options, please keep the list sorted alphabetically.
#define SETUP_RT_SCENE_OPTIONS(app, o)                                                                                          \
    app.add_option("-a", o.animated,                                                                                            \
                   ph::formatstr("Set Animation count.\n"                                                                       \
                                 "    =0: Animation disabled.\n"                                                                \
                                 "    >0: Run animation indefinitely. This is the default option.\n"                            \
                                 "    <0: Run animation for certain number of loops, then quit the app.\n"                      \
                                 "        This option is useful to record and export animation sequence. Note that this\n"      \
                                 "        option is effective only when --record-frame-count=0 is also specified.",             \
                                 o.animated));                                                                                  \
    app.add_option("--accum", o.accum,                                                                                          \
                   ph::formatstr("Accumulative rendering.\n"                                                                    \
                                 "       =0: disabled\n"                                                                        \
                                 "       >0: accumulate for N frames.\n"                                                        \
                                 "       <0: accumulate for N seconds.\n"                                                       \
                                 "   Default is accumulating for %d frames.",                                                   \
                                 o.accum));                                                                                     \
    app.add_option("--camera", o.activeCamera, "Select active camera. Default is 0.");                                          \
    app.add_option("--db,--max-diffuse-bounces", o.diffBounces, "Specify maximum diffuse bounces.");                            \
    app.add_flag("--flythrough", o.flythroughCamera, "Use flythrough, instead of orbital, camera.");                            \
    app.add_flag("-l,--left-handed", o.leftHanded,                                                                              \
                 ph::formatstr("Specify the handedness of the coordinate system from which the geometry data is based off of. " \
                               "Default is a right-handed configuration."));                                                    \
    app.add_option("-r,--render-pack", o.rpmode,                                                                                \
                   ph::formatstr("Select render pack mode. Default is %d.\n"                                                    \
                                 "       0 : Rasterize.\n"                                                                      \
                                 "       1 : Path tracing.\n"                                                                   \
                                 "       2 : Noise-free path tracing.\n"                                                        \
                                 "       3 : Shadow only tracing.\n"                                                            \
                                 "       4 : Fast path tracer.\n",                                                              \
                                 o.rpmode));                                                                                    \
    app.add_option("-s, --shadow", o.shadowMode,                                                                                \
                   ph::formatstr("Specify initial shadow mode. Default is %d. It can also be change in real time by key 'O'.\n" \
                                 "       0 : ray traced shadow.\n"                                                              \
                                 "       1 : rasterized shadow.\n"                                                              \
                                 "       2 : refined shadow.\n"                                                                 \
                                 "       3 : debug mode.\n"                                                                     \
                                 "       4 : ray traced shadows with alpha-blended transparency.\n",                            \
                                 o.shadowMode));                                                                                \
    app.add_option("--sb,--max-specular-bounces", o.specBounces, "Specify maximum specular bounces.");                          \
    app.add_option("--show-ui", o.showUI, "Specify visibility of UI window. Default is on.");                                   \
    app.add_option("--spp", o.spp, ph::formatstr("Samples per pixel per frame. Default is %d.", o.spp));

// ---------------------------------------------------------------------------------------------------------------------
/// Setup common command line options (required CLI11 library)
///
/// Note: this is a long list of options. When adding new options, please keep the list sorted alphabetically.
#define SETUP_COMMON_CLI11_OPTIONS(app, dao, o) \
    SETUP_DESKTOP_APP_OPTIONS(app, dao);        \
    SETUP_RT_SCENE_OPTIONS(app, o);

#define PARSE_CLI11_OPTIONS(app, dao, argc, argv) \
    CLI11_PARSE(app, argc, argv);                 \
    parseResolution(resolution__, dao.width, dao.height);
