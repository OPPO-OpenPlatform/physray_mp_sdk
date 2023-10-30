/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

// This is the low level building blocks of PhysRay HCP SDK.

#pragma once

namespace ph {
namespace hcp {

enum class ShaderImpl {
    DSP_HALIDE,
    DEBUG_OVERLAY,
};

/**
 * Rotations and Transformations for elements
 *
 * Supported rotations are: 90, 180, 270 degrees clockwise and counter-clockwise
 * Rot 90 counter-clockwise = Rot 270 clockwise
 * Rot 270 counter-clockwise = Rot 90 clockwise
 *
 * CW = Clockwise
 * CCW = Counter-Clockwise
 *
 * Supported flips are: Horizontal flip and Vertical flip
 */
enum UIElementTrans { DEFAULT, ROT_90_CW, ROT_180, ROT_270_CW, FLIP_V, FLIP_H, MAX, ROT_90_CCW = 3, ROT_270_CCW = 1 };

/**
 * Rendering Functions
 *
 * ResizeNN, ResizeBilinear refers to Nearest Neighbor and Bilinear resize algorithms
 * DrawLine option allows HCP to render a line
 *
 * HCP also allows adding color or alpha blending to elements
 *
 * Color Tint refers to the following logic implementation:
 *
 * Pseudo implementation:
 * finalRGBA = (RGBA * colorTint_RGBA) / 255
 *
 * AlphaBlending implemented in HCP is an optimized version of Porter-Duff equation, which assumes an opaque background.
 *
 * Pseudo implementation:
 * A = 1
 * (R, G, B) = (Rs, Gs, Bs)As + (Rd, Gd, Bd)(1 - As)
 *
 * An OpenGL equivalent for it is: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
 */
enum RenderingFunction {
    ResizeNN,
    ResizeNN_alphaBlend,
    ResizeNN_tint,
    ResizeNN_tint_alphaBlend,
    ResizeBilinear,
    ResizeBilinear_alphaBlend,
    ResizeBilinear_tint,
    ResizeBilinear_tint_alphaBlend,
    DrawLine,
    DrawLine_alphaBlend,
};

} // namespace hcp
} // namespace ph
