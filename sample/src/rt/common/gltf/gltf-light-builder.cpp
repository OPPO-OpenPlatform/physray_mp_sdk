/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-light-builder.cpp
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
#include "gltf-light-builder.h"

namespace gltf {

ph::rt::Light * GLTFLightBuilder::build(const tinygltf::Light & light, ph::rt::Node * node) {
    // Fetch the scene of the node we are adding a component to
    // so that we can create that component.
    ph::rt::Scene & phScene = node->scene();

    // Reference to the created light.
    ph::rt::Light * phLight;

    // Get the light's emissive color.
    ph::rt::Float3 emission = getEmissive(light);

    // If light is a directional light.
    if (light.type == "directional") {
        // Create the light.
        phLight   = phScene.createLight({});
        auto desc = ph::rt::Light::Desc().setType(ph::rt::Light::Type::DIRECTIONAL).setEmission(emission);
        desc.directional.setDir(0.0f, 0.0f, -1.0f);
        phLight->reset(desc);
        node->attachComponent(phLight);

        // Give light a suitable shadow map.
        // Point lights use cubed maps, other lights use 2D.
        phLight->shadowMap = _textureCache->createShadowMap2D(light.name.c_str());

        // If light is a point light.
    } else if (light.type == "point") {
        // Create the light.

        // From https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md
        // if light.range = 0, it's undefined/assumed to be infinite.
        // When range is defined, it's recommended to be computed as follows:
        // attenuation = max( min( 1.0 - ( current_distance / range )4, 1 ), 0 ) / current_distance2
        // Otherwise, attenuation should be calculated using the inverse square law.
        // So when range is undefined, it should be calculated from attenuation = intensity / distance^2
        // Choosing a cutoff of attenuation = .001,
        // rangeDistance = sqrt(intensity / .001).
        float range = ((light.range <= 0) ? std::sqrt(float(light.intensity) / 0.001f) : float(light.range));
        phLight     = phScene.createLight({});
        phLight->reset(ph::rt::Light::Desc().setEmission(emission).setRange(range));
        node->attachComponent(phLight);

        // Give light a suitable shadow map.
        // Point lights use cubed maps, other lights use 2D.
        phLight->shadowMap = _textureCache->createShadowMapCube(light.name.c_str());

        // If light is a spotlight.
    } else if (light.type == "spot") {
        // Fetch the spotlight being transferred.
        const tinygltf::SpotLight & spot = light.spot;

        // From https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md
        // if light.range = 0, it's undefined/assumed to be infinite.
        // When range is defined, it's recommended to be computed as follows:
        // attenuation = max( min( 1.0 - ( current_distance / range )4, 1 ), 0 ) / current_distance2
        // Otherwise, attenuation should be calculated using the inverse square law.
        // So when range is undefined, it should be calculated from attenuation = intensity / distance^2
        // Choosing a cutoff of attenuation = .001,
        // rangeDistance = sqrt(intensity / .001).
        float range = ((light.range <= 0) ? std::sqrt(float(light.intensity) / 0.001f) : float(light.range));

        // Create the light.
        phLight = phScene.createLight({});
        phLight->reset(ph::rt::Light::Desc().setEmission(emission).setRange(range).setSpot(
            ph::rt::Light::Spot().setDir(ph::rt::Float3::make(0, 0, 1)).setFalloff((float) spot.innerConeAngle, (float) spot.outerConeAngle)));
        node->attachComponent(phLight);

        // Give light a suitable shadow map.
        // Point lights use cubed maps, other lights use 2D.
        phLight->shadowMap = _textureCache->createShadowMap2D(light.name.c_str());

        // If light type isn't recognized.
    } else {
        // Warn user this type isn't supported.
        PH_LOGW("Light type '%s' not supported. Defaulting to point light.", light.type.c_str());

        // Default to a point light.
        phLight = phScene.createLight({});
        phLight->reset(ph::rt::Light::Desc().setPoint(ph::rt::Light::Point()).setDimension(0.f, 0.f).setEmission(emission[0], emission[1], emission[2]));
        node->attachComponent(phLight);

        // Give light a suitable shadow map.
        // Point lights use cubed maps, other lights use 2D.
        phLight->shadowMap = _textureCache->createShadowMapCube(light.name.c_str());
    }

    return phLight;
}

ph::rt::Float3 GLTFLightBuilder::getEmissive(const tinygltf::Light & light) {
    ph::rt::Float3 emissive = {};

    // Fill in the selected colors.
    for (std::size_t index = 0; index < light.color.size(); ++index) { emissive[index] = (float) light.color[index]; }

    // If the file did not specify all (or any) of the colors, default to 1.0f.
    for (std::size_t index = light.color.size(); index < 3; ++index) { emissive[index] = 1.0f; }

    return emissive;
}

} // namespace gltf
