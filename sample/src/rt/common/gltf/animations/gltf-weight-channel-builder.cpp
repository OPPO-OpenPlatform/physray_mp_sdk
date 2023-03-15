/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-weight-channel-builder.cpp
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

#include "pch.h"
#include "gltf-weight-channel-builder.h"
#include "../../animations/target-channel.h"

namespace gltf {
namespace animations {

GLTFWeightChannelBuilder::GLTFWeightChannelBuilder(tinygltf::Model * model, ::animations::WeightChannel * weightChannel,
                                                   tinygltf::AnimationChannel * animationChannel, tinygltf::AnimationSampler * animationSampler)
    : _model(model), _weightChannel(weightChannel), _animationChannel(animationChannel), _accessorReader(model), _animationSampler(animationSampler) {
    //
}

std::shared_ptr<::animations::Channel> GLTFWeightChannelBuilder::build() {
    // If this is animating the node's translation.
    if (_animationChannel->target_path == "weights") {
        ::animations::WeightChannel * weightChannel = _weightChannel;

        std::shared_ptr<::animations::TargetChannel<std::vector<float>>> channel(
            new ::animations::TargetChannel<std::vector<float>>([weightChannel](std::vector<float> & value) { weightChannel->setWeights(value); }));

        // Parse key values
        buildVectorKeyValues(channel->getTimeToKeyValue());

        return channel;
    } else {
        // Return an empty shared pointer.
        return std::shared_ptr<::animations::Channel>();
    }
}

void GLTFWeightChannelBuilder::buildVectorKeyValues(
    std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<::animations::KeyValue<std::vector<float>>>> & timeToKeyValue) {
    size_t stride = _weightChannel->getStride();
    if (_animationSampler->interpolation == "LINEAR") {
        // Only linear is supported for morph targets
        SimpleKeyValueBuilder keyValueBuilder(stride, std::shared_ptr<::animations::Interpolator<std::vector<float>>>(new VectorLinearInterpolator()));

        buildKeyValues<std::vector<float>>(timeToKeyValue, keyValueBuilder, stride);
    } else {
        // Warn user that interpolation type is not supported.
        PH_LOGW("Interpolation type '%s' is not supported for morph target weights.");
    }
}

} // namespace animations
} // namespace gltf