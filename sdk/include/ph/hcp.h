/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

// This is the low level building blocks of PhysRay HCP SDK.

#pragma once

#include <ph/base.h>
#include <ph/hcp/types.h>

#include <cstdint>
#include <memory>
#include <string>

namespace ph {
namespace hcp {

/**
 * A rectangle/ quad is represented by top left coordinates along with width and height
 *
 * TL (x,y); w, h
 *
 * TL------TR
 * |       |
 * |       |
 * BL------BR
 *
 */
struct Rectangle {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
};

struct ColorTintVals {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

/**
 * A UI Element is defined by a following attributes:
 * src Rectangle, represents the quad to be sampled from the source texture
 * dst Rectangle, represents the quad to be to drawn on screen
 * trans, refers to element transformations that need to be applied
 * renderingFunction, refers to the type of resize and post process that needs to be applied to the element
 * color, refers to color values in case of using color tint on element or in case of drawing a line.
 */
struct UIElement {
    Rectangle                  src = {0, 0, 0, 0};
    Rectangle                  dst;
    UIElementTrans             trans             = UIElementTrans::DEFAULT;
    ph::hcp::RenderingFunction renderingFunction = ph::hcp::RenderingFunction::ResizeNN;
    ColorTintVals              color             = {0, 0, 0, 0};
};

// Forward declarations to avoid inclusion of EGL/egl.h
typedef void * EGLDisplay;
typedef void * EGLContext;
typedef void * EGLSurface;

class IPipeline {
public:
    virtual void initialize()                        = 0;
    virtual bool isEnabled()                         = 0;
    virtual void setEnabled(bool enabled)            = 0;
    virtual void setShaderImpl(ShaderImpl shaderImp) = 0;
    virtual void setDSPPower(int level)              = 0;
};

/**
 * HCP Rendering2D Pipeline APIs
 *
 * loadUITexture: Loads the texture in HCP pipeline; It takes OpenGL textureID and texture dimensions as input and returns an HCP textureID.
 *
 * addUIElement: Adds a UIElement to rendering queue. Returns HCP elementID.
 *
 * removeUIElement: Removes UIElement from rendering queue. Takes HCP elementID as input.
 *
 * render: Similar to glDraw* call in OpenGL. Renders all the elements in the rendering queue on to the framebuffer.
 * If clearElements = True, entire framebuffer is cleared before rendering.
 * If clearElements = False, only a portion of framebuffer, which is supposed to be written, is cleared before rendering.
 *
 * setForceRedraw: forces HCP to render/ re-render the elements present in rendering queue.
 */
class IRendering2D : virtual public IPipeline {
public:
    virtual int  loadUITexture(uint32_t gl_tex_id, int32_t w, int32_t h)      = 0;
    virtual int  addUIElement(const UIElement & element, int hcp_tex_id = -1) = 0;
    virtual void removeUIElement(uint32_t id)                                 = 0;
    virtual int  render(bool clearElements = false)                           = 0;
    virtual void setForceRedraw(bool enabled)                                 = 0;
};

/**
 * Create HCP Rendering 2D Pipeline
 */
class PipelineFactory {
public:
    static IRendering2D * CreateRendering2DPipeline(EGLDisplay display, EGLSurface surface, EGLContext context);
};

} // namespace hcp
} // namespace ph
