/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : skybox.cpp
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

#include "pch.h"
#include <array>
#include "skybox.h"

using namespace ph;
using namespace ph::va;
using namespace ph::rps;

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
    bool  skyboxValid;
    float saturation;
    float gamma;
    bool  outputSRGB;
    float skyboxRotation;
} _pc;

layout (location = 0) out vec3 _outUVW;

void main() {
    _outUVW = _inPos;
    vec4 pos = _pc.projView * vec4(_inPos, 1.0);
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
    bool  skyboxValid;
    float saturation;
    float gamma;
    bool  outputSRGB;
    float skyboxRotation;
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

vec3 linear2SRGB(vec3 lin) {
    return vec3(
        lin.x < 0.0031308 ? (12.92 * lin.x) : 1.055 * pow(lin.x, 1. / 2.4) - 0.055,
        lin.y < 0.0031308 ? (12.92 * lin.y) : 1.055 * pow(lin.y, 1. / 2.4) - 0.055,
        lin.z < 0.0031308 ? (12.92 * lin.z) : 1.055 * pow(lin.z, 1. / 2.4) - 0.055);
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 acesFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0f, 1.0f);
}

vec3 rgb2hsv(vec3 rgb) {
    float r   = rgb.r;
    float g   = rgb.g;
    float b   = rgb.b;  
    float min = min(min(r, g), b);
    float max = max(max(r, g), b);
    if (0 == max) {
        // r = g = b = 0
        // in this case, h = -1, s = 0, v is undefined
        return vec3(-1, 0, 0); // v is actually undefined.
    }

    float delta = max - min;
    float v     = max;
    float s     = delta / max;
    float h;
    if (0 == delta) {
        h = 0;
    } else {
        if (r == max) {
            h = (g - b) / delta;      // between yellow & magenta
        } else if (g == max) {
            h = 2 + (b - r) / delta;  // between cyan & yellow
        } else {
            h = 4 + (r - g) / delta;  // between magenta & cyan
        }
        h *= 60;                      // degrees
        if (h < 0) {
            h += 360;
        }
    }
    return vec3(h, s, v);
}

vec3 hsv2rgb(vec3 hsv) {
    float h = hsv.r;
    float s = hsv.g;
    float v = hsv.b;
    if (s == 0) {
        // achromatic (grey)
        return vec3(v);
    }
    h /= 60;            // sector 0 to 5
    int   i = int(floor(h));
    float f = h - i;          // factorial part of h
    float p = v * ( 1 - s );
    float q = v * ( 1 - s * f );
    float t = v * ( 1 - s * ( 1 - f ) );
    switch( i ) {
        case  0: return vec3(v, t, p);
        case  1: return vec3(q, v, p);
        case  2: return vec3(p, v, t);
        case  3: return vec3(p, q, v);
        case  4: return vec3(t, p, v);
        default: return vec3(v, p, q);
    }
}

vec3 postprocess(vec3 color) {
    // ACES tonemapping HDR -> LDR
    color = acesFilm(color);

    // increase saturation of output color
    if (abs(_pc.saturation - 1.0) >= 0.01) {
        vec3 hsv = rgb2hsv(color.xyz);
        hsv.y *= _pc.saturation;
        color.xyz = hsv2rgb(hsv);
    }

    // custom Gamma correction
    if (abs(_pc.gamma - 1.0) >= 0.01) {
        color.xyz = pow(color.xyz, vec3(1.0 / _pc.gamma));
    }

    // color space conversion
    if (_pc.outputSRGB) {
        color = linear2SRGB(clamp(color, vec3(0), vec3(1)));
    }

    // done
    return color;
}

vec4 quat_mul(vec4 q1, vec4 q2) {
    vec4 q;
    q.x = (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y);
    q.y = (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x);
    q.z = (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w);
    q.w = (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z);
    return q;
}

void main() {

    vec3 skymap = vec3(0);
    if (_pc.skyboxValid) {
        vec3 direction = _inUVW;
        if (_pc.skyboxRotation > 0.0) {
            float sinHalfAngle = sin(_pc.skyboxRotation / 2.0);
            float cosHalfAngle = cos(_pc.skyboxRotation / 2.0);
            vec4  q            = normalize(vec4(0.0, sinHalfAngle, 0, cosHalfAngle));
            // quaternion multiplication is
            vec4 rotated = quat_mul(q, vec4(direction, 0.0));
            rotated      = quat_mul(rotated, vec4(-q.x, -q.y, -q.z, q.w));
            direction    = rotated.xyz;
        }

        if (1 == _pc.skyMapType) {
            skymap = textureLod(samplerCubeMap, direction, _pc.lodBias).rgb;
        } else if (2 == _pc.skyMapType) {
            vec2 uv = cube2Equirectangular(direction);
            skymap = textureLod(sampler2DMap, uv, _pc.lodBias).rgb;
        }
    }

    _outFragColor = postprocess(skymap + _pc.ambient);
}
)glsl";

// ---------------------------------------------------------------------------------------------------------------------
//
Skybox::Skybox(const ConstructParameters & cp): _cp(cp) {
    PH_LOGI("[SKYBOX] Init Skybox");
    _fac = Factory::createFactory({.main = &cp.loop.cp().dev.graphicsQ()});
    createBoxGeometry(10.0f, 10.0f, 10.0f);
    setupImageAndSampler();
    createPipelines();
}

// ---------------------------------------------------------------------------------------------------------------------
//
Skybox::~Skybox() {
    const auto & vgi = _cp.loop.vgi();
    threadSafeDeviceWaitIdle(vgi.device);
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
void Skybox::draw(VkCommandBuffer cmdBuffer, const Eigen::Matrix4f & proj, const Eigen::Matrix3f & camera, float saturation, float gamma, bool outputSRGB,
                  float skyboxRotation, float lodBias, const Eigen::Vector3f & ambient) {
    RenderLoopCommandRecorder rec(_cp.loop);
    rec.setCommands(cmdBuffer);

    // setup viewport and scissor
    VkViewport viewport = {0, 0, (float) _cp.width, (float) _cp.height, 0.0f, 1.0f};
    auto       scissor  = ph::va::util::rect2d(_cp.width, _cp.height, 0, 0);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    auto dp = GraphicsProgram::DrawParameters {};
    auto vb = rps::Buffer::View {_vb};
    dp.vertices.reset(&vb, 1);
    dp.setIndexed({_ib}, 36);

    // set arguments
    rps::Program::ArgumentSetBinding asb;
    asb.tier = 0;
    asb.args = _args;
    dp.arguments.reset(&asb, 1);

    // setup push constants
    Eigen::Matrix4f view   = Eigen::Matrix4f::Identity();
    view.block<3, 3>(0, 0) = camera.inverse();

    PushConstants pc;
    pc.pvw            = proj * view;
    pc.ambient        = ambient;
    pc.lodBias        = lodBias;
    pc.saturation     = saturation;
    pc.gamma          = gamma;
    pc.outputSRGB     = outputSRGB;
    pc.skyboxRotation = skyboxRotation;
    pc.skyMapType     = (int) _cp.skymapType;
    pc.skyboxValid    = (int) !_cp.skymap.empty();

    rps::Program::PushConstantBinding pcb;
    pcb.name  = "constants";
    pcb.value = &pc;
    dp.constants.reset(&pcb, 1);

    _program->cmdDraw(rec, dp);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::createPipelines() {
    rps::GraphicsProgram::CreateParameters cp = {};

    cp.pass   = _cp.pass;
    cp.vs     = {_fac->createGLSLShader(VK_SHADER_STAGE_VERTEX_BIT, vscode)};
    cp.fs     = {_fac->createGLSLShader(VK_SHADER_STAGE_FRAGMENT_BIT, fscode)};
    cp.vertex = {rps::GraphicsProgram::VertexBinding {
        .elements = {{"_inPos", {.offset = offsetof(Vertex, pos), .format = VK_FORMAT_R32G32B32_SFLOAT}}},
        .stride   = sizeof(Vertex),
    }};
    cp.depth  = 1;

    _program = _fac->createGraphicsProgram(cp, "skybox");
}

// ---------------------------------------------------------------------------------------------------------------------
//
void Skybox::setupImageAndSampler() {
    // create image
    PH_REQUIRE(_cp.skymap.format != VK_FORMAT_UNDEFINED);
    auto imgap          = rps::Image::ImportParameters {};
    imgap.image         = _cp.skymap.image;
    imgap.type          = VK_IMAGE_TYPE_2D;
    imgap.format        = _cp.skymap.format;
    imgap.extent        = _cp.skymap.extent;
    imgap.mipLevels     = 1;
    imgap.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgap.initialAccess = rps::Image::SR();
    if (_cp.skymapType == SkyMapType::CUBE) {
        imgap.arrayLayers = 6;
    } else {
        imgap.arrayLayers = 1;
    }
    auto skymap = _fac->importImage(imgap);

    // create sampler
    auto cp = Sampler::CreateParameters {};
    if (_cp.skymapType == SkyMapType::CUBE) { cp.setClampToEdge(); }
    cp.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    auto sampler   = _fac->createSampler(cp, "skybox cubemap sampler");

    // create argment set
    _args = _fac->createArgumentSet({}, "skybox");
    _args->seti("samplerCubeMap", {{skymap}, sampler});
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

    _vb = _fac->createBuffer({vertices.size() * sizeof(vertices[0]), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT}, "skybox vb");
    _ib = _fac->createBuffer({indices.size() * sizeof(indices[0]), VK_BUFFER_USAGE_INDEX_BUFFER_BIT}, "skybox ib");

    // Upload the data through a command buffer owned by the skybox
    SynchronousCommandRecorder rec(_cp.loop.cp().dev.graphicsQ());
    rec.syncExec([&]() {
        _vb->cmdWrite<Vertex>(rec, vertices);
        _ib->cmdWrite<uint16_t>(rec, indices);
        _vb->cmdSetAccess(rec, Buffer::VB());
        _ib->cmdSetAccess(rec, Buffer::IB());
    });
}
