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
#include "suzanne.h"
#include <CLI11.hpp>

int main(int argc, const char * argv[]) {
    try {
        DesktopApp::Options   dao;
        SuzanneScene::Options options;
        CLI::App              app("Suzanne");
        SETUP_COMMON_CLI11_OPTIONS(app, dao, options);
        auto c = app.add_flag("-c,--cornell", options.cornellBox, "Add cornell box around the scene. Default is off.");
        app.add_flag("-f,--floor", options.floorPlane, "Add floor plane to the scene. Default is off.")->excludes(c);
        app.add_option("--skybox-lod-bias", options.skyboxLodBias, "Specify skybox texture LOD bias. Default is 2.0. Set to negative to disable skybox.");
        app.add_flag(
            "--rocket", [&](auto) { options.model = "model/the-rocket/the-rocket.glb"; }, "Load rocket scene..");
        app.add_flag(
            "--helmet", [&](auto) { options.model = "model/damaged-helmet/damaged-helmet.gltf"; }, "Load helmet scene..");
        app.add_flag(
            "--glasses",
            [&](auto) {
                options.model         = "model/cat-eye-glasses.gltf";
                options.skyboxLodBias = 0.0f;
            },
            "Load glasses scene..");
        app.add_flag(
            "--skin", [&](auto) { options.model = "model/skinned-test/rigged-simple.glb"; }, "Load skinned animatio.");
        app.add_option("model", options.model, "Optional parameter to specify path of the model to load.");
        app.add_option("animation", options.animation, "Optional parameter to specify name of the animation to play.");
        PARSE_CLI11_OPTIONS(app, dao, argc, argv);
        run<SuzanneScene>(dao, options);
        return 0;
    } catch (std::exception & ex) {
        PH_LOGE("%s\n", ex.what());
        return -1;
    } catch (...) {
        PH_LOGE("unknown exception...");
        return -1;
    }
}
