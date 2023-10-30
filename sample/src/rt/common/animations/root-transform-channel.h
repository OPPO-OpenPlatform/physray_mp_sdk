/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "transform-channel.h"

#include <algorithm>
#include <chrono>

namespace animations {

/**
 * This channel updates the target node to have its
 * transform match the combined value of its
 * translation, rotation, and scale properties relative to some root every tick.
 *
 * This channel does not animate the translation, rotation,
 * or scale itself. Instead, you should
 * create other channels that modify the properties of this one,
 * add them to the timeline's list of channels,
 * then add this channel last.
 *
 * After the other channels update this channel's
 * translation, rotation, and scale, it can then apply
 * those to the targted node.
 */
class RootTransformChannel : public ::animations::TransformChannel {
public:
    /**
     * @param root The node target's transform will be relative to.
     * @param target The node who's transform is
     * being updated by the animation.
     *
     * The values of translation, rotation, and scale
     * will be initialized to target's current values
     * by decomposing its transform.
     * @param target The node this channel will animate.
     */
    RootTransformChannel(ph::rt::Node * root, ph::rt::Node * target);

    /**
     * @param root The node target's transform will be relative to.
     * @param target The node who's world transform is
     * being updated by the animation.
     * @param target The node this channel will animate.
     * @param translation Starting translation.
     * @param rotation Starting rotation.
     * @param scale Starting scale.
     */
    RootTransformChannel(ph::rt::Node * root, ph::rt::Node * target, const Eigen::Vector3f & translation, const Eigen::Quaternionf & rotation,
                         const Eigen::Vector3f & scale);

    /**
     *
     */
    virtual ~RootTransformChannel() = default;

    /**
     * @return The node who's world transform is
     * being updated by the animation.
     */
    ph::rt::Node * getRoot() { return _root; }

    /**
     * This will simply set the target to the current values of the transform.
     */
    void setTime(std::chrono::duration<uint64_t, std::nano> time) override;

private:
    /**
     * The node target's transform will be relative to.
     */
    ph::rt::Node * _root;
};

} // namespace animations
