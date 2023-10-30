/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include "touch-event.h"
#include "rt/common/first-person-controller.h"

#include <android/native_window.h>

#include <unordered_set>

/// Manipulates the first person controller according to how the user
/// drags their fingers across the screen.
/// Dragging on the left side of the screen will translate forward, backward, left, and right.
/// Dragging on the right side of the screen will rotate.
class DragMotionController {
public:
    DragMotionController()  = default;
    ~DragMotionController() = default;

    /// Pass the touch event.
    void onTouch(const TouchEvent & curr);

    void setWindow(ANativeWindow * window) { _window = window; }

    void setFirstPersonController(FirstPersonController * firstPersonController) { _firstPersonController = firstPersonController; }

    void setThreshold(float threshold) { _threshold = threshold; }

    void setSpeedMultiplier(const Eigen::Vector3f & speedMultiplier) { _speedMultiplier = speedMultiplier; }

private:
    /// Surface touch events are being received from.
    ANativeWindow * _window;

    /// Size of the window last time we updated.
    float _windowWidth = 1.0f;

    /// Width or height of the window, whichever was smaller, last time we updated.
    float _windowMinSize = 1.0f;

    /// Touch id of the touch being used for translation. If it's -1, that means no touch selected.
    int _translateTouchId = -1;

    /// Starting position of the touch being used for translation.
    Eigen::Vector2f _translateStartingPosition;

    // Touch id of the touch being used for rotation. If it's -1, that means no touch selected.
    int _rotateTouchId = -1;

    /// The controller this is manipulating.
    FirstPersonController * _firstPersonController;

    /// How far from starting position before we start moving.
    float _threshold = 8.0f;

    /// Treating the smaller dimension of the screen as "1", multiplies how fast the motion controller moves.
    Eigen::Vector3f _speedMultiplier = Eigen::Vector3f::Ones();
    void            initializeTouch(const Touch & touch);
    void            updateTranslateTouch(const Touch & touch);
    void            updateRotateTouch(const Touch & touch);
};
