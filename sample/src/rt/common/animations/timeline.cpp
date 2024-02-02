/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#include "pch.h"
#include "timeline.h"

namespace animations {

Timeline::Timeline(): _time(), _duration(), _start(std::chrono::duration<uint64_t, std::nano>::zero()), _rate(1.0), _playCount(0), _repeatCount(1) {
    //
}

void Timeline::setTime(std::chrono::duration<uint64_t, std::nano> time) {
    // Save the new time.
    _time = (time < _start) ? _start : time;

    // Update all of the channels.
    setChannelTime(time);
}

void Timeline::tick(std::chrono::duration<uint64_t, std::nano> elapsedTime) {
    // If timeline is empty, there is nothing to be done
    // and we want to avoid divide by zero problems.
    if (_duration == std::chrono::duration<uint64_t, std::nano>::zero()) { return; }
    if (getDurationFromStart() == std::chrono::duration<uint64_t, std::nano>::zero()) { return; }

    // Records what time will be after the tick is complete.
    std::chrono::duration<uint64_t, std::nano> nextTime;

    // If rate is negative.
    if (_rate < 0.0) {
        // Calculate the positive equivelent to rate.
        double inverseRate = _rate * -1.0;

        // Calculate how much time will pass so we know how much to subtract from time.
        std::chrono::duration<uint64_t, std::nano> scaledElapsedTime((uint64_t) (inverseRate * elapsedTime.count()));

        // If tick would bring us before the start.
        if (scaledElapsedTime > getTimeFromStart()) {
            // If we still had some repeats previous tick.
            if (hasRepeatsLeft()) {
                // Calculate how far before the beginning time has gone.
                std::chrono::duration<uint64_t, std::nano> timeBeforeStart = scaledElapsedTime - getTimeFromStart();

                // Update the playCount by how many times we have gone past the beginning.
                // We just went past the beginning now, and if timeBeforeStart is big enough,
                // we may well have gone past it a further (timeBeforeStart / _duration) times.
                _playCount = _playCount + 1 + (uint32_t) (timeBeforeStart / getDurationFromStart());

                // If we still have repeats this tick after using one or more up.
                if (hasRepeatsLeft()) {
                    // Wrap time around from the end by how much before the beginning we went.
                    // Just in case we somehow looped around multiple times, use the remainder.
                    nextTime = _duration - timeBeforeStart % getDurationFromStart();

                    // If we used up all of our repeats.
                } else {
                    // Then clamp time to start.
                    nextTime = _start;
                }

                // If we have used up all of our repeats.
            } else {
                // Then clamp time to start.
                nextTime = _start;
            }

            // If the next time would still be in range [start..duration].
        } else {
            // Calculate next time.
            nextTime = _time - scaledElapsedTime;
        }

        // If rate is positive.
    } else {
        // Calculate how much time will pass.
        std::chrono::duration<uint64_t, std::nano> scaledElapsedTime((uint64_t) (_rate * elapsedTime.count()));

        // Calculate what time will be after this tick.
        nextTime = _time + scaledElapsedTime;

        // If the time is greater than end time.
        if (nextTime > _duration) {
            // If we still had some repeats previous tick.
            if (hasRepeatsLeft()) {
                // Calculate how far after the end time has gone.
                std::chrono::duration<uint64_t, std::nano> timeAfterEnd = nextTime - _duration;

                // Update the playCount by how many times we have gone past the end.
                // We just went past the end now, and if timeAfterEnd is big enough,
                // we may well have gone past it a further (timeAfterEnd / _duration) times.
                _playCount = _playCount + 1 + (uint32_t) (timeAfterEnd / getDurationFromStart());

                // If we still have repeats this tick after using one or more up.
                if (hasRepeatsLeft()) {
                    // Wrap time around from the beginning by how much after the end we went.
                    // Just in case we somehow looped around multiple times, use the remainder.
                    nextTime = timeAfterEnd % getDurationFromStart();

                    // If we used up all of our repeats this tick.
                } else {
                    // Then clamp time to the end.
                    nextTime = _duration;
                }

                // If we have used up all of our repeats.
            } else {
                // Then clamp time to the end.
                nextTime = _duration;
            }
        }
    }

    // Update the animation to the new time.
    setTime(nextTime);
}

void Timeline::tickMillis(uint64_t elapsedTimeMillis) {
    // Cast to the equivelant duration type and forward to the other tick method.
    tick(std::chrono::duration<uint64_t, std::milli>(elapsedTimeMillis));
}

void Timeline::updateChannels() {
    // Records the biggest time found out of all channels.
    std::chrono::duration<uint64_t, std::nano> largestDuration = std::chrono::duration<uint64_t, std::nano>::zero();

    // Determine which channel has the furthest time.
    for (std::size_t index = 0; index < _channels.size(); ++index) {
        // Fetch the channel to be updated.
        std::shared_ptr<Channel> & channel = _channels[index];

        // Save its time if its the biggest we've found.
        largestDuration = std::max(largestDuration, channel->getDuration());
    }

    // Update the duration.
    _duration = largestDuration;
}

void Timeline::playFromStart() {
    // Reset the number of times this has been played.
    setPlayCount(0);

    // Start time from the very beginning.
    setTime(std::chrono::nanoseconds::zero());
}

bool Timeline::hasRepeatsLeft() const {
    // We still have repeats left if repeat count is set to indefinite
    //(in which case we get to play an unlimited number of times)
    // or if play count has not yet reached repeat count.
    return _repeatCount == REPEAT_COUNT_INDEFINITE || _playCount < _repeatCount;
}

void Timeline::setChannelTime(std::chrono::duration<uint64_t, std::nano> time) {
    // Update all channels according to the new time.
    for (std::size_t index = 0; index < _channels.size(); ++index) {
        // Fetch the channel to be updated.
        std::shared_ptr<Channel> & channel = _channels[index];

        // Update the channel to the given time.
        channel->setTime(time);
    }
}

} // namespace animations
