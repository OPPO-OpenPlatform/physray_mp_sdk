/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "../texture-cache.h"

#include "gltf.h"

namespace gltf {

/**
 * Constructs a light from a tiny gltf object.
 */
class GLTFLightBuilder {
public:
    /**
     * @param textureCache Loads images into the vulkan instance to create textures.
     */
    GLTFLightBuilder(TextureCache * textureCache): _textureCache(textureCache) {}

    /**
     *
     */
    virtual ~GLTFLightBuilder() = default;

    /**
     * @param light The GLTF light to be converted.
     * @param node The node the created light will be attached to.
     * @return a newly created light matching the given tinygtlf light.
     * Returns a null pointer if light type isn't supported.
     */
    ph::rt::Light * build(const tinygltf::Light & light, ph::rt::Node * node);

private:
    TextureCache * _textureCache;

    ph::rt::Float3 getEmissive(const tinygltf::Light & light);
};

} // namespace gltf
