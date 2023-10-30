/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "interpolator.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace animations {

/**
 * Simple step interpolation.
 * Returns startValue if fraction is any value below 1
 * and endValue if it is greater than or equal to one.
 * @param <T> Type being interpolated.
 */
template<typename T>
class StepInterpolator : public Interpolator<T> {
public:
    /**
     *
     */
    StepInterpolator() = default;

    /**
     *
     */
    virtual ~StepInterpolator() = default;

    void interpolate(const T & startValue, const T & endValue, float fraction, T & interpolated) override {
        // If any value below 1, use the start value.
        if (fraction < 1.0) {
            interpolated = startValue;

            // If 1 or greater, use the end value.
        } else {
            interpolated = endValue;
        }
    }

private:
};

} // namespace animations
