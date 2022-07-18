/**
 *
 */
#include "pch.h"
#include "gltf-camera-builder.h"

namespace gltf {

ph::rt::Camera * GLTFCameraBuilder::build(const tinygltf::Camera & camera, ph::rt::Node * node) {
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
        ph::rt::Camera * phCamera = _scene->addCamera({
            .node = node,
        });

        return phCamera;
    }
}

ph::rt::Camera * GLTFCameraBuilder::buildPerspective(const tinygltf::Camera & camera, ph::rt::Node * node) {
    // Fetch the perspective properties.
    const tinygltf::PerspectiveCamera & perspectiveCamera = camera.perspective;

    // Create the perspective camera.
    ph::rt::Camera * phCamera = _scene->addCamera({.node = node,
                                                   .desc {
                                                       .yFieldOfView = (float) perspectiveCamera.yfov,
                                                       .zNear        = (float) perspectiveCamera.znear,
                                                       .zFar         = (float) perspectiveCamera.zfar,
                                                   }});

    return phCamera;
}

ph::rt::Camera * GLTFCameraBuilder::buildOrthographic(const tinygltf::Camera & camera, ph::rt::Node * node) {
    // Fetch the perspective properties.
    const tinygltf::OrthographicCamera & orthographicCamera = camera.orthographic;

    // Create the orthographic camera.
    ph::rt::Camera * phCamera = _scene->addCamera({.node = node,
                                                   .desc {
                                                       .yFieldOfView = 0.f, // set FOV to 0 to make an orthographic camera.
                                                       .zNear        = (float) orthographicCamera.znear,
                                                       .zFar         = (float) orthographicCamera.zfar,
                                                   }});

    return phCamera;
}

} // namespace gltf
