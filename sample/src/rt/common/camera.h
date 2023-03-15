/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : camera.h
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

#include <ph/va.h>

/// --------------------------------------------------------------------------------------------------------------------
///
/// Represents a camera in the scene.
/// TODO: Only perspective cameras are currently supported.
/// Orthographic cameras will currently be ignored.
struct Camera {
    /// Define camera handness.
    enum Handness {
        LEFT_HANDED,
        RIGHT_HANDED,
    };

    /// vertical field of view in radian. Set to zero to make an orthographic camera.
    float yFieldOfView = 1.0f;

    /// Define handness of the camera. Default is right handed.
    Handness handness = RIGHT_HANDED;

    /// distance of near plane
    float zNear = 0.1f;

    /// distance of far plane
    float zFar = 10000.0f;

    ph::rt::Node * node = nullptr;

    /// Calculate projection matrix of the camera.
    /// \param displayW display width in pixels
    /// \param displayH display height in pixels
    Eigen::Matrix4f calculateProj(float displayW, float displayH) const {
        Eigen::Matrix4f proj = Eigen::Matrix4f::Identity();
        if (yFieldOfView != 0.0f) {
            // Calculate the projection matrix from this camera's properties.
            float displayAspectRatio = displayW / displayH;
            if (RIGHT_HANDED == handness) {
                proj = ph::va::perspectiveRH(yFieldOfView, displayAspectRatio, zNear, zFar);
            } else {
                proj = ph::va::perspectiveLH(yFieldOfView, displayAspectRatio, zNear, zFar);
            }
        } else {
            proj = ph::va::orthographic(displayW, displayH, zNear, zFar, LEFT_HANDED == handness);
        }
        return proj;
    }

    ph::rt::NodeTransform worldTransform() {
        if (node == nullptr) {
            return {};
        } else {
            return node->worldTransform();
        }
    }
};
