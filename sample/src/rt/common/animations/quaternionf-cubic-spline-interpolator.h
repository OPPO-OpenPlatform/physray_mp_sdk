/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

#include "interpolator.h"
#include "simple-cubic-spline-interpolator.h"

namespace animations {

/**
 * Handles cubic spline interpolation between two quaternions.
 * @param <T> Type being interpolated.
 */
class QuaternionfCubicSplineInterpolator : public Interpolator<Eigen::Quaternionf> {
public:
    /**
     * @param startTangent Tangent at the start of the interval.
     * @param endTangent Tangent at the end of the interval.
     */
    QuaternionfCubicSplineInterpolator(const Eigen::Quaternionf & startTangent, const Eigen::Quaternionf & endTangent)
        : xInterpolator(startTangent.x(), endTangent.x()), yInterpolator(startTangent.y(), endTangent.y()), zInterpolator(startTangent.z(), endTangent.z()),
          wInterpolator(startTangent.w(), endTangent.w()) {
        //
    }

    /**
     *
     */
    virtual ~QuaternionfCubicSplineInterpolator() = default;

    void interpolate(const Eigen::Quaternionf & startValue, const Eigen::Quaternionf & endValue, float fraction, Eigen::Quaternionf & interpolated) override {
        // Calculate the cubic spline of each component.
        xInterpolator.interpolate(startValue.x(), endValue.x(), fraction, interpolated.x());
        yInterpolator.interpolate(startValue.y(), endValue.y(), fraction, interpolated.y());
        zInterpolator.interpolate(startValue.z(), endValue.z(), fraction, interpolated.z());
        wInterpolator.interpolate(startValue.w(), endValue.w(), fraction, interpolated.w());

        // Normalize it back into a regular quaternion.
        interpolated.normalize();
    }

private:
    /**
     * Calculates the cubic spline of the x component.
     */
    ::animations::SimpleCubicSplineInterpolator<float> xInterpolator;

    /**
     * Calculates the cubic spline of the y component.
     */
    ::animations::SimpleCubicSplineInterpolator<float> yInterpolator;

    /**
     * Calculates the cubic spline of the z component.
     */
    ::animations::SimpleCubicSplineInterpolator<float> zInterpolator;

    /**
     * Calculates the cubic spline of the w component.
     */
    ::animations::SimpleCubicSplineInterpolator<float> wInterpolator;
};

} // namespace animations
