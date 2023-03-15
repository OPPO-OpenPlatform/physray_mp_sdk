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

#include "carousel.h"
#include "../../desktop/app.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options    dao = {};
        CarouselScene::Options options;
        CLI::App               app("Carousel ReSTIR Demo");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        app.add_option("--cluster", options.cluster, "Enable cluster mode. Default is true.");
        app.add_option("--outputVideo", options.outputVideo, "Enable automatic snapshots for video output. Default is off.");
        app.add_option("--restirM", options.restirM, "Number of initial candidates for ReSTIR DI. Default is 0/off.");
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<CarouselScene>(dao, options);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknown exception...");
        return -1;
    }
}
