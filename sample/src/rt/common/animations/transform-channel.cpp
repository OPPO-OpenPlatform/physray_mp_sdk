/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : transform-channel.cpp
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
#include "transform-channel.h"

namespace animations {

TransformChannel::TransformChannel(ph::rt::Node * target): _target(target) {
    // Retrieve the transform of the given node.
    const ph::rt::NodeTransform & transform = target->transform();

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

TransformChannel::TransformChannel(ph::rt::Node * target, const Eigen::Vector3f & translation, const Eigen::Quaternionf & rotation,
                                   const Eigen::Vector3f & scale)
    : _target(target), _translation(translation), _rotation(rotation), _scale(scale) {
    //
}

void TransformChannel::setTime(std::chrono::duration<uint64_t, std::nano>) {
    // Calculate the node transform from the separated transforms.
    // Make sure the transform is initialized to identity.
    ph::rt::NodeTransform nodeTransform = ph::rt::NodeTransform::Identity();

    // Combine everything into the transform by order of translate, rotate, scale.
    nodeTransform.translate(_translation);
    nodeTransform.rotate(_rotation);
    nodeTransform.scale(_scale);

    // Update the target node to its new transform.
    _target->setTransform(nodeTransform);
}

} // namespace animations
