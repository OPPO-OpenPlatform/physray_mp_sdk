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
#include "shadow.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options  dao     = {};
        ShadowScene::Options options = {};
        CLI::App             app("Shadow");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        app.add_flag("--dir", options.directional, "Use directional light. Default is point light.");
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<ShadowScene>(dao, options);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknowne exception...");
        return -1;
    }
}
