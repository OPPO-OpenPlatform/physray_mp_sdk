/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#include "pch.h"
#include "transform-channel.h"

namespace animations {

TransformChannel::TransformChannel(sg::Node * target): _target(target) {
    // Retrieve the transform of the given node.
    const sg::Transform & transform = target->transform();

    // Decompose the transform's current values so we can initialize
    // this transform channel's properties to them.
    // Fetch the translation.
    _translation = transform.translation();

    // Fetch the rotation scaling.
    Eigen::Matrix3f rotation;
    Eigen::Matrix3f scaling;
    transform.computeRotationScaling(&rotation, &scaling);
    _rotation = rotation;
    _scale    = Eigen::Vector3f {scaling(0, 0), scaling(1, 1), scaling(2, 2)};
}

TransformChannel::TransformChannel(sg::Node * target, const Eigen::Vector3f & translation, const Eigen::Quaternionf & rotation, const Eigen::Vector3f & scale)
    : _target(target), _translation(translation), _rotation(rotation), _scale(scale) {
    //
}

void TransformChannel::setTime(std::chrono::duration<uint64_t, std::nano>) {
    // Calculate the node transform from the separated transforms.
    // Make sure the transform is initialized to identity.
    sg::Transform nodeTransform = sg::Transform::Identity();

    // Combine everything into the transform by order of translate, rotate, scale.
    nodeTransform.translate(_translation);
    nodeTransform.rotate(_rotation);
    nodeTransform.scale(_scale);

    // Update the target node to its new transform.
    _target->setTransform(nodeTransform);
}

} // namespace animations
