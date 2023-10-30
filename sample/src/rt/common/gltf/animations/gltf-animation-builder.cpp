/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "gltf-animation-builder.h"
#include "gltf-timeline-builder.h"

#include <vector>

namespace gltf {
namespace animations {

GLTFAnimationBuilder::GLTFAnimationBuilder(tinygltf::Model * model, std::shared_ptr<SceneAsset> sceneAsset, MorphTargetManager * morphTargetManager)
    : _model(model), _sceneAsset(sceneAsset), _morphTargetManager(morphTargetManager) {
    //
}

void GLTFAnimationBuilder::build() {
    // Fetch the collections holding the results of this operation.
    // The list of all animations.
    std::vector<std::shared_ptr<::animations::Timeline>> & animations = _sceneAsset->getAnimations();

    // Mapping of names to named animations.
    std::unordered_map<std::string, std::unordered_set<std::shared_ptr<::animations::Timeline>>> & nameToAnimations = _sceneAsset->getNameToAnimations();

    // Iterate the animations.
    for (std::size_t index = 0; index < _model->animations.size(); ++index) {
        // Fetch the animation to be built.
        tinygltf::Animation & animation = _model->animations[index];

        // Create the object that will build the timeline for this animation.
        GLTFTimelineBuilder timelineBuilder(_model, _sceneAsset, &animation, _morphTargetManager);

        // Build this animation.
        std::shared_ptr<::animations::Timeline> timeline = timelineBuilder.build();

        // Save it to the list of all animations.
        animations.push_back(timeline);

        // Save the animation to the set of animations with its name.
        nameToAnimations[animation.name].insert(timeline);
    }
}

} // namespace animations
} // namespace gltf
