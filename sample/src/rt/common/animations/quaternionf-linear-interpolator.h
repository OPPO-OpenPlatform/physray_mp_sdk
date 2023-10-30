/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "interpolator.h"

namespace animations {

/**
 * Handles linear interpolation between two quaternions.
 * @param <T> Type being interpolated.
 */
class QuaternionfLinearInterpolator : public Interpolator<Eigen::Quaternionf> {
public:
    /**
     *
     */
    QuaternionfLinearInterpolator() = default;

    /**
     *
     */
    virtual ~QuaternionfLinearInterpolator() = default;

    void interpolate(const Eigen::Quaternionf & startValue, const Eigen::Quaternionf & endValue, float fraction, Eigen::Quaternionf & interpolated) override {
        interpolated = startValue.slerp(fraction, endValue);
    }

private:
};

} // namespace animations
