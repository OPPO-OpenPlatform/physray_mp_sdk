/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : key-value.h
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

#include "interpolator.h"
#include "simple-linear-interpolator.h"

#include <ph/rt-utils.h>

#include <chrono>
#include <memory>

namespace animations {

/**
 * Represents a point in a timeline,
 * indicating what value the target should be.
 * <T> Type of the target being animated.
 */
template<typename T>
class KeyValue {
public:
    /**
     *
     * @param endValue The value channel should have when animation
     * reaches the time of this keyframe.
     * @param interpolator Allows animations to calculate
     * the interpolation between two values for a given type.
     */
    KeyValue(const T & endValue): _endValue(endValue), _interpolator(new animations::SimpleLinearInterpolator<T>()) {
        //
    }

    /**
     *
     * @param endValue The value channel should have when animation
     * reaches the time of this keyframe.
     * @param interpolator Allows animations to calculate
     * the interpolation between two values for a given type.
     */
    KeyValue(const T & endValue, std::shared_ptr<Interpolator<T>> interpolator): _endValue(endValue), _interpolator(interpolator) {
        //
    }

    /**
     *
     */
    virtual ~KeyValue() = default;

    /**
     * @param The value channel should have when animation
     * reaches the time of this keyframe.
     */
    T & getEndValue() { return _endValue; }

    /**
     * @return Allows animations to calculate
     * the interpolation between two values for a given type.
     */
    std::shared_ptr<Interpolator<T>> getInterpolator() { return _interpolator; }

private:
    /**
     * The value channel should have when animation
     * reaches the time of this keyframe.
     */
    T _endValue;

    /**
     * Allows animations to calculate
     * the interpolation between two values for a given type.
     */
    std::shared_ptr<Interpolator<T>> _interpolator;
};

} // namespace animations
