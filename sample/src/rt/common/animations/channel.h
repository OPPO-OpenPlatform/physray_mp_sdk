/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : channel.h
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

#include <chrono>

namespace animations {

/**
 * Represents one entity that is animated over time.
 */
class Channel {
public:
    /**
     *
     */
    Channel() = default;

    /**
     *
     */
    virtual ~Channel() = default;

    /**
     * Updates the channel to the state it should have at the given time.
     */
    virtual void setTime(std::chrono::duration<uint64_t, std::nano> time) = 0;

    /**
     * @return The last time the channel modifies the animation.
     * Is used to determine the duration of the timeline.
     * Default implementation returns time zero.
     */
    virtual std::chrono::duration<uint64_t, std::nano> getDuration() { return std::chrono::nanoseconds::zero(); }

private:
};

} // namespace animations
