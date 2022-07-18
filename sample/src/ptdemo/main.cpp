#include "ptdemo.h"
#include "../desktop/app.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options     dao = {};
        PathTracerDemo::Options options;
        CLI::App                app("Path Tracer Demo");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        app.add_option("--scaling", options.scaling, "Scene scaling. Default is 1.0");
        app.add_option("--model", options.model, "Scene asset. Overrides manually composited scene when not null.");
        app.add_option("--envMap", options.reflectionMapAsset, "Environment map asset. Must be a dds with LODs.");
        app.add_option("--orbitalCenter", options.center,
                       "Orbital center for camera and lights. Should be set based on position of the fairy. Default is \"5,4,-1.5\".");
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<PathTracerDemo>(dao, options);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknown exception...");
        return -1;
    }
}
