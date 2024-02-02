/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#include "pch.h"
#include "root-transform-channel.h"

namespace animations {

RootTransformChannel::RootTransformChannel(sg::Node * root, sg::Node * target)
    : TransformChannel(target, Eigen::Vector3f::Zero(), Eigen::Quaternionf::Identity(), Eigen::Vector3f::Zero()), _root(root) {
    // Retrieve the root's world transform.
    const sg::Transform rootWorldTransform = root->worldTransform();

    // Retrieve the target's world transform.
    const sg::Transform targetWorldTransform = target->worldTransform();

    // Determine what the target's transform is relative to root space.
    const sg::Transform targetRootTransform = rootWorldTransform.inverse() * targetWorldTransform;

    // Decompose the transform's current values so we can initialize
    // this transform channel's properties to them.
    // Fetch the translation.
    setTranslation(targetRootTransform.translation());

    // Fetch the rotation scaling.
    Eigen::Matrix3f rotation;
    Eigen::Matrix3f scaling;
    targetRootTransform.computeRotationScaling(&rotation, &scaling);

    // Convert rotation matrix into quaternion.
    Eigen::Quaternionf rotationQuaternion;
    rotationQuaternion = rotation;

    // Save the rotation and scale.
    setRotation(rotationQuaternion);
    setScale(Eigen::Vector3f {scaling(0, 0), scaling(1, 1), scaling(2, 2)});
}

RootTransformChannel::RootTransformChannel(sg::Node * root, sg::Node * target, const Eigen::Vector3f & translation, const Eigen::Quaternionf & rotation,
                                           const Eigen::Vector3f & scale)
    : TransformChannel(target, translation, rotation, scale), _root(root) {
    //
}

void RootTransformChannel::setTime(std::chrono::duration<uint64_t, std::nano>) {
    // Calculate the node transform from the separated transforms.
    // Make sure the transform is initialized to identity.
    sg::Transform nodeTransform = sg::Transform::Identity();

    // Combine everything into the transform by order of translate, rotate, scale.
    nodeTransform.translate(getTranslation());
    nodeTransform.rotate(getRotation());
    nodeTransform.scale(getScale());

    // Get the root's world transform.
    sg::Transform rootWorldTransform = _root->worldTransform();

    // Add the transform to root space.
    sg::Transform targetWorldTransform = rootWorldTransform * nodeTransform;

    // Fetch the target being transformed.
    sg::Node * target = getTarget();

    // Update the target node to its new transform.
    target->setWorldTransform(targetWorldTransform);
}

} // namespace animations
