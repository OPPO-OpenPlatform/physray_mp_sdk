/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/rps.h>

using namespace ph;
using namespace ph::va;

/// A RPS hello world app that clears screen to a solid color.
class RPSHello : public SimpleScene {
public:
    RPSHello(SimpleApp & app)
        : SimpleScene(app), _recorder(app.loop()), _factory(rps::Factory::createFactory(rps::Factory::CreateParameters {.main = &app.dev().graphicsQ()})) {
        createRenderPass();
    }

    ~RPSHello() override {
        // Must release all RPS resources, before deleting the factory
        _mainPass.clear();
        _backBuffers.clear();
    }

    void resizing() {
        // Release the back buffer, since the image it references is about to be destroyed and recreated.
        _backBuffers.clear();
    }

    void resized() {
        // The swapchain is recreated. So we have to recreate the render target image to reference the new back buffer.
        _backBuffers.resize(sw().backBufferCount());
        for (size_t i = 0; i < sw().backBufferCount(); ++i) {
            const auto & bb = sw().backBuffer(i);
            _backBuffers[i] = _factory->importImage(rps::Image::ImportParameters {
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
        auto & bb = _backBuffers[rp.backBufferIndex];
        bb->syncAccess({.layout = sw().backBuffer(rp.backBufferIndex).layout});

        // Run an empty render pass. This will effectively clear the back buffer to the specified color.
        auto rt0 = rps::Pass::RenderTarget {bb}.setClearColorF(.25f, .5f, .75f, 1.f);
        if (_mainPass->cmdBegin(_recorder, {{rt0}})) {
            // empty pass.
            _mainPass->cmdEnd(_recorder);
        }

        // Must return the latest layout of the back buffer to caller.
        return bb->syncAccess(nullptr).layout;
    }

private:
    rps::RenderLoopCommandRecorder    _recorder;
    rps::Ref<rps::Factory>            _factory;
    rps::Ref<rps::Pass>               _mainPass;
    std::vector<rps::Ref<rps::Image>> _backBuffers;

    void createRenderPass() {
        // create a render pass instance.
        rps::Pass::CreateParameters pcp {};

        // our render pass will have only 1 color render target that renders to back buffer of swapchain.
        pcp.attachments.push_back({sw().initParameters().colorFormat});

        // Only 1 subpass that renders to attachment #0.
        pcp.subpasses.push_back({
            {},  // no input attachment
            {0}, // 1 color attachment: attachments[0]
            {},  // no depth attachment
        });

        _mainPass = _factory->createPass(pcp);
    }
};