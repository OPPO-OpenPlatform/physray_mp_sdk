#pragma once
#include <ph/rt.h>
#include <chrono>
#include <optional>

template<typename T>
struct LinearMix {
    T operator()(const T & a, const T & b, float factor) { return a * (1.0f - factor) + b * factor; }
};

template<class T, class MIX = LinearMix<T>>
class Interpolator {
public:
    /// ctor
    Interpolator() {}

    /// ctor
    /// \param from     Start point of interpolation
    /// \param to       End point of interpolation
    /// \param duration Duration of interpolation
    Interpolator(const T & from, const T & to, float duration): _begin(from), _end(to), _value(from), _duration(duration), _currentTime(0.f) {}

    /// Reset the interpolator
    /// \param from     Start point of interpolation
    /// \param to       End point of interpolation
    /// \param duration  Duration of interpolation
    void reset(const T & from, const T & to, float duration) {
        PH_ASSERT(duration > 0.f);
        _value       = from;
        _begin       = from;
        _end         = to;
        _duration    = duration;
        _currentTime = 0.0f;
    }

    /// Move forward the interpolator by 'deltaTime'.
    /// Return true means the end of interpolation.
    bool update(float elapsed) {
        _currentTime += elapsed;
        float factor = ph::clamp(_currentTime / _duration, 0.f, 1.f);
        _value       = MIX()(_begin, _end, factor);
        return factor == 1.0f;
    }

    ///
    /// get current delta value
    ///
    const T & begin() const { return _begin; }

    ///
    /// get target value
    ///
    const T & end() const { return _end; }

    ///
    /// get current value
    ///
    const T & value() const { return _value; }

private:
    T     _begin, _end, _value;
    float _duration, _currentTime;
};

/// First person controller: walking on X-Z plane, Y is always vertical.
/// Allows us to manipulate a targeted node.
class FirstPersonController {
public:
    /// camera action type
    enum Key : int {
        INVALID_KEY = -1,
        /// Translates controller left.
        MOVE_L = 0,

        /// Translates controller right.
        MOVE_R,

        /// Translates controller up.
        MOVE_U,

        /// Translates controller down.
        MOVE_D,

        /// Translates controller forward.
        MOVE_F,

        /// Translates controller backward.
        MOVE_B,

        /// Rotates controller left.
        TURN_L,

        /// Rotates controller right.
        TURN_R,

        /// Rotates controller up.
        TURN_U,

        /// Rotates controller down.
        TURN_D,

        LOOK,

        PAN,

        NUM_KEYS,
    };

    FirstPersonController();

    /// @return How fast the first person controller will translate.
    const Eigen::Vector3f & moveSpeed() const { return _moveSpeed; }

    /// default is 1.0 (unit/s)
    FirstPersonController & setMoveSpeed(float sx, float sy, float sz) {
        _moveSpeed = {sx, sy, sz};
        return *this;
    }

    /// default is 1.0 (unit/s)
    FirstPersonController & setMoveSpeed(const Eigen::Vector3f & s) {
        _moveSpeed = s;
        return *this;
    }

    /// default is PI/4  (radian/s)
    FirstPersonController & setRotateSpeed(float s) {
        _rotateSpeed = s;
        return *this;
    }

    /// default is PI/500 (radian/pixel)
    FirstPersonController & setMouseLookSensitivity(float s) {
        _mouseLookSensitivity = s;
        return *this;
    }

    /// default is 1.0
    FirstPersonController & setMouseMoveSensitivity(float s) {
        _mouseMoveSensitivity = s;
        return *this;
    }

    /// default is 0.1
    FirstPersonController & setMouseWheelSensitivity(float s) {
        _mouseWheelSensitivity = s;
        return *this;
    }

    /// Set where you want the camera to move to. Note that this method does not affect camera's current position
    /// immediately. It only sets the target, then the update() method will actually move the camera gradually towards
    /// the target point.
    FirstPersonController & setTargetPosition(const Eigen::Vector3f &);

    /// Immediately update camera's position. This method also override any previously set target position.
    FirstPersonController & setPosition(const Eigen::Vector3f &);

    /// Set camera's target euler angle in radian. Note that this method does not affect camera's rotation angle
    /// immediately. It only sets the target, then the update() method will actually rotate the camera gradually toward
    /// the target angle.
    FirstPersonController & setTargetAngle(const Eigen::Vector3f &);

    /// Immediately update camera's rotation angle, in radian.
    ///
    /// In flythrough mode, this is the euler angle where X is pitch, Y is yaw and Z is roll.
    ///
    /// In orbital mode, X and Y together define the polar angle of the rotation. Z is ignored.
    FirstPersonController & setAngle(const Eigen::Vector3f &);

    /// Set desired orbital center. This also switches the camera to orbital mode, if not already.
    /// Note that this method does not change camera's orbital center immediately. It only set the desired value.
    /// Then the update() method will actually move camera's orbital center gradually toward the target point.
    FirstPersonController & setTargetOrbitalCenter(const Eigen::Vector3f &);

    /// Immediately update camera's orbital center. Value of the parameter also determins the camera's rotating mode:
    /// A non-null center sets the camera to orbital mode. A null center pointer sets the camera to fly-through mode.
    FirstPersonController & setOrbitalCenter(const Eigen::Vector3f * center);

    /// Set desired orbital radius. The actual radius will be updated by the built-in interpolator
    /// when update() is called.
    FirstPersonController & setTargetOrbitalRadius(float r);

    /// Immediately update camera's orbital radius.
    FirstPersonController & setOrbitalRadius(float r);

    /// Set handness of the controller.
    FirstPersonController & setHandness(ph::rt::Camera::Handness handness) {
        _handednessMultiplicationFactor = (ph::rt::Camera::RIGHT_HANDED == handness ? 1.0f : -1.0f);
        return *this;
    }

    FirstPersonController & setMinimalOrbitalRadius(float r) {
        _minimalRadius = r;
        return *this;
    }

    /// Set position boundary for flythrough camera. Set to an empty box to disable the boundary check.
    /// Note that this boundary has no effect to orbital camera.
    FirstPersonController & setFlythroughPositionBoundary(const Eigen::AlignedBox3f & b) {
        _flythroughBoundary = b;
        return *this;
    }

    /// Returns true if the camera is in orbital mode.
    bool orbiting() const { return _orbitalCenter.has_value(); }

    /// \return Get camera's orbital center. Undefined if camera is _NOT_ in oribital mode.
    Eigen::Vector3f orbitalCenter() const { return _orbitalCenter.value_or(Eigen::Vector3f(0.f, 0.f, 0.f)); }

    /// Get current orbital radius. Undefined, if the camera is not in orbital mode.
    float orbitalRadius() const { return _orbitalRadius; }

    /// \return Current camera position
    const Eigen::Vector3f & position() const { return _position; }

    /// Get camera rotation eular angle in radian. The meaning of the angle depends on if the camera is in flythrough
    /// mode or orbital mode.
    /// In fly through mode, this is the euler angle of the camera:
    ///     x is pitch, y is yaw, z is roll.
    /// In orbit mode, x and y are spherical coordinate. In our Y-up coordinate system,
    ///     x is angle towards the X-Z plane
    ///     y is angle towards the X-Y plane
    ///     z is not used.
    const Eigen::Vector3f & angle() const { return _angle; }

    /// Returns the world transformation of this controller (from local space to world/parent).
    /// If you need the world-to-view transform, use inverse of this.
    /// This value is refreshed by and only by the update() method.
    const Eigen::AffineCompact3f & getWorldTransform() const { return _worldTransform; }

    void onKeyPress(Key k, bool pressed);
    void onMouseMove(float x, float y, float z = 0.f);
    void onMouseWheel(float delta);

    /// Update camera's position and angle to move/rotate toward the target position/angle.
    /// \param elapsedSeconds Elapsed time, in seconds, since last update()
    void update(float elapsedSeconds);

private:
    struct Look {
        bool            start  = false;
        float           startX = 0, startY = 0;
        Eigen::Vector3f startR = {};
    };

    struct Pan {
        bool            start  = false;
        float           startX = 0, startY = 0, startZ = 0;
        Eigen::Vector3f startP = {};
        Eigen::Vector3f startO = {};
        float           startR = 0;
    };

    /// If set to false, this will stop updating.
    bool            _keys[NUM_KEYS]                 = {};
    Eigen::Vector3f _moveSpeed                      = {1.0f, 1.0f, 1.0f}; ///< unit/second
    float           _rotateSpeed                    = ph::PI / 4.0f;      ///< radian/second
    float           _mouseLookSensitivity           = ph::PI / 500.0f;    ///< radian/pixel
    float           _mouseMoveSensitivity           = 1.0f;
    float           _mouseWheelSensitivity          = 0.1f;
    float           _handednessMultiplicationFactor = 1.0f;
    Eigen::Vector3f _position                       = {0, 0, 0}; ///< camera position

    /// In fly through mode, this is the euler angle of the camera:
    ///     x is pitch, y is yaw, z is roll.
    /// In orbit mode, x and y are spherical coordinate. In our Y-up coordinate system,
    ///     x is angle towards the X-Z plane
    ///     y is angle towards the X-Y plane
    ///     z is not used.
    Eigen::Vector3f _angle = {0, 0, 0};

    /// local to world/parent space transformation
    Eigen::AffineCompact3f _worldTransform = Eigen::AffineCompact3f::Identity();

    // orbital specific fields
    std::optional<Eigen::Vector3f> _orbitalCenter {}; // no orbital point by default.
    float                          _orbitalRadius = 0.f;
    float                          _minimalRadius = 0.f;

    // Interpolators for smooth camera movement.
    Interpolator<Eigen::Vector3f> _positionInterp;
    Interpolator<Eigen::Vector3f> _rotationInterp;
    Interpolator<Eigen::Vector3f> _orbitalCenterInterp;
    Interpolator<float>           _orbitalRadiusInterp;

    // Boundary for flythough camera. No effect on orbital camera.
    // Set to empty to disable boundary check.
    Eigen::AlignedBox3f _flythroughBoundary;

    // For mouse looking and panning
    Look _look;
    Pan  _pan;

private:
    void resetOrbitalParametersBasedOnCurrentCameraPosition(bool immediateUpdate);
    void flythroughUpdate(float elapsedSeconds);
    void orbitingUpdate(float elapsedSeconds);
};
