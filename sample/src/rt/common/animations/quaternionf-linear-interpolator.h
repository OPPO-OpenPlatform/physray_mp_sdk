/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : quaternionf-linear-interpolator.h
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
