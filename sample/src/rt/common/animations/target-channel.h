/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "channel.h"
#include "key-value.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace animations {

/**
 * A channel that modifies a given target over time.
 * @param <T> Type being interpolated.
 */
template<typename T>
class TargetChannel : public Channel {
public:
    /**
     * @param target The item being updated by the animation.
     * Is called with what the new value should be every tick.
     */
    TargetChannel(std::function<void(T &)> target): _target(target) {
        //
    }

    /**
     *
     */
    virtual ~TargetChannel() = default;

    /**
     * @return The item being updated by the animation.
     * Is called with what the new value should be every tick.
     */
    std::function<void(T &)> & getTarget() { return _target; }

    /**
     * @return A sorted map, mapping times to the value target should
     * be at each time.
     */
    std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<KeyValue<T>>> & getTimeToKeyValue() { return _timeToKeyValue; }

    /**
     * Calculates what the value will be at a given time using the key values.
     * @param time The time we want to retrieve the value for.
     * @param value Reference is set to the value for time.
     * This will be unmodified if timeToKeyValue is empty.
     *
     * \TODO this method is extremely slow when the scene has lots of animations, primarily due to calling of std::map::upper_bound()
     *  and std::map::lower_bound(). Optimize it for continuously increasing time values.
     *
     */
    void getValueAtTime(std::chrono::duration<uint64_t, std::nano> time, T & value) {
        // If map of key values is empty, there is nothing to be done.
        if (_timeToKeyValue.empty()) { return; }

        // Get the upper bound, that is, the first key value greater than time.
        auto upperBoundIterator = _timeToKeyValue.upper_bound(time);

        // If time has reached or passed the last value.
        if (upperBoundIterator == _timeToKeyValue.end()) {
            // Fetch the last key value in the animation.
            auto lastValueIterator = std::prev(upperBoundIterator);

            // Set interpolated to the last value of the animation.
            value = lastValueIterator->second->getEndValue();

            // If time has not yet passed the last value.
        } else {
            // If we haven't even reached the first key.
            if (upperBoundIterator == _timeToKeyValue.begin()) {
                // The first key value is the starting value of the animation.
                // Since we are before it, initialize value to it.
                value = upperBoundIterator->second->getEndValue();

                // If we are between two key values.
            } else {
                // Get the lower bound, which would be the one just before the upper bound.
                auto lowerBoundIterator = std::prev(upperBoundIterator);

                // Interpolate the value between the lower and upper key values.
                interpolateIterators(lowerBoundIterator, upperBoundIterator, time, value);
            }
        }
    }

    /**
     * Sets the target to the value of the animation at the given time,
     * calculating it from the mapping of timeToKeyValue.
     * If timeToKeyValue is empty, target will be set to match a default
     * instance of type T.
     * @param time Time in the animation we want target to be set to.
     */
    void setTime(std::chrono::duration<uint64_t, std::nano> time) override {
        // Stores the interpolated value the animation is currently at.
        T value;

        // Fetch the value at time.
        getValueAtTime(time, value);

        // Assign the interpolated value to the target.
        _target(value);
    }

    std::chrono::duration<uint64_t, std::nano> getDuration() override {
        // If keyvalues are empty.
        if (_timeToKeyValue.empty()) {
            // Return time zero.
            return std::chrono::nanoseconds::zero();

            // If key values are not empty.
        } else {
            // Return the time of the last item in the map.
            return std::prev(_timeToKeyValue.end())->first;
        }
    }

protected:
    /**
     * Calculates the normalized value of time relative to two key frames.
     * @param startTime Time of the previous keyframe.
     * @param endTime Time of the next keyframe.
     * @param time A time between start and endtime that we want
     * to map to the range [0..1].
     * @return a value remapped from [startTime..endTime] to [0..1],
     * representing the normalized time.
     * Note that it can be lower than 0 if time is less than startTime
     * and greater than 1 if time is greater than endTime.
     */
    static float normalizeTime(std::chrono::duration<uint64_t, std::nano> startTime, std::chrono::duration<uint64_t, std::nano> endTime,
                               std::chrono::duration<uint64_t, std::nano> time) {
        // Calculate the total amount of time between the 2 frames.
        std::chrono::duration<float, std::nano> totalTime = (endTime - startTime);

        // Calculate time compared to the start.
        std::chrono::duration<float, std::nano> relativeTime = (time - startTime);

        // Calculate the normalized time.
        return relativeTime / totalTime;
    }

    /**
     * Calculates the interpolated value between two iterators in the key value map.
     * @param start The previous keyframe.
     * @param end The next keyframe.
     * @param time A time relative to the range start and endtime that we want
     * to get the interpolated value for.
     * @param interpolated Will be assigned the result of the operation,
     * the interpolated result relative to the 2 keys that parameter time represents.
     * @param <KeyValueIterator> Iterator type used to iterate key values.
     */
    template<class KeyValueIterator>
    static void interpolateIterators(KeyValueIterator start, KeyValueIterator end, std::chrono::duration<uint64_t, std::nano> time, T & interpolated) {
        // Fetch the upper key value since that is what will be handling the interpolation
        // and will therefore be used more than once in this function.
        KeyValue<T> * endKeyValue = end->second.get();

        // Calculate the interpolated value.
        // Calculate normalized time.
        float normalizedTime = normalizeTime(start->first, end->first, time);

        // Use the interpolator to calculate the interpolated value
        // and save it to the value variable.
        endKeyValue->getInterpolator()->interpolate(start->second->getEndValue(), endKeyValue->getEndValue(), normalizedTime, interpolated);
    }

private:
    /**
     * The item being updated by the animation.
     * Is called with what the new value should be every tick.
     */
    std::function<void(T &)> _target;

    /**
     * A sorted map, mapping times to the value target should
     * be at each time.
     */
    std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<KeyValue<T>>> _timeToKeyValue;
};

} // namespace animations
