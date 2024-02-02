/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

#include "interpolator.h"

namespace animations {

/**
 * Cubic Spline interpolation.
 * Returns startValue if fraction is any value below 1
 * and endValue if it is greater than or equal to one.
 *
 * The types of T and Tangent must support being multiplied by doubles
 * and then being added to one another.
 * @param <T> Type being interpolated.
 * @param <Tangent> Type being used as a tangent.
 */
template<typename T, typename Tangent = T>
class SimpleCubicSplineInterpolator : public Interpolator<T> {
public:
    /**
     *
     * @param startTangent Tangent at the start of the interval.
     * @param endTangent Tangent at the end of the interval.
     */
    SimpleCubicSplineInterpolator(const Tangent & startTangent, const Tangent & endTangent): _startTangent(startTangent), _endTangent(endTangent) {
        //
    }

    /**
     *
     */
    virtual ~SimpleCubicSplineInterpolator() = default;

    void interpolate(const T & startValue, const T & endValue, float fraction, T & interpolated) override {
        // Calculate fraction at different powers.
        float t  = fraction;
        float t2 = t * t;
        float t3 = t2 * t;

        // Calculate cubic spline interpolation and cast it.
        interpolated = (T) ((2.0f * t3 - 3.0f * t2 + 1.0f) * startValue + (t3 - 2.0f * t2 + t) * _startTangent + (-2.0f * t3 + 3.0f * t2) * endValue +
                            (t3 - t2) * _endTangent);
    }

private:
    /**
     * Tangent at the start of the interval.
     */
    Tangent _startTangent;

    /**
     * Tangent at the end of the interval.
     */
    Tangent _endTangent;
};

} // namespace animations
