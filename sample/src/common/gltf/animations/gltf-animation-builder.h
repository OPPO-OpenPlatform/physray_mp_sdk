/**
 *
 */
#pragma once

#include <ph/rt.h>

#include "../../animations/timeline.h"
#include "../accessor-reader.h"
#include "../gltf.h"
#include "../../scene-asset.h"
#include "../../morphtargets.h"

namespace gltf {
namespace animations {

/**
 * Assembles animations from tiny gltf objects.
 */
class GLTFAnimationBuilder {
public:
    /**
     *
     * @param model The tinygltf model who's items are being instantiated as animations.
     * @param sceneAsset The scene asset who's items are being animated.
     */
    GLTFAnimationBuilder(tinygltf::Model * model, std::shared_ptr<SceneAsset> sceneAsset, MorphTargetManager * morphTargetManager = nullptr);

    /**
     * Destructor.
     */
    virtual ~GLTFAnimationBuilder() = default;

    /**
     * @return The tinygltf model who's items are being instantiated as animations.
     */
    tinygltf::Model * getModel() { return _model; }

    /**
     * @return The scene asset who's items are being animated.
     */
    std::shared_ptr<SceneAsset> getSceneAsset() { return _sceneAsset; }

    /**
     * Generates the animations inside the model.
     */
    void build();

private:
    /**
     * The tinygltf model who's animations are being built.
     */
    tinygltf::Model * _model;

    /**
     * The scene asset who's items are being animated.
     */
    std::shared_ptr<SceneAsset> _sceneAsset;

    MorphTargetManager * _morphTargetManager;
};

} // namespace animations
} // namespace gltf
