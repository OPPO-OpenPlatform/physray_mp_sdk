/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/rps.h>

using namespace ph;
using namespace ph::va;

/// A RPS scene with a colored triangle
class RPSTriangle4 : public SimpleScene {
public:
    RPSTriangle4(SimpleApp & app, float dtheta = 0.0002f)
        : SimpleScene(app), _recorder(app.loop()), _factory(rps::Factory::createFactory(rps::Factory::CreateParameters {.main = &app.dev().graphicsQ()})),
          _scene(new Scene()), _dtheta(dtheta) {
        createRenderPass();
        createPrograms();
        createVertices();
        createArgumentSet();
    }

    ~RPSTriangle4() override {
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

        // Begin the main render pass
        auto rt0 = rps::Pass::RenderTarget {{_scene->texture}}.setClearColorF(.25f, .5f, .75f, 1.f);
        auto rt1 = rps::Pass::RenderTarget {{bb}}.setClearColorF(.75f, .25f, 0.f, 1.f);
        if (_scene->mainPass->cmdBegin(_recorder, {std::array {rt0, rt1}})) {
            // draw the triangle to the offscreen texture
            auto                                 vertices = std::vector<rps::Buffer::View> {{_scene->vertexBuffer}};
            rps::GraphicsProgram::DrawParameters dp0;
            dp0.vertices    = vertices;
            dp0.vertexCount = 3;
            rps::Program::PushConstantBinding pcb;
            pcb.name = "PushConstants";
            _scene->theta += _dtheta;
            pcb.value = &_scene->theta;
            dp0.constants.reset(&pcb, 1);
            _scene->program0->cmdDraw(_recorder, dp0);

            _scene->mainPass->cmdNextSubpass(_recorder);

            // draw the offscreen texture to the screen
            rps::Program::ArgumentSetBinding     args[] = {{rps::Program::DRAW_TIER, _scene->argSet}};
            rps::GraphicsProgram::DrawParameters dp1;
            dp1.arguments = args;
            dp1.setNonIndexed(3);
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
        rps::Ref<rps::GraphicsProgram>    program0; // program for first subpass
        rps::Ref<rps::GraphicsProgram>    program1; // program for second subpass
        rps::Ref<rps::ArgumentSet>        argSet;   // argument set for second subpass
        rps::Ref<rps::Buffer>             vertexBuffer;
        rps::Ref<rps::Image>              texture;
        float                             theta = 0.f; // rotation angle
    };

    rps::RenderLoopCommandRecorder _recorder;
    rps::Ref<rps::Factory>         _factory;
    std::unique_ptr<Scene>         _scene;
    float                          _dtheta;

    void createRenderPass() {
        // create a render pass instance.
        rps::Pass::CreateParameters pcp {};

        // our render pass has 2 color render targets (one for each subpass).
        pcp.attachments.push_back({sw().initParameters().colorFormat});
        pcp.attachments.push_back({sw().initParameters().colorFormat});

        // Render to attachment #0.
        pcp.subpasses.push_back({
            {},  // no input attachment
            {0}, // 1 color attachment: attachments[0]
            {},  // no depth attachment
        });
        // Read attachment #0 and render to attachment #1
        pcp.subpasses.push_back({
            {0}, // 1 input attachment: attachments[0]
            {1}, // 1 color attachment: attachments[1]
            {},  // no depth attachment
        });

        _scene->mainPass = _factory->createPass(pcp);
    }

    struct Vertex {
        float x, y, z;
        RGBA8 color;
    };

    /// Create a simple vertex coloring GPU program.
    void createPrograms() {
        // we have 1 vertex buffer with 2 elements: position and color.
        auto vertexInput = rps::GraphicsProgram::VertexInput {rps::GraphicsProgram::VertexBinding {
            .elements =
                {
                    {"v_position", {offsetof(Vertex, x), VK_FORMAT_R32G32B32_SFLOAT}},
                    {"v_color", {offsetof(Vertex, color), VK_FORMAT_R8G8B8A8_UNORM}},
                },
            .stride = sizeof(Vertex),
        }};

        _scene->program0 = _factory->createGraphicsProgram(
            {
                .pass    = _scene->mainPass->handle(),
                .subpass = 0,
                .vs      = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_VERTEX_BIT, R"glsl(

#version 460

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_color;
layout(location = 0) out vec3 o_color;
layout(push_constant) uniform PushConstants {
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
            "Program0");

        _scene->program1 = _factory->createGraphicsProgram(
            {
                .pass    = _scene->mainPass->handle(),
                .subpass = 1,
                .vs      = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_VERTEX_BIT, R"glsl(
#version 460
    void main() {
        // Vulkan clip space has (-1, -1) on the left-top corner of the screen.
        const vec2 corners[] = vec2[](vec2(-1, -1), vec2(-1, 3), vec2(3, -1));
        gl_Position          = vec4(corners[gl_VertexIndex % 3], 0., 1.);
    })glsl")},
                .fs      = {.shader = _factory->createGLSLShader(VK_SHADER_STAGE_FRAGMENT_BIT, R"glsl(
#version 460

// Input generated by a previous pass.
layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput colorInput;

layout(location = 0) out vec4 o_color;

void main() {
    o_color = subpassLoad(colorInput);
}
            )glsl")},
            },
            "Program1");
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

    void createArgumentSet() {
        auto colorCreateInfo = va::ImageObject::CreateInfo()
                                   .set2D(1280, 720)
                                   .setFormat(sw().initParameters().colorFormat)
                                   .setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        _scene->texture = _factory->createImage({.ci = colorCreateInfo}, "texture");
        _scene->argSet  = _factory->createArgumentSet({}, "sample program argument set");
        _scene->argSet->seti("colorInput", rps::ImageSampler {{_scene->texture}});
    }
};