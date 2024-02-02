/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/rps.h>

using namespace ph;
using namespace ph::va;

/// A RPS scene with a colored triangle
class RPSTriangle2 : public SimpleScene {
public:
    RPSTriangle2(SimpleApp & app, float dtheta = 0.0002f)
        : SimpleScene(app), _recorder(app.loop()), _factory(rps::Factory::createFactory(rps::Factory::CreateParameters {.main = &app.dev().graphicsQ()})),
          _scene(new Scene()), _perDrawArgs(_factory, "perDrawArgs"), _dtheta(dtheta) {
        createRenderPass();
        createProgram();
        createVertices();
        _argSet = _factory->createArgumentSet({}, "sample program argument set");
        _argSet->setb("UniformBlock", _perDrawArgs.buffer);
    }

    ~RPSTriangle2() override {
        // Must release all RPS resources, before deleting the factory
        _scene = nullptr;
    }

    void resizing() override {
        // Release the back buffer, since the image it references is about to be destroyed and recreated.
        _scene->backBuffers.clear();
    }

    void resized() override {
        // The swapchain is recreated. So we have to recreate the render target image to reference the new back buffer.
        _scene->backBuffers.resize(sw().backBufferCount());
        for (size_t i = 0; i < sw().backBufferCount(); ++i) {
            const auto & bb        = sw().backBuffer(i);
            _scene->backBuffers[i] = _factory->importImage(rps::Image::ImportParameters {
                .image  = bb.image,
                .type   = VK_IMAGE_TYPE_2D,
                .format = bb.format,
                .extent = VkExtent3D {bb.extent.width, bb.extent.height, 1},
            });
        }
    }

    VkImageLayout record(const SimpleRenderLoop::RecordParameters & rp) override {
        // Each frame, the render loop class allocates new command buffers to record GPU commands. So we have
        // to update the command buffer of the command recorder each frame too.
        _recorder.setCommands(rp.cb);

        // Update state of the back buffer that we are currently render to.
        auto & bb = _scene->backBuffers[rp.backBufferIndex];
        bb->syncAccess({.layout = sw().backBuffer(rp.backBufferIndex).layout});

        // We need the vertex buffer in vertex input state. This list is passed to rps::Pass::begin() method
        // to transfer the buffer into correct state before the render pass begins. This is because Vulkan
        // doesn't allow non-graphics pipeline barriers inside a graphics render pass.
        _scene->vertexBuffer->cmdSetAccess(_recorder, rps::Buffer::VB());

        _perDrawArgs.theta += _dtheta;
        _perDrawArgs.update(_recorder);

        // Begin the main render pass
        auto rt0 = rps::Pass::RenderTarget {{bb}}.setClearColorF(.25f, .5f, .75f, 1.f);
        if (_scene->mainPass->cmdBegin(_recorder, {.targets = {&rt0, 1}})) {
            // draw the triangle
            auto                                 vertices = std::vector<rps::Buffer::View> {{_scene->vertexBuffer}};
            rps::Program::ArgumentSetBinding     args[]   = {{rps::Program::DRAW_TIER, _argSet}};
            rps::GraphicsProgram::DrawParameters dp;
            dp.arguments   = args;
            dp.vertices    = vertices;
            dp.vertexCount = 3;
            _scene->program->cmdDraw(_recorder, dp);

            // End render pass
            _scene->mainPass->cmdEnd(_recorder);
        }

        // Must return the latest layout of the back buffer to caller.
        return bb->syncAccess(nullptr).layout;
    }

private:
    struct Scene {
        rps::Ref<rps::Pass>               mainPass;
        std::vector<rps::Ref<rps::Image>> backBuffers;
        rps::Ref<rps::GraphicsProgram>    program;
        rps::Ref<rps::Buffer>             vertexBuffer;
        rps::Ref<rps::Image>              texture;
    };

    rps::RenderLoopCommandRecorder _recorder;
    rps::Ref<rps::Factory>         _factory;
    std::unique_ptr<Scene>         _scene;

    template<class T>
    class UniformBlock : public T {
    public:
        UniformBlock(rps::Ref<rps::Factory> & factory, const char * name = nullptr) {
            rps::Buffer::CreateParameters cp {
                .size   = sizeof(T),
                .usages = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            };

            // create the uniform buffer instance.
            buffer = factory->createBuffer(cp, name);
        }
        void update(rps::CommandRecorder & rec) {
            buffer->cmdWrite(rec, (const T *) this);
            buffer->cmdSetAccess(rec, rps::Buffer::UB());
        }

        rps::Ref<rps::Buffer> buffer;
    };

    // Tier 0 (per-draw) program arguments
    struct PerDrawArgs {
        float theta = 0.0f;
    };

    UniformBlock<PerDrawArgs>  _perDrawArgs;
    rps::Ref<rps::ArgumentSet> _argSet;
    float                      _dtheta;

    void createRenderPass() {
        // create a render pass instance.
        rps::Pass::CreateParameters pcp {};

        // our render pass has 1 color render target rendering to back buffer.
        pcp.attachments.push_back({sw().initParameters().colorFormat});

        // Only 1 subpass that renders to attachment #0.
        pcp.subpasses.push_back({
            {},  // no input attachment
            {0}, // 1 color attachment: attachments[0]
            {},  // no depth attachment
        });

        _scene->mainPass = _factory->createPass(pcp);
    }

    struct Vertex {
        float x, y, z;
        RGBA8 color;
    };

    /// Create a simple vertex coloring GPU program.
    void createProgram() {
        // we have 1 vertex buffer with 2 elements: position and color.
        auto vertexInput = rps::GraphicsProgram::VertexInput {rps::GraphicsProgram::VertexBinding {
            .elements =
                {
                    {"v_position", {offsetof(Vertex, x), VK_FORMAT_R32G32B32_SFLOAT}},
                    {"v_color", {offsetof(Vertex, color), VK_FORMAT_R8G8B8A8_UNORM}},
                },
            .stride = sizeof(Vertex),
        }};

        _scene->program = _factory->createGraphicsProgram({
            .pass   = _scene->mainPass->handle(),
            .vs     = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_VERTEX_BIT, R"...(

#version 460

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_color;
layout(location = 0) out vec3 o_color;
layout(std140, set = 0, binding = 0) uniform UniformBlock {
    float u_theta;
};

void main() {
    // Pass vertex attributes to rasterizer and fragment shader.
    float cost = cos(u_theta);
    float sint = sin(u_theta);
    mat2 m = mat2(cost, -sint, sint, cost);
    gl_Position = vec4(m * v_position.xy, v_position.z, 1.0);
    o_color = v_color;
}

        )...")},
            .fs     = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_FRAGMENT_BIT, R"...(
#version 460

layout(location = 0) in vec3 v_color;
layout(location = 0) out vec3 o_color;

void main() {
    // Use interpolated vertex color as the output color.
    o_color = v_color;
}
            )...")},
            .vertex = vertexInput,
        });
    }

    /// Create a vertex buffer containing 3 vertices.
    void createVertices() {
        Vertex vertices[] = {
            {-0.5f, +0.5f, 0.f, RGBA8::make(255, 0, 0)},
            {+0.5f, +0.5f, 0.f, RGBA8::make(0, 255, 0)},
            {+0.0f, -0.5f, 0.f, RGBA8::make(0, 0, 255)},
        };

        rps::Buffer::CreateParameters cp {
            .size   = std::size(vertices) * sizeof(Vertex),
            .usages = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        };

        // create the vertex buffer instance.
        _scene->vertexBuffer = _factory->createBuffer(cp, "vertices");

        // Upload vertex data to vertex buffer. We don't care perf so much here. So just use an
        // synchronous command recorder for simplicity.
        rps::SynchronousCommandRecorder rec(dev().graphicsQ());
        rec.syncExec([&]() { _scene->vertexBuffer->cmdWrite(rec, vertices); });
    }
};