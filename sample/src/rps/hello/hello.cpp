/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "../../desktop/app.h"
#include "hello.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options dao;
        CLI::App            app("RPS Hello World");
        SETUP_DESKTOP_APP_OPTIONS(app, dao);
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<RPSHello>(dao);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknown exception...");
        return -1;
    }
}
