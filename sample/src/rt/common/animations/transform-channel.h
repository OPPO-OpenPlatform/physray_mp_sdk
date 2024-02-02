/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

#include "channel.h"

#include <algorithm>
#include <chrono>

namespace animations {

/**
 * This channel updates the target node to have its
 * transform match the combined value of its
 * translation, rotation, and scale properties every tick.
 *
 * This channel does not animate the translation, rotation,
 * or scale itself. Instead, you should
 * create other channels that modify the properties of this one,
 * add them to the timeline's list of channels,
 * then add this channel last.
 *
 * After the other channels update this channel's
 * translation, rotation, and scale, it can then apply
 * those to the targeted node.
 */
class TransformChannel : public ::animations::Channel {
public:
    /**
     * @param target The node who's transform is
     * being updated by the animation.
     *
     * The values of translation, rotation, and scale
     * will be initialized to target's current values
     * by decomposing its transform.
     * @param target The node this channel will animate.
     */
    TransformChannel(sg::Node * target);

    /**
     * @param target The node who's transform is
     * being updated by the animation.
     * @param target The node this channel will animate.
     * @param translation Starting translation.
     * @param rotation Starting rotation.
     * @param scale Starting scale.
     */
    TransformChannel(sg::Node * target, const Eigen::Vector3f & translation, const Eigen::Quaternionf & rotation, const Eigen::Vector3f & scale);

    /**
     *
     */
    virtual ~TransformChannel() = default;

    /**
     * @return The node who's transform is
     * being updated by the animation.
     */
    sg::Node * getTarget() { return _target; }

    /**
     * @return Translation the target will be set to.
     */
    const Eigen::Vector3f & getTranslation() const { return _translation; }

    /**
     * @param translation Translation the target will be set to.
     */
    void setTranslation(const Eigen::Vector3f & translation) { _translation = translation; }

    /**
     * @return Rotation the target will be set to.
     */
    const Eigen::Quaternionf & getRotation() const { return _rotation; }

    /**
     * @param rotation Rotation the target will be set to.
     */
    void setRotation(const Eigen::Quaternionf & rotation) { _rotation = rotation; }

    /**
     * @return Scale the target will be set to.
     */
    const Eigen::Vector3f & getScale() const { return _scale; }

    /**
     * @param scale Scale the target will be set to.
     */
    void setScale(const Eigen::Vector3f & scale) { _scale = scale; }

    /**
     * This will simply set the target to the current values of the transform.
     */
    void setTime(std::chrono::duration<uint64_t, std::nano> time) override;

private:
    /**
     * The node who's transform is
     * being updated by the animation.
     */
    sg::Node * _target;

    /**
     * Translation the target will be set to.
     */
    Eigen::Vector3f _translation;

    /**
     * Rotation the target will be set to.
     */
    Eigen::Quaternionf _rotation;

    /**
     * Scale the target will be set to.
     */
    Eigen::Vector3f _scale;
};

} // namespace animations
