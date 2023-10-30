/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "../../desktop/app.h"
#include "garage.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options  dao;
        GarageScene::Options options;
        CLI::App             app("Garage");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        app.add_option("model", options.model, "Specify path of the model.");
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<GarageScene>(dao, options);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknown exception...");
        return -1;
    }
}
