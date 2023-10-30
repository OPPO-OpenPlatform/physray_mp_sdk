/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "../../desktop/app.h"
#include "empty_app.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options dao     = {};
        EmptyScene::Options options = {};
        CLI::App            app("Empty");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<EmptyScene>(dao, options);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknown exception...");
        return -1;
    }
}
