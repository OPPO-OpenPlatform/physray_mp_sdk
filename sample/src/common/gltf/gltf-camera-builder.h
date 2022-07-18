/**
 *
 */
#pragma once

#include <ph/rt.h>

#include "gltf.h"

namespace gltf {

/**
 * Constructs a camera from a tiny gltf object.
 */
class GLTFCameraBuilder {
public:
    /**
     * @param world Used to create new cameras.
     */
    GLTFCameraBuilder(ph::rt::Scene * scene): _scene(scene) {};

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
    ph::rt::Camera * build(const tinygltf::Camera & camera, ph::rt::Node * node);

private:
    /**
     * Scene being used to create new lights.
     */
    ph::rt::Scene * _scene;

    ph::rt::Camera * buildPerspective(const tinygltf::Camera & camera, ph::rt::Node * node);

    ph::rt::Camera * buildOrthographic(const tinygltf::Camera & camera, ph::rt::Node * node);
};

} // namespace gltf
