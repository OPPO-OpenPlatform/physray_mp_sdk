/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : simple-linear-interpolator.h
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
 * Performs linear interpolation using the type's
 * subtraction operator, double multiplier operator,
 * and addition operator.
 *
 * Interpolation is equivelant to:
 * interpolated = startValue + (T) (fraction * (endValue - startValue)).
 * @param <T> Type being interpolated.
 */
template<typename T>
class SimpleLinearInterpolator : public Interpolator<T> {
public:
    /**
     *
     */
    SimpleLinearInterpolator() = default;

    /**
     *
     */
    virtual ~SimpleLinearInterpolator() = default;

    virtual void interpolate(const T & startValue, const T & endValue, float fraction, T & interpolated) override {
        // Calculate the gap between start and end.
        T distance = endValue - startValue;

        // Add the desired fraction of the gap to start and
        // save it to result.
        interpolated = startValue + (T) (fraction * distance);
    }

private:
};

} // namespace animations
