#include "pch.h"
#include <array>
#include "skybox.h"

using namespace ph;
using namespace ph::va;

// ---------------------------------------------------------------------------------------------------------------------
// Vertex shader
static const char vscode[] = R"glsl(
#version 460

layout (location = 0) in vec3 _inPos;

//push constants block
layout( push_constant ) uniform constants {
	mat4  projView; // proj * view
    vec3  ambient; // ambient color
    float lodBias;
    int   skyMapType;
    int   skyboxValid;
} _pc;

layout (location = 0) out vec3 _outUVW;

void main() {
    _outUVW = _inPos;
    vec4 pos = _pc.projView * vec4(_inPos, 1.0);
    // (RV-664): offset Z a bit to workaround skybox flickering issue on MTK phone.
    gl_Position = vec4(pos.xy , pos.w - 0.001, pos.w);
}
)glsl";

// ---------------------------------------------------------------------------------------------------------------------
// Fragment shader
static const char fscode[] = R"glsl(
#version 460

//push constants block
layout( push_constant ) uniform constants {
	mat4  projView; // proj * view
    vec3  ambient; // ambient color
    float lodBias;
    int   skyMapType;
    int   skyboxValid;
} _pc;

layout (location = 0) in vec3 _inUVW;

layout (binding =  1) uniform samplerCube samplerCubeMap;
layout (binding =  1) uniform sampler2D   sampler2DMap;

layout (location = 0) out vec3 _outFragColor;

const float PI     = 3.14159265358979323846;
const float TWO_PI = (PI * 2.0);

/// Convert direction vector to spherical angles: theta and phi.
///     x (phi)   : the horizontal angle in range of [0, 2*PI)
///     y (theta) : the vertical angle in range of [0, PI]
/// The math reference is here: https://en.wikipedia.org/wiki/Spherical_coordinate_system
vec2 directionToSphericalCoordinate(vec3 direction) {
    vec3 v = normalize(direction);

    float theta = acos(v.y); // this give theta in range of [0, PI];

    float r = sin(theta);

    float phi = acos(v.x / r); // this gives phi in range of [0, PI];

    if (v.z < 0) phi = TWO_PI - phi;

    return vec2(phi, theta);
}

vec2 cube2Equirectangular(vec3 direction) {
    vec2 thetaPhi = directionToSphericalCoordinate(direction);

    // convert phi to U
    float u = thetaPhi.x / TWO_PI;

    // convert theta to V
    float v = thetaPhi.y / PI;

    return vec2(u, v);
}

void main() {

    vec3 skymap = vec3(0);
    if (_pc.skyboxValid > 0) {
        if (1 == _pc.skyMapType) {
            skymap = textureLod(samplerCubeMap, _inUVW, _pc.lodBias).rgb;
        } else if (2 == _pc.skyMapType) {
            vec2 uv = cube2Equirectangular(_inUVW);
            skymap = textureLod(sampler2DMap, uv, _pc.lodBias).rgb;
        }
    }

    _outFragColor = skymap + _pc.ambient;
}
)glsl";

// ---------------------------------------------------------------------------------------------------------------------
//
Skybox::Skybox(const ConstructParameters & cp): _cp(cp) {
    PH_LOGI("[SKYBOX] Init Skybox");

    // Create geometry to be rendered.
    createBoxGeometry(10.0f, 10.0f, 10.0f);
    setupImageAndSampler();
    createPipelines();
}

// ---------------------------------------------------------------------------------------------------------------------
//
Skybox::~Skybox() {
    const auto & vgi = _cp.vsp.vgi();
    threadSafeDeviceWaitIdle(vgi.device);
    vgi.safeDestroy(_cubemapSampler);
    vgi.safeDestroy(_descriptorSetLayout);
    vgi.safeDestroy(_pipelineLayout);
    vgi.safeDestroy(_descriptorPool);
    vgi.safeDestroy(_skyboxPipeline);
    PH_LOGI("[SKYBOX] Skybox destroyed.");
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::resize(uint32_t w, uint32_t h) {
    _cp.width  = w;
    _cp.height = h;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::updatePushContants(VkCommandBuffer cmdBuffer, const Eigen::Matrix4f & proj, const Eigen::Matrix3f & camera, const Eigen::Vector3f & ambient,
                                float lodBias) {
    Eigen::Matrix4f view   = Eigen::Matrix4f::Identity();
    view.block<3, 3>(0, 0) = camera.inverse();

    PushConstants pc;
    pc.pvw         = proj * view;
    pc.ambient     = ambient;
    pc.lodBias     = lodBias;
    pc.skyMapType  = (int) _cp.skymapType;
    pc.skyboxValid = (int) !_cp.skymap.empty();
    vkCmdPushConstants(cmdBuffer, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::draw(VkCommandBuffer cmdBuffer, const Eigen::Matrix4f & proj, const Eigen::Matrix3f & camera, const Eigen::Vector3f & ambient, float lodBias) {
    VkViewport viewport = {0, 0, (float) _cp.width, (float) _cp.height, 0.0f, 1.0f};
    auto       scissor  = ph::va::util::rect2d(_cp.width, _cp.height, 0, 0);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSet, 0, 0);
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &_vertexBufferObj.g.buffer, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, _indexBufferObj.g.buffer, 0, VK_INDEX_TYPE_UINT16);
    updatePushContants(cmdBuffer, proj, camera, ambient, lodBias);
    vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(_indexBufferObj.size()), 1, 0, 0, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::createPipelines() {
    VkRenderPass pass = _cp.pass;

    // The pipeline layout and the render pass must have been created prior to setting up the pipeline.
    PH_REQUIRE(pass != VK_NULL_HANDLE);

    // Descriptor set layout.
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // ph::va::util::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        //                                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        //                                            0), // Binding 0
        ph::va::util::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                 1) // Binding 1
    };

    VkDescriptorSetLayoutCreateInfo setCreateInfo = ph::va::util::descriptorSetLayoutCreateInfo(setLayoutBindings);
    const auto &                    vgi           = _cp.vsp.vgi();
    PH_VA_REQUIRE(vkCreateDescriptorSetLayout(vgi.device, &setCreateInfo, vgi.allocator, &_descriptorSetLayout));

    // Setup descriptor pool.
    std::vector<VkDescriptorPoolSize> poolSizes = {// ph::va::util::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
                                                   ph::va::util::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};

    VkDescriptorPoolCreateInfo descriptorPoolInfo = ph::va::util::descriptorPoolCreateInfo(poolSizes, 1);

    PH_VA_REQUIRE(vkCreateDescriptorPool(vgi.device, &descriptorPoolInfo, vgi.allocator, &_descriptorPool));

    // Setup descriptor set.
    auto setAllocInfo = ph::va::util::descriptorSetAllocateInfo(_descriptorPool, &_descriptorSetLayout, 1);

    PH_VA_REQUIRE(vkAllocateDescriptorSets(vgi.device, &setAllocInfo, &_descriptorSet));

    if (_cp.skymapType != SkyMapType::EMPTY && !_cp.skymap) { createDummySkyboxTexture(); }

    auto descImageInfo = ph::va::util::descriptorImageInfo(_cubemapSampler, _cp.skymap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets {
        // ph::va::util::writeDescriptorSet(_descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &descBufferInfo),
        ph::va::util::writeDescriptorSet(_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &descImageInfo),
    };
    vkUpdateDescriptorSets(vgi.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // Create pipeline layout with push constant
    VkPipelineLayoutCreateInfo pipelineLayoutCi = ph::va::util::pipelineLayoutCreateInfo(&_descriptorSetLayout, 1);
    auto                       pcr       = VkPushConstantRange {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t) sizeof(PushConstants)};
    pipelineLayoutCi.pPushConstantRanges = &pcr;
    pipelineLayoutCi.pushConstantRangeCount = 1;
    PH_VA_REQUIRE(vkCreatePipelineLayout(vgi.device, &pipelineLayoutCi, vgi.allocator, &_pipelineLayout));

    // Load shaders and create pipeline.
    VkPipelineInputAssemblyStateCreateInfo iaState {};
    iaState.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaState.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaState.flags                  = 0;
    iaState.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rastState {};
    rastState.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastState.polygonMode = VK_POLYGON_MODE_FILL;
    rastState.cullMode    = VK_CULL_MODE_NONE;
    rastState.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rastState.lineWidth   = 1.0f;
    rastState.flags       = 0;

    VkPipelineColorBlendAttachmentState blendAttachmentState {};
    blendAttachmentState.colorWriteMask = 0xf;
    blendAttachmentState.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendState {};
    colorBlendState.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments    = &blendAttachmentState;

    VkPipelineDepthStencilStateCreateInfo depthStencilState {};
    depthStencilState.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable  = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.back.compareOp   = VK_COMPARE_OP_ALWAYS;

    VkDynamicState                   dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCi  = {.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                       .dynamicStateCount = (uint32_t) std::size(dynamicStates),
                                                       .pDynamicStates    = dynamicStates};

    VkViewport                        viewport = {0, 0, (float) _cp.width, (float) _cp.height, 0.0f, 1.0f};
    auto                              scissor  = ph::va::util::rect2d(_cp.width, _cp.height, 0, 0);
    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineMultisampleStateCreateInfo multisampleState {};
    multisampleState.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkVertexInputBindingDescription vertexBindingDesc {0, sizeof(Skybox::Vertex), VK_VERTEX_INPUT_RATE_VERTEX};

    _vertexAttribDesc.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Skybox::Vertex, pos)});
    _vertexAttribDesc.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Skybox::Vertex, normal)});

    VkPipelineVertexInputStateCreateInfo vertexInputState {};
    vertexInputState.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount   = 1;
    vertexInputState.pVertexBindingDescriptions      = &vertexBindingDesc;
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(_vertexAttribDesc.size());
    vertexInputState.pVertexAttributeDescriptions    = _vertexAttribDesc.data();

    VkGraphicsPipelineCreateInfo pipelineCi {};
    pipelineCi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCi.pInputAssemblyState = &iaState;
    pipelineCi.pRasterizationState = &rastState;
    pipelineCi.pColorBlendState    = &colorBlendState;
    pipelineCi.pMultisampleState   = &multisampleState;
    pipelineCi.pViewportState      = &viewportState;
    pipelineCi.pDepthStencilState  = &depthStencilState;
    pipelineCi.pVertexInputState   = &vertexInputState;
    pipelineCi.pDynamicState       = &dynamicStateCi;

    auto mainVS = ph::va::createGLSLShader(vgi, "skybox.vert", VK_SHADER_STAGE_VERTEX_BIT, {vscode, std::size(vscode)});
    auto mainFS = ph::va::createGLSLShader(vgi, "skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT, {fscode, std::size(fscode)});
    PH_ASSERT(mainVS && mainFS);

    auto ssci = [](auto stage, auto & shader) {
        return VkPipelineShaderStageCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, {}, {}, stage, shader.get(), "main"};
    };

    shaderStages = {
        ssci(VK_SHADER_STAGE_VERTEX_BIT, mainVS),
        ssci(VK_SHADER_STAGE_FRAGMENT_BIT, mainFS),
    };

    pipelineCi.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCi.pStages    = shaderStages.data();
    pipelineCi.renderPass = pass;
    pipelineCi.layout     = _pipelineLayout;

    PH_VA_REQUIRE(vkCreateGraphicsPipelines(vgi.device, VK_NULL_HANDLE, 1, &pipelineCi, vgi.allocator, &_skyboxPipeline));
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::setupImageAndSampler() {
    const auto &        vgi     = _cp.vsp.vgi();
    VkSamplerCreateInfo sampler = ph::va::util::samplerCreateInfo();
    sampler.magFilter           = VK_FILTER_LINEAR;
    sampler.minFilter           = VK_FILTER_LINEAR;
    sampler.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV        = sampler.addressModeU;
    sampler.addressModeW        = sampler.addressModeU;
    sampler.mipLodBias          = 0.0f;
    sampler.compareOp           = VK_COMPARE_OP_NEVER;
    sampler.minLod              = 0.0f;
    sampler.maxLod              = VK_LOD_CLAMP_NONE;
    sampler.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler.maxAnisotropy       = 1.0f;
    PH_VA_REQUIRE(vkCreateSampler(vgi.device, &sampler, vgi.allocator, &_cubemapSampler));
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::createBoxGeometry(float width, float height, float depth) {
    std::vector<Vertex> vertices;
    const uint32_t      NumTotalCoords = 3 * 8;
    vertices.resize(NumTotalCoords);

    float w2 = 0.5f * width;
    float h2 = 0.5f * height;
    float d2 = 0.5f * depth;

    // Fill in the front face vertex data.
    vertices[0] = Skybox::Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f);
    vertices[1] = Skybox::Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f);
    vertices[2] = Skybox::Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f);
    vertices[3] = Skybox::Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f);

    // Fill in the back face vertex data.
    vertices[4] = Skybox::Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f);
    vertices[5] = Skybox::Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f);
    vertices[6] = Skybox::Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f);
    vertices[7] = Skybox::Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f);

    // Fill in the top face vertex data.
    vertices[8]  = Skybox::Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f);
    vertices[9]  = Skybox::Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f);
    vertices[10] = Skybox::Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f);
    vertices[11] = Skybox::Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f);

    // Fill in the bottom face vertex data.
    vertices[12] = Skybox::Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f);
    vertices[13] = Skybox::Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f);
    vertices[14] = Skybox::Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f);
    vertices[15] = Skybox::Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f);

    // Fill in the left face vertex data.
    vertices[16] = Skybox::Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f);
    vertices[17] = Skybox::Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f);
    vertices[18] = Skybox::Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f);
    vertices[19] = Skybox::Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f);

    // Fill in the right face vertex data.
    vertices[20] = Skybox::Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f);
    vertices[21] = Skybox::Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f);
    vertices[22] = Skybox::Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f);
    vertices[23] = Skybox::Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f);

    std::vector<uint16_t> indices;
    indices.resize(36);

    // Fill in the front face index data
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;

    // Fill in the back face index data
    indices[6]  = 4;
    indices[7]  = 5;
    indices[8]  = 6;
    indices[9]  = 4;
    indices[10] = 6;
    indices[11] = 7;

    // Fill in the top face index data
    indices[12] = 8;
    indices[13] = 9;
    indices[14] = 10;
    indices[15] = 8;
    indices[16] = 10;
    indices[17] = 11;

    // Fill in the bottom face index data
    indices[18] = 12;
    indices[19] = 13;
    indices[20] = 14;
    indices[21] = 12;
    indices[22] = 14;
    indices[23] = 15;

    // Fill in the left face index data
    indices[24] = 16;
    indices[25] = 17;
    indices[26] = 18;
    indices[27] = 16;
    indices[28] = 18;
    indices[29] = 19;

    // Fill in the right face index data
    indices[30] = 20;
    indices[31] = 21;
    indices[32] = 22;
    indices[33] = 20;
    indices[34] = 22;
    indices[35] = 23;

    _vertexBufferObj.allocate(_cp.vsp.vgi(), vertices, "Skybox Vertex Buffer");
    _indexBufferObj.allocate(_cp.vsp.vgi(), indices, "Skybox Index Buffer");

    // Upload the data through a command buffer owned by the skybox
    SingleUseCommandPool cp(_cp.vsp);
    auto                 cb = cp.create();
    _vertexBufferObj.sync2gpu(cb);
    _indexBufferObj.sync2gpu(cb);
    cp.finish(cb);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::createDummySkyboxTexture() {
    // create a dumy cube texture
    _dummy.create("dummy skybox", _cp.vsp.vgi(),
                  ImageObject::CreateInfo {}.setCube(1).setFormat(VK_FORMAT_R8G8B8A8_UNORM).setUsage(VK_IMAGE_USAGE_SAMPLED_BIT));
    _cp.skymap     = _dummy;
    _cp.skymapType = SkyMapType::EMPTY;

    // setup layout of the dummy texture
    SingleUseCommandPool pool(_cp.vsp);
    pool.syncexec([this](auto cb) {
        setImageLayout(cb, _dummy.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
    });
}