/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "first-person-controller.h"
#include <cmath>

using namespace ph;
using namespace Eigen;

// ---------------------------------------------------------------------------------------------------------------------
/// eular angle (in radian) -> rotation
static inline Quaternionf euler(const Vector3f & e) {
    auto rx = AngleAxis(e.x(), Vector3f::UnitX());
    auto ry = AngleAxis(e.y(), Vector3f::UnitY());
    auto rz = AngleAxis(e.z(), Vector3f::UnitZ());
    auto r  = rz * ry * rx;
    return r;
}

// ---------------------------------------------------------------------------------------------------------------------
/// Limit rotation angle within valid range. This method need to be called everytime _angle is updated.
Eigen::Vector3f FirstPersonController::limitRotationAngle(const Vector3f & a) {
    if (orbiting())
        return {
            std::clamp(a.x(), _rollLimits.x(), _rollLimits.y()), std::clamp(a.y(), _pitchLimits.x(), _pitchLimits.y()),
            a.z(), // unused in orbiting mode
        };
    else
        return {
            std::clamp(a.x(), _pitchLimits.x(), _pitchLimits.y()),
            a.y(), // no limiting on Yaw
            std::clamp(a.z(), _rollLimits.x(), _rollLimits.y()),
        };
}

static constexpr float INTERP_TIME = 0.05f;

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController::FirstPersonController() {
    _positionInterp.reset(_position, _position, INTERP_TIME);
    _rotationInterp.reset(_angle, _angle, INTERP_TIME);
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setTargetPosition(const Vector3f & p) {
    // limit target position within the bounding range.
    Eigen::Vector3f newPosition;
    if (_flythroughBoundary.isEmpty()) {
        newPosition = p;
    } else {
        newPosition.x() = clamp(p.x(), _flythroughBoundary.min().x(), _flythroughBoundary.max().x());
        newPosition.y() = clamp(p.y(), _flythroughBoundary.min().y(), _flythroughBoundary.max().y());
        newPosition.z() = clamp(p.z(), _flythroughBoundary.min().z(), _flythroughBoundary.max().z());
    }
    _positionInterp.reset(_position, newPosition, INTERP_TIME);
    if (orbiting()) { resetOrbitalParametersBasedOnCurrentCameraPosition(false); }
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setPosition(const Vector3f & p) {
    setTargetPosition(p);
    _position = _positionInterp.end();
    if (orbiting()) { resetOrbitalParametersBasedOnCurrentCameraPosition(true); }
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setTargetOrbitalCenter(const Eigen::Vector3f & p) {
    if (orbiting()) {
        // This means the camera is already in orbital mode. In this case, we just need to
        // reset the orbital center interpolator to match the new value.
        _orbitalCenterInterp.reset(_orbitalCenter.value(), p, INTERP_TIME);
    } else {
        // This means the camera is currently in fly-though mode and is switching to orbital mode.
        // In this case, other than updating the orbital center interpolator, we also need to calculate
        // orbital radius and angle via updateOrbitalParameters().
        _orbitalCenterInterp.reset(p, p, INTERP_TIME);
        resetOrbitalParametersBasedOnCurrentCameraPosition(false);
    }
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setOrbitalCenter(const Eigen::Vector3f * p) {
    if (p) {
        bool wasOrbiting = orbiting();
        _orbitalCenter   = *p;
        setTargetOrbitalCenter(*p);
        if (!wasOrbiting) { resetOrbitalParametersBasedOnCurrentCameraPosition(true); }
    } else {
        // clear orbital center. this will switch the camera back to fly-though mode.
        _orbitalCenter.reset();
    }
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setTargetOrbitalRadius(float r) {
    _orbitalRadiusInterp.reset(_orbitalRadius, std::min(_maximalRadius, std::max(_minimalRadius, r)), INTERP_TIME);
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setOrbitalRadius(float r) {
    PH_ASSERT(std::isfinite(r));
    _orbitalRadius = std::min(_maximalRadius, std::max(_minimalRadius, r));
    setTargetOrbitalRadius(r);
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setTargetAngle(const Vector3f & r) {
    _rotationInterp.reset(_angle, limitRotationAngle(r), INTERP_TIME);
    // TODO: update targetPosition
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
FirstPersonController & FirstPersonController::setAngle(const Vector3f & r) {
    _angle = limitRotationAngle(r);
    setTargetAngle(_angle);
    return *this;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void FirstPersonController::update(float elapsedSeconds) {
    if (orbiting())
        orbitingUpdate(elapsedSeconds);
    else
        flythroughUpdate(elapsedSeconds);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void FirstPersonController::onKeyPress(Key k, bool down) {
    if (k < 0 || k >= NUM_KEYS) return; // filter out invalid key

    bool wasLooking = _keys[LOOK];
    bool wasPanning = _keys[PAN];

    _keys[k] = down;

    if (!wasLooking && _keys[LOOK]) { _look.start = true; }

    if (!wasPanning && _keys[PAN]) { _pan.start = true; }
}

// ---------------------------------------------------------------------------------------------------------------------
//
void FirstPersonController::onMouseMove(float x, float y, float z) {
    if (_keys[LOOK]) {
        if (_look.start) {
            _look.start  = false;
            _look.startX = x;
            _look.startY = y;
            _look.startR = _angle;
        }
        Vector3f rotation(0, 0, 0);
        float    mouseLookSensitivityAdjusted = _mouseLookSensitivity * _handednessMultiplicationFactor;
        rotation.y()                          = (_look.startX - x) * mouseLookSensitivityAdjusted;
        rotation.x()                          = (_look.startY - y) * mouseLookSensitivityAdjusted;
        if (!rotation.isZero()) { setTargetAngle(_look.startR + rotation); }
    } else if (_keys[PAN]) {
        if (_pan.start) {
            _pan.start  = false;
            _pan.startX = x;
            _pan.startY = y;
            _pan.startZ = z;
            _pan.startP = _position;
            _pan.startO = _orbitalCenter.value_or(Eigen::Vector3f::Zero());
            _pan.startR = _orbitalRadius;
        }
        Vector3f movement(0, 0, 0);
        movement.x() = (x - _pan.startX) * _mouseMoveSensitivity;
        // Screen space is Y-down. World/Camera space is Y-up.
        movement.y() = (_pan.startY - y) * _mouseMoveSensitivity;
        movement.z() = (z - _pan.startZ) * _mouseMoveSensitivity;
        if (!movement.isZero()) {
            const auto & rotation = _worldTransform.rotation();
            if (orbiting()) {
                // To make the object move along with the mouse, we need to move
                // the orbital center to the opposite direction.
                movement.z() = 0.f;
                movement     = rotation * movement; // transform the movment to world space.
                setTargetOrbitalCenter(_pan.startO - movement);

                // update orbital radius
                float ratio = (0 == z) ? 1.0f : (_pan.startZ / z);
                float newR  = std::max(_pan.startR * std::clamp(ratio, 0.01f, 100.0f), _minimalRadius);
                setTargetOrbitalRadius(newR);
            } else {
                movement = rotation * movement; // transform the movment to world space.
                setTargetPosition(_pan.startP + movement);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
void FirstPersonController::onMouseWheel(float deltaZ) {
    if (_mouseWheelSensitivity == 0.f) return;
    if (orbiting()) {
        // In orbiting mode, mouse wheel affects orbit radius
        float oldR = _orbitalRadiusInterp.end();
        float diff = std::clamp(deltaZ * _mouseWheelSensitivity, -.25f, .25f);
        float newR = std::max(oldR * (1.f - diff), _minimalRadius);
        setTargetOrbitalRadius(newR);
    } else {
        // In flythrough ode, mouse wheel direclty affects camera position
        Eigen::Vector3f position = _positionInterp.end();
        position += Vector3f(0, 0, -deltaZ * _mouseWheelSensitivity);
        setTargetPosition(position);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
void FirstPersonController::resetOrbitalParametersBasedOnCurrentCameraPosition(bool immediateUpdate) {
    // update the orbital radius
    Eigen::Vector3f center2eye = _positionInterp.end() - _orbitalCenterInterp.end();
    float           radius     = center2eye.norm();
    if (immediateUpdate)
        setOrbitalRadius(radius);
    else
        setTargetOrbitalRadius(radius);

    // update the spherical angle
    //  _angle.x() is angle to X axis. Angle 0 points to +Z.
    //  _angle.y() is angle to Y axis. Angle 0 points to +Z;
    center2eye.normalize();
    auto x = -std::asin(center2eye.y());
    auto y = std::atan2(center2eye.x(), center2eye.z());
    auto a = Eigen::Vector3f(x, y, 0.f);
    if (immediateUpdate)
        setAngle(a);
    else
        setTargetAngle(a);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void FirstPersonController::flythroughUpdate(float elapsedSeconds) {
    // Process keyboard actions:
    //   +X: move right
    //   +Y: move up
    //   +Z: move back when right handed. foward when left handed.
    Vector3f delta_p(0, 0, 0);
    Vector3f delta_a(0, 0, 0);
    float    elapsedSecondsAdjusted = elapsedSeconds * _handednessMultiplicationFactor;
    if (_keys[MOVE_F]) delta_p.z() -= _moveSpeed.z() * elapsedSecondsAdjusted;
    if (_keys[MOVE_B]) delta_p.z() += _moveSpeed.z() * elapsedSecondsAdjusted;
    if (_keys[MOVE_L]) delta_p.x() -= _moveSpeed.x() * elapsedSeconds;
    if (_keys[MOVE_R]) delta_p.x() += _moveSpeed.x() * elapsedSeconds;
    if (_keys[MOVE_D]) delta_p.y() -= _moveSpeed.y() * elapsedSeconds;
    if (_keys[MOVE_U]) delta_p.y() += _moveSpeed.y() * elapsedSeconds;

    // FIXME: converting delta_a from radian to degree seem fixing the issue of keyboard rotation was too slow.
    // But the math doesn't make sense. In all other places, the rotation unit is defined in radian, not degree.
    // Something fishy is going on.
    constexpr float r2d = 180.0f / PI;
    if (_keys[TURN_U]) delta_a.x() += _rotateSpeed * elapsedSecondsAdjusted * r2d;
    if (_keys[TURN_D]) delta_a.x() -= _rotateSpeed * elapsedSecondsAdjusted * r2d;
    if (_keys[TURN_L]) delta_a.y() += _rotateSpeed * elapsedSecondsAdjusted * r2d;
    if (_keys[TURN_R]) delta_a.y() -= _rotateSpeed * elapsedSecondsAdjusted * r2d;

    // Update rotation
    if (!delta_a.isZero()) { setTargetAngle(_angle + delta_a); }
    _rotationInterp.update(elapsedSeconds);
    _angle        = limitRotationAngle(_rotationInterp.value());
    auto rotation = euler(_angle);

    // Update position. In flythrough mode, the camera position is determined by the position interpolator.
    if (!delta_p.isZero()) {
        // Do _NOT_ use auto to declare "t" for the reason described here:
        // http://eigen.tuxfamily.org/dox-devel/TopicPitfalls.html#title3
        // If you do, the whole calculation will result in garbage in release build.
        // In short, Eigen library is not auto friendly :(
        Eigen::Vector3f targetPosition = _positionInterp.end() + rotation.matrix() * delta_p;

        setTargetPosition(targetPosition);
    }
    _positionInterp.update(elapsedSeconds);
    _position = _positionInterp.value();

    // Combine everything into view transform.
    _worldTransform = ph::rt::NodeTransform::Identity();
    _worldTransform.translate(_position);
    _worldTransform.rotate(rotation);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void FirstPersonController::orbitingUpdate(float elapsedSeconds) {
    // Orbital camera can only move forward and backward. All other movements are actually rotation around the
    // orbital center.
    Vector3f delta_p(0, 0, 0);
    Vector3f delta_a(0, 0, 0);
    float    elapsedSecondsAdjusted = elapsedSeconds * _handednessMultiplicationFactor;

    // Update rotation angles
    //  - x is angle towards the X-Z plane. When right handed, positive angle is turning down.
    //  - y is angle towards the X-Y plane. When right handed, positive angle is turning right.
    // FIXME: converting delta_a from radian to degree seem fixing the issue of keyboard rotation was too slow.
    // But the math doesn't make sense. In all other places, the rotation unit is defined in radian, not degree.
    // Something fishy is going on.
    constexpr float r2d = 180.0f / PI;
    if (_keys[TURN_U]) delta_a.x() -= _rotateSpeed * elapsedSecondsAdjusted * r2d;
    if (_keys[TURN_D]) delta_a.x() += _rotateSpeed * elapsedSecondsAdjusted * r2d;
    if (_keys[TURN_L]) delta_a.y() += _rotateSpeed * elapsedSecondsAdjusted * r2d;
    if (_keys[TURN_R]) delta_a.y() -= _rotateSpeed * elapsedSecondsAdjusted * r2d;
    if (!delta_a.isZero()) { setTargetAngle(_angle + delta_a); }
    _rotationInterp.update(elapsedSeconds);
    _angle        = limitRotationAngle(_rotationInterp.value());
    auto rotation = euler(_angle);

    // In orbital mode, moving left/right/up/down does not affect the camera position.
    // It is actually moving the orbital center to the opposite direction.
    if (_keys[MOVE_L]) delta_p.x() -= _moveSpeed.x() * elapsedSeconds;
    if (_keys[MOVE_R]) delta_p.x() += _moveSpeed.x() * elapsedSeconds;
    if (_keys[MOVE_D]) delta_p.y() -= _moveSpeed.y() * elapsedSeconds;
    if (_keys[MOVE_U]) delta_p.y() += _moveSpeed.y() * elapsedSeconds;
    if (delta_p.x() != 0.f || delta_p.y() != 0.f) {
        _orbitalCenterInterp.reset(_orbitalCenter.value(), _orbitalCenterInterp.end() + rotation.matrix() * Eigen::Vector3f(delta_p.x(), delta_p.y(), 0.f),
                                   INTERP_TIME);
    }
    _orbitalCenterInterp.update(elapsedSeconds);
    _orbitalCenter = _orbitalCenterInterp.value();

    // In orbital mode, moving forward and backward affects orbital radius.
    // If right handed, moving forward is moving toward negative Z.
    if (_keys[MOVE_F]) delta_p.z() -= _moveSpeed.z() * elapsedSeconds;
    if (_keys[MOVE_B]) delta_p.z() += _moveSpeed.z() * elapsedSeconds;
    if (0.f != delta_p.z()) {
        // Orbital radius changing speed should not be porpotional to move speed, but to current radius.
        float z    = clamp(delta_p.z() / _moveSpeed.z(), -.25f, .25f);
        float oldR = _orbitalRadiusInterp.end();
        float newR = std::max(oldR * (1.f + z), _minimalRadius);
        setTargetOrbitalRadius(newR);
    }
    _orbitalRadiusInterp.update(elapsedSeconds);
    _orbitalRadius = std::max(_minimalRadius, _orbitalRadiusInterp.value());

    // In orbiting mode, the camera position is determined by 3 factors:
    //      1. orbital center.
    //      2. rotation angle.
    //      3. radius.
    float y   = _orbitalRadius * -std::sin(_angle.x()) * _handednessMultiplicationFactor;
    float p   = _orbitalRadius * std::cos(_angle.x());
    float x   = p * std::sin(_angle.y()) * _handednessMultiplicationFactor;
    float z   = p * std::cos(_angle.y()) * _handednessMultiplicationFactor;
    _position = _orbitalCenter.value() + Eigen::Vector3f(x, y, z);

    // Combine everything into view transform.
    _worldTransform = ph::rt::NodeTransform::Identity();
    _worldTransform.translate(_position);
    _worldTransform.rotate(rotation);
}
