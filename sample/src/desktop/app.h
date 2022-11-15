#include "../common/vkutils.h"
#include "../common/first-person-controller.h"
#include "../common/recorder.h"
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
        bool     gpuBvh          = false;
        bool     offscreen       = false;
        bool     useVmaAllocator = true;
        bool     showUI          = true;

        /// If set to a folder path, this will output the app's screen to a series of images.
        std::string recordPath = "";

        /// Specify from which frame the recording starts.
        uint32_t recordFrame = 0;
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
            for (uint32_t i = 0; i < extcount; ++i) {
                instanceExtensions[required[i]] = true;
            }
        }
        construct({
            {
                .instanceExtensions = instanceExtensions,
                .validation         = PH_BUILD_DEBUG ? SimpleVulkanInstance::LOG_ON_VK_ERROR_WITH_CALL_STACK : SimpleVulkanInstance::VALIDATION_DISABLED,
            },
            {
                .useVmaAllocator = o.useVmaAllocator,
            },
            VK_FORMAT_B8G8R8A8_UNORM,
            o.rayQuery,
            o.gpuBvh,
            o.offscreen,
            o.vsync,
            o.showUI,
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
                running = render();
                recordFrame(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
            return;
        }

        // a little hack to make run parameters available to GLFW callbacks.
        static DesktopApp * p = this;
        glfwSetCursorPosCallback(_window, [](GLFWwindow * window, double x, double y) {
            glfwSetWindowTitle(window, ph::formatstr("mouse: (%d, %d)", (int) x, (int) y));
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

    /// @param layout the back buffer layout.
    /// @return raw image read from current frame.
    ph::RawImage readCurrentFrame(VkImageLayout layout) {
        // This code is currently only used in window mode.
        // Ensure image layout is able to switch if we modify this for using headless rendering.
        return readBaseImagePixels(dev().graphicsQ(), sw().backBuffer(sw().activeBackBufferIndex()).image, layout, sw().initParameters().colorFormat,
                                   sw().initParameters().width, sw().initParameters().height);
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

    /// Records current draw to an image.
    /// \param layout The back buffer layout
    void recordFrame(VkImageLayout layout) {
        // If we are recording.
        auto & loop = this->loop();
        if (_options.recordPath.size() > 0 && loop.frameCounter() >= _options.recordFrame) {
            // Write the image to the recorder.
            _recorder.write(readCurrentFrame(layout), loop.frameCounter());
        }
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
template<typename SCENE>
inline void run(const DesktopApp::Options & dao, const typename SCENE::Options & options) {
    DesktopApp app(dao, [&](SimpleApp & app) { return new SCENE(app, options); });
    app.run();
}

// ---------------------------------------------------------------------------------------------------------------------
/// Setup common command line options (required CLI11 library)
///
/// Note: this is a long list of options. When adding new options, please keep the list sorted alphabatically.
#define SETUP_COMMON_CLI11_OPTIONS(app, dao, o)                                                                                                               \
    std::string resolution__ = "1280x720";                                                                                                                    \
    app.set_help_all_flag("--help-all", "Expand all help");                                                                                                   \
    app.add_flag("-a", o.animated, ph::formatstr("Enable animation. Default is %s.", o.animated ? "enabled" : "disabled"));                                   \
    app.add_flag(                                                                                                                                             \
        "-A", [&](int64_t) { o.animated = false; }, ph::formatstr("Disable animation. Default is %s.", o.animated ? "enabled" : "disabled"));                 \
    app.add_option("--accum", o.accum, "Accumulative rendering. Default is on.");                                                                             \
    app.add_option("--camera", o.activeCamera, "Select active camera. Default is 0.");                                                                        \
    app.add_option("--db,--max-diffuse-bounces", o.diffBounces, "Specify maxinum diffuse bounces.");                                                          \
    app.add_option_function<float>(                                                                                                                           \
        "--fixed-frame-rate",                                                                                                                                 \
        [&](const float & fixedFrameRate) {                                                                                                                   \
            o.minFrameRate = fixedFrameRate;                                                                                                                  \
            o.maxFrameRate = fixedFrameRate;                                                                                                                  \
        },                                                                                                                                                    \
        "Sets min and max framerate to argument.");                                                                                                           \
    app.add_flag("--flythrough", o.flythroughCamera, "Use flythrough, instead of orbital, camera.");                                                          \
    app.add_flag("-l,--left-handed", o.leftHanded,                                                                                                            \
                 ph::formatstr("Specify the handedness of the coordinate system from which the geometry data is based off of. "                               \
                               "Default is a right-handed configuration."));                                                                                  \
    app.add_option("--max-frames", o.maxFrames, "Exit the app after rendering certain number of frames. Set to 0 to disable this feature.");                  \
    app.add_option("--min-frame-rate", o.minFrameRate, ph::formatstr("Minimum number of frames per second. Defaults to %f.", o.minFrameRate));                \
    app.add_option("--max-frame-rate", o.maxFrameRate, "Maximum number of frames per second. Defaults to infinity.");                                         \
    app.add_option("--max-spp", o.maxSpp,                                                                                                                     \
                   ph::formatstr("Maximum samples per pixel. Default is %d which allows infinite accumulation. Set to -1 to accumulate over 3s.", o.maxSpp)); \
    app.add_flag("-o,--offscreen", dao.offscreen, ph::formatstr("Enable offscreen mode when specified."));                                                    \
    app.add_flag("-q", dao.rayQuery, ph::formatstr("Enable HW ray query extension if supported. Default is %s.", dao.rayQuery ? "on" : "off"));               \
    app.add_flag(                                                                                                                                             \
        "-Q", [&](int64_t) { dao.rayQuery = false; }, ph::formatstr("Disable HW ray query extension. Default is %s.", dao.rayQuery ? "on" : "off"));          \
    app.add_option("--record-path", dao.recordPath,                                                                                                           \
                   "File path you want to record application output to. "                                                                                     \
                   "For images, this requires formatted strings accepting the frame number (e.g. \"output/%d.jpg\").");                                       \
    app.add_option("--record-start-frame", dao.recordFrame, "Index of the first frame to start recording. Default is 0.");                                    \
    app.add_option("-r,--render-pack", options.rpmode,                                                                                                        \
                   ph::formatstr("Select render pack mode. Default is %d.\n"                                                                                  \
                                 "       0 : Rasterize.\n"                                                                                                    \
                                 "       1 : Path tracing.\n"                                                                                                 \
                                 "       2 : Noise-free path tracing.\n"                                                                                      \
                                 "       3 : Shadow only tracing.\n",                                                                                         \
                                 options.rpmode));                                                                                                            \
    app.add_option("--resolution", resolution__, "Specify resolution in form of \"wxh\". Default is 1280x720");                                               \
    app.add_option("-s, --shadow", o.shadowMode,                                                                                                              \
                   ph::formatstr("Specify initial shadow mode. Default is %d. It can also be change in real time by key 'O'.\n"                               \
                                 "       0 : ray traced shadow.\n"                                                                                            \
                                 "       1 : rasterized shadow.\n"                                                                                            \
                                 "       2 : refined shadow.\n"                                                                                               \
                                 "       3 : debug mode.\n"                                                                                                   \
                                 "       4 : ray traced shadows with alpha-blended transparency.\n",                                                          \
                                 o.shadowMode));                                                                                                              \
    app.add_option("--sb,--max-specular-bounces", o.specBounces, "Specify maxinum specular bounces.");                                                        \
    app.add_option("--show-ui", o.showUI, "Specify visibility of UI window. Default is on.");                                                                 \
    app.add_option("--skinning", options.skinMode,                                                                                                            \
                   ph::formatstr("Specify skinning mode. Default is %d.\n"                                                                                    \
                                 "       0: skinning off.\n"                                                                                                  \
                                 "       1: CPU skinning.\n",                                                                                                 \
                                 options.skinMode));                                                                                                          \
    app.add_option("--spp", o.spp, ph::formatstr("Samples per pixel per frame. Default is %d.", o.spp));                                                      \
    app.add_option("-v,--vsync", dao.vsync, "Specify vsync state. Default is off.");                                                                          \
    app.add_option("--use-vma-allocator", dao.useVmaAllocator, ph::formatstr("Enable VMA for device memory allocations. Default is on."));

#define PARSE_CLI11_OPTIONS(app, dao, argc, argv) \
    CLI11_PARSE(app, argc, argv);                 \
    parseResolution(resolution__, dao.width, dao.height);
