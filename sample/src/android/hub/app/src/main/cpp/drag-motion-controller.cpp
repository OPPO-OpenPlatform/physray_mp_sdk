/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : drag-motion-controller.cpp
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

#include "drag-motion-controller.h"

void DragMotionController::onTouch(const TouchEvent & curr) {
    // Do not correspond to the user's touch event, when _firstPersonController is not initialized
    if (_firstPersonController == nullptr) return;

    // Make sure our information on the window's size is still accurate.
    _windowWidth = ((float) ANativeWindow_getWidth(_window));

    // Update whichever window dimension is smaller.
    _windowMinSize = std::min(_windowWidth, ((float) ANativeWindow_getHeight(_window)));

    // PH_LOGI(s("touch event: ") << curr << ", trans id = " << _translateTouchId << ", rot id = " << _rotateTouchId);

    // Iterate touches.
    bool                t = false;    // flag indicating if translation is processed.
    bool                r = false;    // flag indicating if rotation is processed.
    std::vector<size_t> unrecognized; // List of touch indices that were not used to translate or rotate.

    for (size_t index = 0; index < curr.size(); ++index) {
        // Current touch being processed.
        const Touch & touch = curr[index];

        // Check if this is the touch being used to translate or rotate.
        // Only 1 touch may control either of these at a time,
        // so if it fits neither, it will be ignored.
        if (touch.id() == _translateTouchId) {
            updateTranslateTouch(touch);
            t = true;
        } else if (touch.id() == _rotateTouchId) {
            updateRotateTouch(touch);
            r = true;
        } else {
            // This is an touch id that we don't recognize. Remember it for now.
            unrecognized.push_back(index);
        }
    }

    // If translation was not processed, but we did have a touch assigned to control it.
    if (!t && _translateTouchId >= 0) {
        // We have processed all touch event. But none of them has the translation ID.
        // This could mean only one thing that the finger that was used to translate the camera has lift up.
        // So we turn off all translation keys and stop moving the camera.
        _firstPersonController->onKeyPress(FirstPersonController::MOVE_L, false);
        _firstPersonController->onKeyPress(FirstPersonController::MOVE_R, false);
        _firstPersonController->onKeyPress(FirstPersonController::MOVE_F, false);
        _firstPersonController->onKeyPress(FirstPersonController::MOVE_B, false);

        // Clear translate touch so it can be used again.
        _translateTouchId = -1;
    }

    // If rotation was not processed, but we did have a touch assigned to control it.
    if (!r && _rotateTouchId >= 0) {
        // Similar as the translation logic above. The rotation finger has lift up. Need to stop rotation.
        _firstPersonController->onKeyPress(FirstPersonController::LOOK, false);

        // Clear rotate touch so it can be used again.
        _rotateTouchId = -1;
    }

    // process unrecognized events
    for (auto index : unrecognized) {
        const Touch & touch = curr[index];
        initializeTouch(touch);
    }
}

void DragMotionController::initializeTouch(const Touch & touch) {
    // Calculate the middle of the window.
    float halfWidth = _windowWidth * 0.5f;

    // Check which side of the screen touch is on.
    bool isLeft = touch.position().x() < halfWidth;

    // If this touch started on the left side of the screen,
    // then it is for translation.
    if (isLeft) {
        // PH_LOGI("new touch on left.");
        // If we already have a touch controlling the translation, then ignore this touch.
        // If we do not already have a touch controlling the translation, then use it.
        if (_translateTouchId == -1) {
            // PH_LOGI("start translating.");

            // Record that we are using this touch for translation.
            _translateTouchId = touch.id();

            // Record the translate touch's starting position.
            _translateStartingPosition = touch.position();
        }

        // If this touch started on the right side of the screen,
        // then it is for rotation.
    } else {
        // PH_LOGI("new touch on right.");
        // If we already have a touch controlling the rotation, then ignore this touch.
        // If we do not already have a touch controlling the rotation, then use it.
        if (_rotateTouchId == -1) {
            // PH_LOGI("start rotating.");

            // Record that we are using this touch for rotation.
            _rotateTouchId = touch.id();

            // set camera to look mode
            _firstPersonController->onKeyPress(FirstPersonController::LOOK, true);
        }
    }
}

void DragMotionController::updateTranslateTouch(const Touch & touch) {
    // Get position of the current pointer being processed.
    const Eigen::Vector2f & position = touch.position();

    // Calculate the distance between the starting and current position.
    Eigen::Vector2f positionDifference = position - _translateStartingPosition;

    // Records the speed at which the camera controller will be moving.
    // This will be calculated based on how far from the origin the touch is along both axis.
    Eigen::Vector3f speed = Eigen::Vector3f::Zero();

    // Absolute value of y, used to determine whether we are distant
    // enough to move along y axis and by how much.
    auto absY = std::abs(positionDifference.y());
    // Absolute value of x, used to determine whether we are distant
    // enough to move along x axis and by how much.
    auto absX = std::abs(positionDifference.x());

    // If the touch has moved far away enough from its origin along the y axis, then we should move along it.

    if (absY > absX) {
        if (absY > _threshold) {
            // Check if we are moving forward or backward, depending on whether y is above or below the origin.
            bool forward = positionDifference.y() < 0;

            // Update whether we are moving forward or backward.
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_F, forward);
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_B, !forward);

            // Treat min window dimension as a speed of "1" then apply the multiplier.
            // Moving forward/backward is moving in Z direction, in camera space.
            // So here we need to update speed.z().
            speed.z() = absY * _speedMultiplier.z() / _windowMinSize;

            // If the touch is too close to its origin along the y axis.
        } else {
            // Stop any forward/backward movement already in progress.
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_F, false);
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_B, false);
        }
    } else {
        // If the touch has moved far away enough from its origin along the x axis, then we should move along it.
        if (absX > _threshold) {
            // Check if we are moving left or right, depending on whether x is above or below the origin.
            bool left = positionDifference.x() < 0;

            // Update whether we are moving left or right.
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_L, left);
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_R, !left);

            // Treat min window dimension as a speed of "1" then apply the multiplier.
            speed.x() = absX * _speedMultiplier.x() / _windowMinSize;

            // If the touch is too close to its origin along the x axis.
        } else {
            // Stop any left/right movement already in progress.
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_L, false);
            _firstPersonController->onKeyPress(FirstPersonController::MOVE_R, false);
        }
    }

    // Update how fast we should move based on the touch's distance from its starting point.
    _firstPersonController->setMoveSpeed(speed);
}

void DragMotionController::updateRotateTouch(const Touch & touch) {
    // Rotate the screen by how much the pointer has been moved.
    _firstPersonController->onMouseMove(touch.x(), touch.y());
}
