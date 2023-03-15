/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-camera-builder.h
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

/**
 *
 */
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
