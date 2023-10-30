/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/va.h>
#include <ph/rps.h>
#include <map>
#include <utility>

// ---------------------------------------------------------------------------------------------------------------------
/// A simple skybox implementation.
class Skybox {

public:
    enum class SkyMapType : int {
        EMPTY = 0, ///< textureless. When set it this type, the skymap member of ConstructParameters is ignored.
        CUBE  = 1, ///< cuemap
        EQUIRECT,  ///< Equirectangular projected texture.
    };

    struct ConstructParameters {
        ph::va::SimpleRenderLoop &      loop;
        ph::AssetSystem &               assetSys;
        uint32_t                        width {};
        uint32_t                        height {};
        VkRenderPass                    pass {};
        ph::rt::Material::TextureHandle skymap {};
        SkyMapType                      skymapType = SkyMapType::CUBE;
    };

    Skybox(const ConstructParameters &);

    ~Skybox();

    void resize(uint32_t w, uint32_t h);

    void draw(VkCommandBuffer cmdBuffer, const Eigen::Matrix4f & proj, const Eigen::Matrix3f & camera, float saturation, float gamma, bool outputSRGB,
              float skyboxRotation, float lodBias = 0.0f, const Eigen::Vector3f & ambientColor = {0.f, 0.f, 0.f});

private:
    struct PushConstants {
        Eigen::Matrix4f pvw;     // proj * view * world matrix
        Eigen::Vector3f ambient; // ambient color
        float           lodBias;
        int             skyMapType;
        int             skyboxValid;
        float           saturation;
        float           gamma;
        int             outputSRGB;
        float           skyboxRotation;
    };

    struct Vertex {
        Eigen::Vector3f pos;
        Eigen::Vector3f normal;

        Vertex() = default;
        Vertex(float x, float y, float z, float nx, float ny, float nz) {
            pos    = Eigen::Vector3f(x, y, z);
            normal = Eigen::Vector3f(nx, ny, nz);
        }
    };

    void createPipelines();
    void createBoxGeometry(float width, float height, float depth);
    void constructRenderpass();
    void setupImageAndSampler();
    void createDummySkyboxTexture();

    typedef ph::va::StagedBufferObject<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, Vertex>  VertexBuffer;
    typedef ph::va::StagedBufferObject<VK_BUFFER_USAGE_INDEX_BUFFER_BIT, uint16_t> IndexBuffer;

    ConstructParameters                    _cp;
    ph::rps::Ref<ph::rps::Factory>         _fac;
    ph::rps::Ref<ph::rps::GraphicsProgram> _program;
    ph::rps::Ref<ph::rps::ArgumentSet>     _args;
    ph::rps::Ref<ph::rps::Buffer>          _vb, _ib, _ub;
};
