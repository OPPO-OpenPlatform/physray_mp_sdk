/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : texture.h
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
#include <ph/rps.h>

using namespace ph;
using namespace ph::va;

/// A RPS scene demonstrate texture usage in RPS.
class RPSTexture : public SimpleScene {
public:
    RPSTexture(SimpleApp & app)
        : SimpleScene(app), _recorder(app.loop()), _factory(rps::Factory::createFactory(rps::Factory::CreateParameters {.main = &app.dev().graphicsQ()})),
          _scene(new Scene()) {
        createRenderPass();
        createProgram();
        createVertices();
        createTexture();
        createSampler();
        createArgumentSet();
    }

    ~RPSTexture() override {
        // Must release all RPS resources, before deleting the factory
        _scene = nullptr;
    }

    void resizing() {
        // Release the back buffer, since the image it references is about to be destroyed and recreated.
        _scene->backBuffers.clear();
    }

    void resized() {
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

    VkImageLayout record(const SimpleRenderLoop::RecordParameters & rp) {
        // Each frame, the render loop class allocates new command buffers to record GPU commands. So we have
        // to update the command buffer of the command recorder each frame too.
        _recorder.setCommands(rp.cb);

        // Update state of the back buffer that we are currently render to.
        auto & bb = _scene->backBuffers[rp.backBufferIndex];
        bb->syncAccess({.layout = sw().backBuffer(rp.backBufferIndex).layout});

        // This list is passed to rps::Pass::begin() method to transfer all buffers into correct state before
        // the render pass begins. This is because Vulkan doesn't allow non-graphics pipeline barriers inside
        // a graphics render pass.
        _scene->vertexBuffer1->cmdSetAccess(_recorder, rps::Buffer::VB());
        _scene->vertexBuffer2->cmdSetAccess(_recorder, rps::Buffer::VB());
        _scene->indexBuffer->cmdSetAccess(_recorder, rps::Buffer::IB());
        _scene->texture->cmdSetAccess(_recorder, rps::Image::SR());

        // Begin the main render pass
        auto rt0 = rps::Pass::RenderTarget {bb}.setClearColorF(.25f, .5f, .75f, 1.f);
        if (_scene->mainPass->cmdBegin(_recorder, {.targets = {rt0}})) {
            // draw quad #1
            auto args     = std::array<rps::Program::ArgumentSetBinding, 1> {{0, _scene->arguments}};
            auto vertices = std::array<rps::Buffer::View, 1> {{_scene->vertexBuffer1}};
            auto dp       = rps::GraphicsProgram::DrawParameters {
                .arguments = args,
                .vertices  = vertices,
            };
            dp.setIndexed({_scene->indexBuffer}, 6);
            _scene->program1->cmdDraw(_recorder, dp);

            // draw quad #2
            vertices[0] = {_scene->vertexBuffer2};
            _scene->program2->cmdDraw(_recorder, dp);

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
        rps::Ref<rps::GraphicsProgram>    program1, program2;
        rps::Ref<rps::ArgumentSet>        arguments;
        rps::Ref<rps::Buffer>             vertexBuffer1, vertexBuffer2;
        rps::Ref<rps::Buffer>             indexBuffer;
        rps::Ref<rps::Image>              texture;
        rps::Ref<rps::Sampler>            sampler;
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
        float x, y, z;
        float u, v;
    };

    /// Create a simple vertex coloring GPU program.
    void createProgram() {
        // we have 1 vertex buffer with 2 elements: position and color.
        auto vertexInput = rps::GraphicsProgram::VertexInput {rps::GraphicsProgram::VertexBinding {
            .elements =
                {
                    {"v_position", {offsetof(Vertex, x), VK_FORMAT_R32G32B32_SFLOAT}},
                    {"v_texcoord", {offsetof(Vertex, u), VK_FORMAT_R32G32_SFLOAT}},
                },
            .stride = sizeof(Vertex),
        }};

        auto vs = _factory->createGLSLShader(VK_SHADER_STAGE_VERTEX_BIT, R"...(
#version 460

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;
layout(location = 0) out vec2 o_texcoord;

void main() {
    // Pass vertex attributes to rasterizer and fragment shader.
    gl_Position = vec4(v_position, 1.0);
    o_texcoord = v_texcoord;
}
       )...");

        auto fs1Code = R"...(
#version 460

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 o_color;
layout(binding = 0) uniform sampler u_sampler;
layout(binding = 1) uniform texture2D u_texture;

void main() {
    o_color = texture(sampler2D(u_texture, u_sampler), v_texcoord);
}
           )...";

        auto fs2Code = R"...(
#version 460

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 o_color;
layout(binding = 0) uniform sampler2D u_combinedSampler;

void main() {
    o_color = texture(u_combinedSampler, v_texcoord);
}
           )...";

        auto fs1 = _factory->createGLSLShader(VK_SHADER_STAGE_FRAGMENT_BIT, fs1Code);
        auto fs2 = _factory->createGLSLShader(VK_SHADER_STAGE_FRAGMENT_BIT, fs2Code);

        _scene->program1 = _factory->createGraphicsProgram({
            .pass   = _scene->mainPass->handle(),
            .vs     = {.shader = vs},
            .fs     = {.shader = fs1},
            .vertex = vertexInput,
        });

        _scene->program2 = _factory->createGraphicsProgram({
            .pass   = _scene->mainPass->handle(),
            .vs     = {.shader = vs},
            .fs     = {.shader = fs2},
            .vertex = vertexInput,
        });
    }

    /// Create a quad mesh with 2 triangles.
    void createVertices() {
        Vertex vertices1[] = {
            {-0.75f, -0.3f, 0.f, 0.f, 0.f},
            {-0.75f, +0.3f, 0.f, 0.f, 1.f},
            {-0.25f, -0.3f, 0.f, 1.f, 0.f},
            {-0.25f, +0.3f, 0.f, 1.f, 1.f},
        };

        Vertex vertices2[] = {
            {+0.25f, -0.3f, 0.f, 0.f, 0.f},
            {+0.25f, +0.3f, 0.f, 0.f, 1.f},
            {+0.75f, -0.3f, 0.f, 1.f, 0.f},
            {+0.75f, +0.3f, 0.f, 1.f, 1.f},
        };

        uint16_t indices[] = {0, 1, 2, 2, 1, 3};

        // create the vertex buffer instance.
        _scene->vertexBuffer1 = _factory->createBuffer(
            rps::Buffer::CreateParameters {
                .size   = std::size(vertices1) * sizeof(Vertex),
                .usages = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            },
            "vertices1");

        _scene->vertexBuffer2 = _factory->createBuffer(
            rps::Buffer::CreateParameters {
                .size   = std::size(vertices2) * sizeof(Vertex),
                .usages = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            },
            "vertices2");

        // create index buffer
        _scene->indexBuffer = _factory->createBuffer(
            rps::Buffer::CreateParameters {
                .size   = std::size(indices) * sizeof(uint16_t),
                .usages = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            },
            "indices");

        // Upload vertex and index data.
        rps::SynchronousCommandRecorder rec(dev().graphicsQ());
        rec.syncExec([&]() {
            _scene->vertexBuffer1->cmdWrite(rec, vertices1);
            _scene->vertexBuffer2->cmdWrite(rec, vertices2);
            _scene->indexBuffer->cmdWrite(rec, indices);
        });
    }

    void createTexture() {
        rps::Image::CreateParameters1 cp = {
            .ci = va::ImageObject::CreateInfo {}.set2D(64, 64),
        };
        _scene->texture = _factory->createImage(cp, "pack man");

        // fill the pixel array with checkerboard patterns
        std::vector<RGBA8> pixels(64 * 64);
        for (size_t i = 0; i < pixels.size(); ++i) {
            auto x = i % 64;
            auto y = i / 64;
            if (((x / 8) + (y / 8)) % 2) {
                pixels[i].set(255, 255, 255, 255);
            } else {
                pixels[i].set(0, 0, 0, 255);
            }
        }
        rps::Image::PixelArray array = {pixels.data(), 64 * sizeof(RGBA8)};

        // update texture content
        rps::SynchronousCommandRecorder rec(dev().graphicsQ());
        rec.syncExec([&]() { _scene->texture->cmdWriteSubresource(rec, array); });
    }

    void createSampler() {
        _scene->sampler = _factory->createSampler({}); // create sampler with default argument.
    }

    void createArgumentSet() {
        _scene->arguments = _factory->createArgumentSet({});
        _scene->arguments->seti("u_sampler", rps::ImageSampler {{}, _scene->sampler});
        _scene->arguments->seti("u_texture", rps::ImageSampler {{_scene->texture}});
        _scene->arguments->seti("u_combinedSampler", rps::ImageSampler {{_scene->texture}, _scene->sampler});
    }
};