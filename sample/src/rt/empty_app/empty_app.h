/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
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