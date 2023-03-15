/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : main.cpp
 *
 * Version: 2.0
 *
 * Date : Feb 2023
 *
 * Author: Computing & Graphics Research Institute
 *
 * ------------------ Revision History: ---------------------
 *  <version>  <date>  <author>  <desc>
 *
 *******************************************************************************/

#include "../../desktop/app.h"
#include "ptdemo.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options     dao = {};
        PathTracerDemo::Options options;
        CLI::App                app("Path Tracer Demo");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        resolution__ = "2412x1080";
        app.add_option("--day", options.day, "Select day/true or night/false. Default is true.");
        app.add_option("--envMap", options.reflectionMapAsset, "Environment map asset. Must be a dds with LODs.");
        app.add_option("--irrMap", options.irradianceMapAsset, "Irradiance map asset. Must be a dds with LODs.");
        app.add_option("--orbitalCenter", options.center,
                       "Orbital center for camera and lights. Should be set based on position of the fairy. Default is \"5,4,-1.5\".");
        app.add_option("--roughnessCutoff", options.roughnessCutoff, "Reflection roughness cutoff. Default is 0.5.");
        app.add_option("--enableIdle", options.enableIdle, "Enable idle animation. Default is off.");
        app.add_option("--outputVideo", options.outputVideo, "Enable automatic snapshots for video output. Default is off.");
        app.add_option("--skipCamAnim", options.skipCamAnim, "Skip camera animations. Default is off.");
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
