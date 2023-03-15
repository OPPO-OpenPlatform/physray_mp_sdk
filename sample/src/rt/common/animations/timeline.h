/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : timeline.h
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

#include "channel.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace animations {

/**
 * This class represents a keyframe animation.
 *
 * The following is an example of how to use this class.
 * @code
 * //Create the keyframe animation.
 * Timeline timeline;
 *
 * //Create a channel to be animated by the timeline.
 * std::shared_ptr<animations::TargetChannel<float>> printTimeChannel(new animations::TargetChannel<float>(
 *     [](float& value) {
 *         //Print the current value of the channel every tick.
 *         PH_LOGI("Timeline: %f", value);
 *     }
 *     ));
 *
 * //Time of the first keyframe.
 * std::chrono::milliseconds startTime(0);
 *
 * //Time of the last keyframe.
 * std::chrono::milliseconds endTime(1000);
 *
 * //At the beginning of the animation, channel's value starts at zero.
 * printTimeChannel->getTimeToKeyValue()[startTime].reset(new animations::KeyValue<float>(0.0f));
 *
 * //Over the course of a second, the channel's value slowly increases to one.
 * printTimeChannel->getTimeToKeyValue()[endTime].reset(new animations::KeyValue<float>(1.0f));
 *
 * //Save the channel to the timeline so it knows to update it.
 * timeline.getChannels().push_back(printTimeChannel);
 *
 * //Setup timeline.
 * timeline.updateChannels();
 *
 * //Start the game loop.
 * while (true) {
 *     //Progress the animation by 250 milliseconds.
 *     timeline.tickMillis(250);
 *
 *     //Wait for 250 millisecods.
 *     sleep(250);
 * }
 *
 * //This should result in the following printouts:
 * //Time 0ms: 0.0
 * //Time 250ms: 0.25
 * //Time 500ms: 0.5
 * //Time 750ms: 0.75
 * //Time 1000ms: 1.0
 * //Time 1250ms: 1.0
 * //etc.
 * @endcode
 *
 * If you wish for the animation to continue playing
 */
class Timeline {
public:
    /**
     * Value to set repeat count to if you want to timeline to replay forever.
     */
    static const uint32_t REPEAT_COUNT_INDEFINITE = std::numeric_limits<uint32_t>::max();

    /**
     *
     */
    Timeline();

    /**
     *
     */
    virtual ~Timeline() = default;
    std::chrono::duration<uint64_t, std::nano> getStart() const { return _start; }
    void                                       setStart(std::chrono::duration<uint64_t, std::nano> start) {
        if (start < std::chrono::duration<uint64_t, std::nano>::zero())
            _start = std::chrono::duration<uint64_t, std::nano>::zero();
        else if (start > _duration)
            _start = _duration;
        else
            _start = start;
    }

    /**
     * @return The current time in the animation, which will
     * be clamped to the range [getStart()..getDuration()].
     */
    std::chrono::duration<uint64_t, std::nano> getTime() const { return _time; }
    std::chrono::duration<uint64_t, std::nano> getTimeFromStart() const { return _time - _start; }

    /**
     * @param time The distance from start time in
     * the animation you want this to be set to.
     */
    void setTime(std::chrono::duration<uint64_t, std::nano> time);

    /**
     * @return How long the animation is.
     */
    std::chrono::duration<uint64_t, std::nano> getDuration() const { return _duration; }
    std::chrono::duration<uint64_t, std::nano> getDurationFromStart() const { return _duration - _start; }

    /**
     * @return Multipilies how fast the animation is being played.
     * Defaults to 1.0. Can be set to negative to make it play in reverse.
     */
    double getRate() const { return _rate; }

    /**
     * @param Multipilies how fast the animation is being played.
     * Defaults to 1.0. Can be set to negative to make it play in reverse.
     */
    void setRate(double rate) { _rate = rate; }

    /**
     * @return playCount  Number of times this has played since starting the animation.
     * Is incremented every time animation plays to before startTime
     * or after endTime.
     */
    uint32_t getPlayCount() const { return _playCount; }

    /**
     * @param playCount Number of times this has played since starting the animation.
     * Is incremented every time animation plays to before startTime
     * or after endTime.
     *
     * You can call this directly to reset it.
     */
    void setPlayCount(uint32_t playCount) { _playCount = playCount; }

    /**
     * @return Number of times to let playCount increment
     * before halting the animation. If set
     * to REPEAT_COUNT_INDEFINITE, it will
     * continue playing forever.
     */
    uint32_t getRepeatCount() const { return _repeatCount; }

    /**
     * @param repeatCount Number of times to let playCount increment
     * before halting the animation. If set
     * to REPEAT_COUNT_INDEFINITE, it will
     * continue playing forever.
     */
    void setRepeatCount(uint32_t repeatCount) { _repeatCount = repeatCount; }

    /**
     * @return List of channels being animated by the timeline.
     * Each channel will be updated in the same order as they are
     * in in the list, meaning you can add a channel that is
     * dependant on previous channels in a higher index
     * in the collection.
     */
    std::vector<std::shared_ptr<Channel>> & getChannels() { return _channels; }

    /**
     * Updates the animation, adding elapsed time to time.
     * Will update the play count if we pass the end.
     * @param elapsedTime Amount to add to time.
     */
    void tick(std::chrono::duration<uint64_t, std::nano> elapsedTime);

    /**
     * Updates the animation, adding elapsed time to time.
     * Will update the play count if we pass the end.
     * @param elapsedTimeMillis Amount to add to time in milliseconds.
     */
    void tickMillis(uint64_t elapsedTimeMillis);

    /**
     * Sets up the timeline according to the current list of channels.
     * This should be called any time you modify the list of channels.
     */
    void updateChannels();

    /**
     * Resets playCount to 0
     * and sets time to zero.
     */
    void playFromStart();

    std::string name = "";

private:
    /**
     * The distance from start time in
     * the animation you want this to be set to.
     */
    std::chrono::duration<uint64_t, std::nano> _time;

    /**
     * How long the animation is.
     */
    std::chrono::duration<uint64_t, std::nano> _duration;

    /**
     * The start time of the animation. Time will be clamped
     * to this minimum value.
     */
    std::chrono::duration<uint64_t, std::nano> _start;

    /**
     * Multipilies how fast the animation is being played.
     * Defaults to 1.0. Can be set to negative to make it play in reverse.
     */
    double _rate;

    /**
     * Number of times this has played since starting the animation.
     * Is incremented every time animation plays to before startTime
     * or after endTime.
     */
    uint32_t _playCount;

    /**
     * Number of times to let playCount increment
     * before halting the animation. If set
     * to REPEAT_COUNT_INDEFINITE, it will
     * continue playing forever.
     */
    uint32_t _repeatCount;

    /**
     * List of channels being animated by the timeline.
     * Each channel will be updated in the same order as they are
     * in in the list, meaning you can add a channel that is
     * dependant on previous channels in a higher index
     * in the collection.
     */
    std::vector<std::shared_ptr<Channel>> _channels;

    /**
     * @return true if timeline still has some repeat counts left.
     */
    bool hasRepeatsLeft() const;

    /**
     * Updates all the channels to the given time.
     * @param time The time to set all the channels to.
     */
    void setChannelTime(std::chrono::duration<uint64_t, std::nano> time);
};

} // namespace animations
