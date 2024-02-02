/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "channel.h"
#include "../morphtargets.h"

#include <algorithm>
#include <chrono>

namespace animations {

class WeightChannel : public ::animations::Channel {
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
    WeightChannel(sg::Node * target, MorphTargetManager * morphTargetManager);

    /**
     * @param target The node this channel will animate.
     * @param weights Starting weights
     */
    WeightChannel(sg::Node * target, MorphTargetManager * morphTargetManager, const std::vector<float> weights);

    /**
     *
     */
    virtual ~WeightChannel() = default;

    /**
     * @return The node who's transform is
     * being updated by the animation.
     */
    sg::Node * getTarget() { return _target; }

    const std::vector<float> & getWeights() const { return _weights; }
    void                       setWeights(const std::vector<float> & weights) { _weights = weights; }

    size_t getStride() const { return _weights.size(); }

    /**
     * This will simply set the target to the current values of the transform.
     */
    void setTime(std::chrono::duration<uint64_t, std::nano> time) override;

private:
    /**
     * The node whose mesh has blend weights
     * being updated by the animation.
     */
    sg::Node * _target;

    const ph::rt::Mesh * _mesh;

    MorphTargetManager * _morphTargetManager;

    /**
     * Translation the target will be set to.
     */
    std::vector<float> _weights;
};

} // namespace animations