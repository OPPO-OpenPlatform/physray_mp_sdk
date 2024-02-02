/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "weight-channel.h"

namespace animations {

WeightChannel::WeightChannel(sg::Node * target, MorphTargetManager * morphTargetManager): _target(target), _morphTargetManager(morphTargetManager) {
    _target->forEachModel([this](auto model, auto) { _mesh = &model->mesh(); });
    if (_mesh == nullptr)
        PH_LOGI("Node targeted by weight animation channel has no model attached to it!");
    else {
        _weights = _morphTargetManager->getWeights(_mesh);
    }
}

WeightChannel::WeightChannel(sg::Node * target, MorphTargetManager * morphTargetManager, const std::vector<float> weights)
    : _target(target), _morphTargetManager(morphTargetManager), _weights(weights) {
    //
}

void WeightChannel::setTime(std::chrono::duration<uint64_t, std::nano>) { _morphTargetManager->setWeights(((ph::rt::Mesh *) _mesh), _weights); }
} // namespace animations