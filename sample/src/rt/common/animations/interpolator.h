/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

namespace animations {

/**
 * Allows animations to calculate the interpolation
 * between two values for a given type.
 * @param <T> Type being interpolated.
 */
template<typename T>
class Interpolator {
public:
    /**
     *
     */
    Interpolator() = default;

    virtual ~Interpolator() = default;

    /**
     *
     * @param startValue The start value interpolated is being interpolated between.
     * @param endValue The end value interpolated is being interpolated between.
     * @param fraction A value mapping the range [0.0..1.0] to
     * values interpolated between start and end, calculating the value
     * interpolated should be set to.
     * Implementations should support values beyond the range [0.0..1.0]
     * as well for certain animation implementations.
     * @param interpolated Will be assigned the result of the operation.
     */
    virtual void interpolate(const T & startValue, const T & endValue, float fraction, T & interpolated) = 0;

private:
};

} // namespace animations
