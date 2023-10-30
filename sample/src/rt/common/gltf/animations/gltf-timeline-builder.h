/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "../../animations/timeline.h"
#include "../gltf.h"
#include "../../scene-asset.h"
#include "../../animations/transform-channel.h"
#include "../../animations/weight-channel.h"
#include "../../morphtargets.h"

#include <memory>

namespace gltf {
namespace animations {

class GLTFTransformChannelBuilder;

/**
 * Assembles animations from tiny gltf objects.
 */
class GLTFTimelineBuilder {
public:
    /**
     *
     * @param model The tinygltf model who's items are being instantiated as animations.
     * @param sceneAsset The scene asset who's items are being animated.
     * @param animation The animation a Timeline is being built from.
     */
    GLTFTimelineBuilder(tinygltf::Model * model, std::shared_ptr<SceneAsset> sceneAsset, tinygltf::Animation * animation,
                        MorphTargetManager * morphTargetManager = nullptr);

    /**
     * Destructor.
     */
    virtual ~GLTFTimelineBuilder() = default;

    /**
     * @return The tinygltf model who's items are being instantiated as animations.
     */
    tinygltf::Model * getModel() { return _model; }

    /**
     * @return The scene asset who's items are being animated.
     */
    std::shared_ptr<SceneAsset> getSceneAsset() { return _sceneAsset; }

    /**
     * @return The scene asset who's items are being animated.
     */
    tinygltf::Animation * getAnimation() { return _animation; }

    /**
     * Builds a timeline using the tinygltf animation object.
     * @return The timeline object generated from the tinygltf animation.
     */
    std::shared_ptr<::animations::Timeline> build();

private:
    /**
     * The tinygltf model who's animations are being built.
     */
    tinygltf::Model * _model;

    /**
     * The scene asset who's items are being animated.
     */
    std::shared_ptr<SceneAsset> _sceneAsset;

    /**
     * The animation a Timeline is being built from.
     */
    tinygltf::Animation * _animation;

    MorphTargetManager * _morphTargetManager;

    /**
     * Maps node ids to the transform channel being
     * used to animate their transforms.
     *
     * The Transform channels will eventually be passed to a shared pointer inside of the timeline,
     * so this class doesn't have to worry about deleting them.
     */
    std::unordered_map<ph::rt::Node *, ::animations::TransformChannel *> _nodeToTransformChannel;

    std::unordered_map<ph::rt::Node *, ::animations::WeightChannel *> _nodeToWeightChannel;

    /**
     * Converts tinygltf animation channel to its equivelant ph
     * type and saves it to channels.
     * Not all channel types are supported, and as such
     * this method may leave the channels collection unmodified.
     * @param channels The collection to save the converted channel to.
     * @param channel tiny gltf channel to be converted.
     */
    void addChannel(std::vector<std::shared_ptr<::animations::Channel>> & channels, tinygltf::AnimationChannel & channel);

    /**
     * @param channel tiny gltf channel to be converted.
     * @param buildMethod The method of GLTFTransformChannelBuilder to use to build the channel.
     * @return a pointer to the converted PhysRay channel.
     */
    std::shared_ptr<::animations::Channel> buildTransformChannel(tinygltf::AnimationChannel & channel,
                                                                 std::shared_ptr<::animations::Channel> (GLTFTransformChannelBuilder::*buildMethod)());

    /**
     * @param channel tiny gltf channel to be converted.
     * @param buildMethod The method of GLTFTransformChannelBuilder to use to build the channel.
     * @return a pointer to the converted PhysRay channel.
     */
    std::shared_ptr<::animations::Channel> buildWeightChannel(tinygltf::AnimationChannel & channel);

    /**
     * @param targetNode Identifier of the node this wants to animate.
     * @return The transform channel being used to animate
     * the given node's transform.
     * Will be lazy initialized if it does not already exist.
     */
    ::animations::TransformChannel * getNodeTransformChannel(int targetNode);
    ::animations::WeightChannel *    getNodeWeightChannel(int targetNode);
};

} // namespace animations
} // namespace gltf
