/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/rps.h>

using namespace ph;
using namespace ph::va;

/// A RPS scene with a colored triangle
class RPSCompute : public SimpleScene {
public:
    RPSCompute(SimpleApp & app)
        : SimpleScene(app), _recorder(app.loop()), _factory(rps::Factory::createFactory(rps::Factory::CreateParameters {.main = &app.dev().graphicsQ()})),
          _scene(new Scene()) {
        createRenderPass();
        createPrograms();
        createVertices();
        createArgumentSet();
    }

    ~RPSCompute() override {
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

        // generate mesh vertices
        rps::Program::ArgumentSetBinding        args[] = {{rps::Program::DRAW_TIER, _scene->argSet}};
        rps::ComputeProgram::DispatchParameters dp0;
        dp0.arguments = args;
        dp0.width     = _scene->vertexCount;

        _scene->program0->cmdDispatch(_recorder, dp0);

        // Begin the main render pass
        auto rt0 = rps::Pass::RenderTarget {{bb}}.setClearColorF(.25f, .5f, .75f, 1.f);
        if (_scene->mainPass->cmdBegin(_recorder, {std::array {rt0}})) {
            // draw the generated mesh
            auto                                 vertices = std::vector<rps::Buffer::View> {{_scene->vertexBuffer}};
            rps::GraphicsProgram::DrawParameters dp1;
            dp1.vertices    = vertices;
            dp1.vertexCount = _scene->vertexCount;
            _scene->program1->cmdDraw(_recorder, dp1);

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
        rps::Ref<rps::ComputeProgram>     program0; // compute program for generating mesh vertices
        rps::Ref<rps::GraphicsProgram>    program1; // graphics program for rendering mesh
        rps::Ref<rps::ArgumentSet>        argSet;   // argument set for second subpass
        rps::Ref<rps::Buffer>             vertexBuffer;
        uint32_t                          vertexCount = 3;
        rps::Ref<rps::Image>              texture;
        float                             theta = 0.f; // rotation angle
    };

    rps::RenderLoopCommandRecorder _recorder;
    rps::Ref<rps::Factory>         _factory;
    std::unique_ptr<Scene>         _scene;

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
        float x, y, z, w;
        float r, g, b, a;
    };

    /// Create a simple vertex coloring GPU program.
    void createPrograms() {
        // we have 1 vertex buffer with 2 elements: position and color.
        auto vertexInput = rps::GraphicsProgram::VertexInput {rps::GraphicsProgram::VertexBinding {
            .elements =
                {
                    {"v_position", {offsetof(Vertex, x), VK_FORMAT_R32G32B32_SFLOAT}},
                    {"v_color", {offsetof(Vertex, r), VK_FORMAT_R32G32B32_SFLOAT}},
                },
            .stride = sizeof(Vertex),
        }};

        _scene->program0 = _factory->createComputeProgram(
            {
                .cs = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_COMPUTE_BIT, R"glsl(
#version 460
layout(local_size_x = 3, local_size_y = 1, local_size_z = 1) in;

struct Vertex {
    vec4 position;
    vec4 color;
};

layout(std430, binding = 0) buffer vertices { Vertex v[]; };

void main() {
    uint i = gl_GlobalInvocationID.x;
    const vec2 corners[] = vec2[](vec2(-0.5, 0.5), vec2(0.5, 0.5), vec2(0, -0.5));
    const vec3 colors[] = vec3[](vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1));

    if (i < 3) {
        v[i].position = vec4(corners[i], 0, 1);
        v[i].color = vec4(colors[i], 0);
    }
}
)glsl")},
            },
            "Program0");

        _scene->program1 = _factory->createGraphicsProgram(
            {
                .pass    = _scene->mainPass->handle(),
                .subpass = 0,
                .vs      = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_VERTEX_BIT, R"glsl(
#version 460

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_color;
layout(location = 0) out vec3 o_color;

void main() {
    // Pass vertex attributes to rasterizer and fragment shader.
    gl_Position = vec4(v_position, 1.0);
    o_color = v_color;
}
)glsl")},
                .fs      = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_FRAGMENT_BIT, R"glsl(
#version 460

layout(location = 0) in vec3 v_color;
layout(location = 0) out vec3 o_color;

void main() {
    // Use interpolated vertex color as the output color.
    o_color = v_color;
}
)glsl")},
                .vertex  = vertexInput,
            },
            "Program1");
    }

    /// Create a vertex buffer to hold vertices.
    void createVertices() {
        rps::Buffer::CreateParameters cp {
            .size   = _scene->vertexCount * sizeof(Vertex),
            .usages = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        };

        // create the vertex buffer instance.
        _scene->vertexBuffer = _factory->createBuffer(cp, "vertices");
    }

    void createArgumentSet() {
        _scene->argSet = _factory->createArgumentSet({}, "sample program argument set");
        _scene->argSet->setb("vertices", _scene->vertexBuffer);
    }
};