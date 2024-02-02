/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "refl.h"
#include "../../desktop/app.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options dao = {};
        ReflScene::Options  options;
        CLI::App            app("Reflection Render Pack Tests");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<ReflScene>(dao, options);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknown exception...");
        return -1;
    }
}
