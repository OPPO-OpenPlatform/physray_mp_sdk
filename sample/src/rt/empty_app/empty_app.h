/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : empty_app.h
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

#include "../common/modelviewer.h"

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct EmptyScene : ModelViewer {

    struct Options : ModelViewer::Options {
        Options() {
            rpmode = World::RayTracingRenderPackCreateParameters::SHADOW_TRACING;
            showUI = false;
        }
    };

    EmptyScene(SimpleApp & app, const Options & o): ModelViewer(app, o) {}

private:
    const Options _options;
};