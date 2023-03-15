/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : touch-event.h
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

#pragma once

#include <ph/va.h> // this is to include the Eigne header included by va.h

#include <algorithm>
#include <sstream>
#include <set>

/// Represents one finger against the screen.
class Touch {
public:
    Touch() = default;

    /// @param id Uniquely identifies this touch until it has been released.
    /// @param x Current x coordinate of this touch.
    /// @param y Current y coordinate of this touch.
    Touch(int32_t id, float x, float y): _id(id), _position(x, y) {}

    bool operator==(const Touch & rhs) const { return _id == rhs._id && _position == rhs._position; }

    /// @return Uniquely identifies this touch until it has been released.
    int32_t id() const { return _id; };

    /// @return Touch's position on the screen.
    const Eigen::Vector2f & position() const { return _position; }

    float x() const { return _position.x(); }

    float y() const { return _position.y(); }

private:
    /// Uniquely identifies this touch.
    int32_t _id = 0;

    /// Touch's position on the screen.
    Eigen::Vector2f _position = Eigen::Vector2f::Zero();
};

/// Represents one or more fingers touching the screen.
class TouchEvent {
public:
    TouchEvent(const std::vector<Touch> & touches) {
        // We sometimes see the same ID being reported multiple times in the event list. When that happens,
        // only the first occurrence contains valid data.
        std::set<int> idList;
        _touches.reserve(touches.size());
        for (const auto & t : touches) {
            if (!idList.insert(t.id()).second) continue;
            _touches.push_back(t);
        }
    }

    /// @return Distance in pixels between the 2 most distant pointers.
    /// Will return zero if there were fewer than 2 pointers.
    float distance() const {
        // If there aren't enough points for there to be distance, then just return zero.
        if (_touches.size() <= 1) { return 0; }

        // Biggest square distance we have found so far.
        float largestSquareDistance = 0;

        // Number of touches to check since checking too many would be needlessly expensive.
        size_t touchCount = std::min(_touches.size(), (size_t) 6);

        // Check against the 1st few points.
        for (size_t i = 0; i < touchCount; ++i) {
            // Get 1st point being checked.
            const Eigen::Vector2f & position1 = _touches[i].position();

            for (size_t j = i + 1; j < touchCount; ++j) {
                // Get 2nd point being checked.
                const Eigen::Vector2f & position2 = _touches[j].position();

                // Calculate the square distance between these 2 points.
                Eigen::Vector2f difference     = position2 - position1;
                float           squareDistance = difference.x() * difference.x() + difference.y() * difference.y();

                // If distance is bigger than the largest found, record it.
                if (squareDistance > largestSquareDistance) { largestSquareDistance = squareDistance; }
            }
        }

        // Finish calculating the distance formula and return it.
        return std::sqrt(largestSquareDistance);
    }

    /// Used to specify a discrete position despite having multiple pointers on the screen,
    /// since the camera movement functions require a single position.
    /// @return Average x coordinate of all touches.
    float midpointX() const {
        // Add the cumulative coordinates.
        float sum = 0.0f;
        for (const Touch & touch : _touches) { sum += touch.position().x(); }

        // Calculate the average.
        return sum / ((float) _touches.size());
    }

    /// Used to specify a discrete position despite having multiple pointers on the screen,
    /// since the camera movement functions require a single position.
    /// @return Average y coordinate of all touches.
    float midpointY() const {
        // Add the cumulative coordinates.
        float sum = 0.0f;
        for (const Touch & touch : _touches) { sum += touch.position().y(); }

        // Calculate the average.
        return sum / ((float) _touches.size());
    }

    size_t size() const { return _touches.size(); }

    bool empty() const { return _touches.empty(); }

    std::string toString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    /// Helper function to append to the end of a stream.
    friend inline std::ostream & operator<<(std::ostream & os, const TouchEvent & event) {
        for (const auto & t : event._touches) { os << "[" << t.id() << ", " << t.x() << ", " << t.y() << "] "; }
        return os;
    }

    const Touch & operator[](size_t i) const {
        PH_ASSERT(i < _touches.size());
        return _touches[i];
    }

    bool operator==(const TouchEvent & rhs) const { return _touches == rhs._touches; }

private:
    /// Collection of touches hitting the screen.
    std::vector<Touch> _touches;
};
