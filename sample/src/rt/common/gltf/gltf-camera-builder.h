/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "gltf.h"
#include "../camera.h"

namespace gltf {

/**
 * Constructs a camera from a tiny gltf object.
 */
class GLTFCameraBuilder {
public:
    /**
     *
     */
    GLTFCameraBuilder() = default;

    /**
     *
     */
    virtual ~GLTFCameraBuilder() = default;

    /**
     * @param light The GLTF light to be converted.
     * @param node The node the created light will be attached to.
     * @return a newly created light matching the given tinygtlf light.
     * Returns a null pointer if light type isn't supported.
     */
    Camera build(const tinygltf::Camera & camera, ph::rt::Node * node);

private:
    Camera buildPerspective(const tinygltf::Camera & camera, ph::rt::Node * node);

    Camera buildOrthographic(const tinygltf::Camera & camera, ph::rt::Node * node);
};

} // namespace gltf
