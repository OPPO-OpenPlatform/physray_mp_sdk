/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

namespace ph {
namespace va {

/// A Vulkan pipeline creation helper class to simplify creating commonly used pipeline object.
struct SimplePipelineCreateInfo {
    StackArray<VkPipelineShaderStageCreateInfo, 8> shaders;

    SimplePipelineCreateInfo & addShader(VkShaderStageFlagBits stage, VkShaderModule module, const char * entry = "main") {
        auto ci = VkPipelineShaderStageCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, {}, {}, stage, module, entry};
        shaders.append(ci);
        return *this;
    }

    SimplePipelineCreateInfo & setShaders(ConstRange<VkPipelineShaderStageCreateInfo> shaders) {
        graphics.stageCount = (uint32_t) shaders.size();
        graphics.pStages    = shaders.data();
        return *this;
    }

    StackArray<VkVertexInputBindingDescription, 64>   vertexBindings;
    StackArray<VkVertexInputAttributeDescription, 64> vertexAttributes;
    VkPipelineVertexInputStateCreateInfo              vertexInput = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, {}, {}, 0, vertexBindings.data(), 0, vertexAttributes.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        {},
        {},
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkViewport                        viewports[16] = {};
    VkRect2D                          scissors[16]  = {};
    VkPipelineViewportStateCreateInfo viewportState {
        // 0 viewport and 0 scissor by default.
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, {}, {}, 0, viewports, 0, scissors,
    };

    SimplePipelineCreateInfo & setViewportAndScissor(const VkViewport & vp, uint32_t renderTargetWidth, uint32_t renderTargetHeight) {
        viewports[0]                = vp;
        scissors[0]                 = viewportToScissor(vp, renderTargetWidth, renderTargetHeight);
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;
        return *this;
    }

    SimplePipelineCreateInfo & setViewportAndScissor(uint32_t width, uint32_t height) {
        viewports[0].x              = 0.f;
        viewports[0].y              = 0.f;
        viewports[0].width          = (float) width;
        viewports[0].height         = (float) height;
        viewports[0].minDepth       = 0.f;
        viewports[0].maxDepth       = 1.f;
        scissors[0]                 = viewportToScissor(viewports[0], width, height);
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;
        return *this;
    }

    VkPipelineRasterizationStateCreateInfo rasterizeState {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        {},
        {},
        false,                           // depthClampEnable
        false,                           // rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,            // polygonMode
        VK_CULL_MODE_NONE,               // cullMode (no culling)
        VK_FRONT_FACE_COUNTER_CLOCKWISE, // frontFace (right handed)
        false,                           // depthBiasEnable
        0.0f,                            // depthBiasConstantFactor
        0.0f,                            // depthBiasClamp
        0.0f,                            // depthBiasSlopeFactor
        1.0f                             // lineWidth
    };

    VkPipelineMultisampleStateCreateInfo multisampleState {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        {},
        {},
        VK_SAMPLE_COUNT_1_BIT // rasterizationSamples
                              // other values can be default
    };

    // depth state: enabled by default.
    VkPipelineDepthStencilStateCreateInfo depthState {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        {},
        {},
        true,                                                                                              // depthTestEnable
        true,                                                                                              // depthWriteEnable
        VK_COMPARE_OP_LESS_OR_EQUAL,                                                                       // depthCompareOp
        false,                                                                                             // depthBoundTestEnable
        false,                                                                                             // stencilTestEnable
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFF, 0xFF, 0}, // stencil front
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xFF, 0xFF, 0}, // stencil back
    };

    SimplePipelineCreateInfo & enableDepth() {
        depthState.depthTestEnable  = true;
        depthState.depthWriteEnable = true;
        depthState.depthCompareOp   = VK_COMPARE_OP_LESS;
        return *this;
    }

    SimplePipelineCreateInfo & enableReadOnlyDepth() {
        depthState.depthTestEnable  = true;
        depthState.depthWriteEnable = false;
        depthState.depthCompareOp   = VK_COMPARE_OP_EQUAL;
        return *this;
    }

    SimplePipelineCreateInfo & disableDepth() {
        depthState.depthTestEnable  = false;
        depthState.depthWriteEnable = false;
        return *this;
    }

    SimplePipelineCreateInfo & resetStencilOp(bool enabled) {
        depthState.stencilTestEnable = enabled;
        depthState.front.failOp      = VK_STENCIL_OP_KEEP;
        depthState.front.passOp      = VK_STENCIL_OP_KEEP;
        depthState.front.depthFailOp = VK_STENCIL_OP_KEEP;
        depthState.front.compareOp   = VK_COMPARE_OP_ALWAYS;
        depthState.front.compareMask = 0xFF;
        depthState.front.writeMask   = 0xFF;
        depthState.front.reference   = 0;
        depthState.back.failOp       = VK_STENCIL_OP_KEEP;
        depthState.back.passOp       = VK_STENCIL_OP_KEEP;
        depthState.back.depthFailOp  = VK_STENCIL_OP_KEEP;
        depthState.back.compareOp    = VK_COMPARE_OP_ALWAYS;
        depthState.back.compareMask  = 0xFF;
        depthState.back.writeMask    = 0xFF;
        depthState.back.reference    = 0;
        return *this;
    }

    SimplePipelineCreateInfo & enableStencil() {
        depthState.stencilTestEnable = true;
        return *this;
    }

    SimplePipelineCreateInfo & disableStencil() {
        depthState.stencilTestEnable = false;
        return *this;
    }

    /// blend is disabled by default.
    VkPipelineColorBlendAttachmentState attachmentBlendStates[8] {blend(false), blend(false), blend(false), blend(false),
                                                                  blend(false), blend(false), blend(false), blend(false)};
    VkPipelineColorBlendStateCreateInfo blendState {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        {},
        {},
        false,             // logicOpEnable
        VK_LOGIC_OP_NO_OP, // logicOp
        1,
        attachmentBlendStates,   // attachments
        {1.0f, 1.0f, 1.0f, 1.0f} // blendConstants
    };

    SimplePipelineCreateInfo & enableAlphaBlend(uint32_t stage = 0) {
        attachmentBlendStates[stage] = blend(true);
        return *this;
    }

    SimplePipelineCreateInfo & enableAdditiveBlend(uint32_t stage = 0) {
        attachmentBlendStates[stage] = additiveBlend();
        return *this;
    }

    SimplePipelineCreateInfo & disableBlend(uint32_t stage = 0) {
        attachmentBlendStates[stage] = blend(false);
        return *this;
    }

    VkDynamicState                   dynamicStates[16] {};
    VkPipelineDynamicStateCreateInfo dynamicCI {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, {}, {}, 0, dynamicStates};

    SimplePipelineCreateInfo & addDynamicState(VkDynamicState s) {
        PH_REQUIRE(dynamicCI.dynamicStateCount < std::size(dynamicStates));
        dynamicStates[dynamicCI.dynamicStateCount++] = s;
        return *this;
    }

    // TODO: shader stages

    /// this is the graphics pipeline create info
    VkGraphicsPipelineCreateInfo graphics = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        {},
        {},
        0,
        nullptr,           // stages
        &vertexInput,      // vertex input
        &inputAssembly,    // input assembly
        nullptr,           // tessellation
        &viewportState,    // viewport
        &rasterizeState,   // rasterize
        &multisampleState, // multisample
        &depthState,       // depth-stencil
        &blendState,       // color blend
        &dynamicCI,        // dynamic states
    };

private:
    static VkPipelineColorBlendAttachmentState blend(bool enabled) {
        return {enabled,                             // blendEnable
                VK_BLEND_FACTOR_SRC_ALPHA,           // srcColorBlendFactor
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // dstColorBlendFactor
                VK_BLEND_OP_ADD,                     // colorBlendOp
                VK_BLEND_FACTOR_ZERO,                // srcAlphaBlendFactor
                VK_BLEND_FACTOR_ONE,                 // dstAlphaBlendFactor
                VK_BLEND_OP_ADD,                     // alphaBlendOp
                VK_COLOR_COMPONENT_R_BIT |           // colorWriteMask
                    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    }
    static VkPipelineColorBlendAttachmentState additiveBlend() {
        return {true,                      // blendEnable
                VK_BLEND_FACTOR_ONE,       // srcColorBlendFactor
                VK_BLEND_FACTOR_ONE,       // dstColorBlendFactor
                VK_BLEND_OP_ADD,           // colorBlendOp
                VK_BLEND_FACTOR_ONE,       // srcAlphaBlendFactor
                VK_BLEND_FACTOR_ONE,       // dstAlphaBlendFactor
                VK_BLEND_OP_ADD,           // alphaBlendOp
                VK_COLOR_COMPONENT_R_BIT | // colorWriteMask
                    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    }
};

} // namespace va
} // namespace ph