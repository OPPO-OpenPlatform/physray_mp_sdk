/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "root-transform-channel.h"

namespace animations {

RootTransformChannel::RootTransformChannel(ph::rt::Node * root, ph::rt::Node * target)
    : TransformChannel(target, Eigen::Vector3f(), Eigen::Quaternionf(), Eigen::Vector3f()), _root(root) {
    // Retrieve the root's world transform.
    const ph::rt::NodeTransform rootWorldTransform = root->worldTransform();

    // Retrieve the target's world transform.
    const ph::rt::NodeTransform targetWorldTransform = target->worldTransform();

    // Determine what the target's transform is relative to root space.
    const ph::rt::NodeTransform targetRootTransform = rootWorldTransform.inverse() * targetWorldTransform;

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

RootTransformChannel::RootTransformChannel(ph::rt::Node * root, ph::rt::Node * target, const Eigen::Vector3f & translation, const Eigen::Quaternionf & rotation,
                                           const Eigen::Vector3f & scale)
    : TransformChannel(target, translation, rotation, scale), _root(root) {
    //
}

void RootTransformChannel::setTime(std::chrono::duration<uint64_t, std::nano>) {
    // Calculate the node transform from the separated transforms.
    // Make sure the transform is initialized to identity.
    ph::rt::NodeTransform nodeTransform = ph::rt::NodeTransform::Identity();

    // Combine everything into the transform by order of translate, rotate, scale.
    nodeTransform.translate(getTranslation());
    nodeTransform.rotate(getRotation());
    nodeTransform.scale(getScale());

    // Get the root's world transform.
    ph::rt::NodeTransform rootWorldTransform = _root->worldTransform();

    // Add the transform to root space.
    ph::rt::NodeTransform targetWorldTransform = rootWorldTransform * nodeTransform;

    // Fetch the target being transformed.
    ph::rt::Node * target = getTarget();

    // Update the target node to its new transform.
    target->setWorldTransform(targetWorldTransform);
}

} // namespace animations
