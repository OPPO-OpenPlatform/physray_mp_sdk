/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "gltf-timeline-builder.h"
#include "gltf-transform-channel-builder.h"
#include "gltf-weight-channel-builder.h"
#include "../physray-type-converter.h"
#include "../../animations/target-channel.h"

#include <chrono>

namespace gltf {
namespace animations {

GLTFTimelineBuilder::GLTFTimelineBuilder(tinygltf::Model * model, std::shared_ptr<SceneAsset> sceneAsset, tinygltf::Animation * animation,
                                         MorphTargetManager * morphTargetManager)
    : _model(model), _sceneAsset(sceneAsset), _animation(animation), _morphTargetManager(morphTargetManager) {
    //
}

std::shared_ptr<::animations::Timeline> GLTFTimelineBuilder::build() {
    // Create the animation object being converted to.
    std::shared_ptr<::animations::Timeline> timeline(new ::animations::Timeline());
    timeline->name = _animation->name;

    // Get the list of animation channels.
    std::vector<std::shared_ptr<::animations::Channel>> & channels = timeline->getChannels();

    // Iterate all channels.
    for (std::size_t index = 0; index < _animation->channels.size(); ++index) {
        // Fetch the channel being converted.
        tinygltf::AnimationChannel & channel = _animation->channels[index];

        // Convert it to its PhysRay equivelant and save to the result.
        addChannel(channels, channel);
    }

    // Add all the transform channels to the end of the list of channels.
    for (auto iterator = _nodeToTransformChannel.begin(); iterator != _nodeToTransformChannel.end(); ++iterator) {
        // Store the channel in a shared pointer to handle its deletion
        // and then save it to the list of channels.
        channels.push_back(std::shared_ptr<::animations::Channel>(iterator->second));
    }

    // Add all the weight channels to the end of the list of channels.
    for (auto iterator = _nodeToWeightChannel.begin(); iterator != _nodeToWeightChannel.end(); ++iterator) {
        // Store the channel in a shared pointer to handle its deletion
        // and then save it to the list of channels.
        channels.push_back(std::shared_ptr<::animations::Channel>(iterator->second));
    }

    // Update the timeline so that it knows about the new channels.
    timeline->updateChannels();

    // Clear set of node to transform channels to ensure they are not used again.
    _nodeToTransformChannel.clear();
    _nodeToWeightChannel.clear();

    PH_LOGI("GLTF animation loaded. Duration = %s", ph::ns2str(timeline->getDuration().count()).c_str());
    return timeline;
}

void GLTFTimelineBuilder::addChannel(std::vector<std::shared_ptr<::animations::Channel>> & channels, tinygltf::AnimationChannel & channel) {
    // Determine the animation type and build according to it.
    // If this is animating the node's translation.
    if (channel.target_path == "translation") {
        // Build the channel to animate the desired property.
        std::shared_ptr<::animations::Channel> propertyTransformChannel = buildTransformChannel(channel, &GLTFTransformChannelBuilder::buildTranslateChannel);

        // Build the channel and add it to the timeline's list of channels.
        channels.push_back(propertyTransformChannel);

        // If this is animating the node's rotation.
    } else if (channel.target_path == "rotation") {
        // Build the channel to animate the desired property.
        std::shared_ptr<::animations::Channel> propertyTransformChannel = buildTransformChannel(channel, &GLTFTransformChannelBuilder::buildRotateChannel);

        // Build the channel and add it to the timeline's list of channels.
        channels.push_back(propertyTransformChannel);

        // If this is animating the node's scale.
    } else if (channel.target_path == "scale") {
        // Build the channel to animate the desired property.
        std::shared_ptr<::animations::Channel> propertyTransformChannel = buildTransformChannel(channel, &GLTFTransformChannelBuilder::buildScaleChannel);

        // Build the channel and add it to the timeline's list of channels.
        channels.push_back(propertyTransformChannel);

        // If this is animating the node's morph targets.
    } else if (channel.target_path == "weights") {
        if (_morphTargetManager) {
            std::shared_ptr<::animations::Channel> weightChannel = buildWeightChannel(channel);

            // Build the channel and add it to the timeline's list of channels.
            channels.push_back(weightChannel);
        }

        // If target path is not recognized.
    } else {
        // Fire a warning and ignore it.
        PH_LOGW("Unsupported animation channel target path '%s'", channel.target_path.c_str());
    }
}

std::shared_ptr<::animations::Channel>
GLTFTimelineBuilder::buildTransformChannel(tinygltf::AnimationChannel & channel,
                                           std::shared_ptr<::animations::Channel> (GLTFTransformChannelBuilder::*buildMethod)()) {
    // Fetch its sampler.
    tinygltf::AnimationSampler & sampler = _animation->samplers[channel.sampler];

    // Determine the target.
    // Get/create the channel used to animate this node's transform.
    ::animations::TransformChannel * transformChannel = getNodeTransformChannel(channel.target_node);

    // Create the object to build the channel.
    GLTFTransformChannelBuilder channelBuilder(_model, transformChannel, &channel, &sampler);

    // Build the channel using the selected build method and return it.
    return (channelBuilder.*buildMethod)();
}

std::shared_ptr<::animations::Channel> GLTFTimelineBuilder::buildWeightChannel(tinygltf::AnimationChannel & channel) {
    // Fetch its sampler.
    tinygltf::AnimationSampler & sampler = _animation->samplers[channel.sampler];

    // Determine the target.
    // Get/create the channel used to animate this node's transform.
    ::animations::WeightChannel * weightChannel = getNodeWeightChannel(channel.target_node);

    // Create the object to build the channel.
    GLTFWeightChannelBuilder channelBuilder(_model, weightChannel, &channel, &sampler);

    // Build the channel using the selected build method and return it.
    return channelBuilder.build();
}

::animations::TransformChannel * GLTFTimelineBuilder::getNodeTransformChannel(int targetNode) {
    // Fetch the node being manipulated by this animation channel.
    ph::rt::Node * phNode = _sceneAsset->getNodes()[targetNode];

    // Find the channel in question.
    auto iterator = _nodeToTransformChannel.find(phNode);

    // If it already exists.
    if (iterator != _nodeToTransformChannel.end()) {
        // Return it.
        return iterator->second;

        // If it doesn't exist.
    } else {
        // We need to lazy initialize the transform channel for this node.
        // Create the channel for this node.
        // The GLTF specification says that starting matrices will always be decomposable
        // and never skew or shear, so it should have no trouble decomposing the node's transform.
        ::animations::TransformChannel * channel = new ::animations::TransformChannel(phNode);

        // Save it for any future uses.
        _nodeToTransformChannel[phNode] = channel;

        return channel;
    }
}

::animations::WeightChannel * GLTFTimelineBuilder::getNodeWeightChannel(int targetNode) {
    // Fetch the node being manipulated by this animation channel.
    ph::rt::Node * phNode = _sceneAsset->getNodes()[targetNode];

    // Find the channel in question.
    auto iterator = _nodeToWeightChannel.find(phNode);

    // If it already exists.
    if (iterator != _nodeToWeightChannel.end()) {
        // Return it.
        return iterator->second;

        // If it doesn't exist.
    } else {
        // Create the channel for this node.
        ::animations::WeightChannel * channel = new ::animations::WeightChannel(phNode, _morphTargetManager);

        // Save it for any future uses.
        _nodeToWeightChannel[phNode] = channel;

        return channel;
    }
}

} // namespace animations
} // namespace gltf
