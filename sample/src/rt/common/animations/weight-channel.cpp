/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "weight-channel.h"

namespace animations {

WeightChannel::WeightChannel(ph::rt::Node * target, MorphTargetManager * morphTargetManager): _target(target), _morphTargetManager(morphTargetManager) {
    const auto components = _target->components();
    for (const auto & component : components) {
        if (component->type() == ph::rt::NodeComponent::MODEL) {
            _mesh = &((ph::rt::Model *) component)->mesh();
            break;
        }
    }
    if (_mesh == nullptr)
        PH_LOGI("Node targeted by weight animation channel has no meshview!");
    else {
        _weights = _morphTargetManager->getWeights(_mesh);
    }
}

WeightChannel::WeightChannel(ph::rt::Node * target, MorphTargetManager * morphTargetManager, const std::vector<float> weights)
    : _target(target), _morphTargetManager(morphTargetManager), _weights(weights) {
    //
}

void WeightChannel::setTime(std::chrono::duration<uint64_t, std::nano>) { _morphTargetManager->setWeights(((ph::rt::Mesh *) _mesh), _weights); }
} // namespace animations