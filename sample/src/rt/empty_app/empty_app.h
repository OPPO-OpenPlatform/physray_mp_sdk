/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "../common/modelviewer.h"

using namespace ph;
using namespace ph::va;
using namespace ph::rt;
using namespace ph::rt::render;

struct EmptyScene : ModelViewer {

    struct Options : ModelViewer::Options {
        Options() {
            rpmode = RenderPackMode::SHADOW;
            showUI = false;
        }
    };

    EmptyScene(SimpleApp & app, const Options & o): ModelViewer(app, o) {}

private:
    const Options _options;
};