/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-camera-builder.cpp
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
#include "pch.h"
#include "gltf-camera-builder.h"
#include "../camera.h"

namespace gltf {

Camera GLTFCameraBuilder::build(const tinygltf::Camera & camera, ph::rt::Node * node) {
    // Give the camera its properties according to the camera's type.
    // If this is an perspective camera.
    if (camera.type == "perspective") {
        return buildPerspective(camera, node);

        // If this is an orthographic camera.
    } else if (camera.type == "orthographic") {
        return buildOrthographic(camera, node);

        // If camera type is unsupported.
    } else {
        // Warn user that camera type is not supported.
        PH_LOGW("Camera type '%s' not supported. Defaulting to perspective camera.", camera.type.c_str());

        // Default to a perspective camera.
        Camera phCamera = {
            .node = node,
        };

        return phCamera;
    }
}

Camera GLTFCameraBuilder::buildPerspective(const tinygltf::Camera & camera, ph::rt::Node * node) {
    // Fetch the perspective properties.
    const tinygltf::PerspectiveCamera & perspectiveCamera = camera.perspective;

    // Create the perspective camera.
    Camera phCamera = {.yFieldOfView = (float) perspectiveCamera.yfov,
                       .zNear        = (float) perspectiveCamera.znear,
                       .zFar         = (float) perspectiveCamera.zfar,
                       .node         = node};

    return phCamera;
}

Camera GLTFCameraBuilder::buildOrthographic(const tinygltf::Camera & camera, ph::rt::Node * node) {
    // Fetch the perspective properties.
    const tinygltf::OrthographicCamera & orthographicCamera = camera.orthographic;

    // Create the orthographic camera.
    Camera phCamera = {.yFieldOfView = 0.f, // set FOV to 0 to make an orthographic camera.
                       .zNear        = (float) orthographicCamera.znear,
                       .zFar         = (float) orthographicCamera.zfar,
                       .node         = node};

    return phCamera;
}

} // namespace gltf
