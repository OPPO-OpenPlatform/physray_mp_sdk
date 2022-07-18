#pragma once

#include <ph/rt.h> // Camera, ph::va, ph::ImageProxy, Eigen
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
        ph::va::VulkanSubmissionProxy & vsp;
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

    void draw(VkCommandBuffer cmdBuffer, const Eigen::Matrix4f & proj, const Eigen::Matrix3f & camera, const Eigen::Vector3f & ambientColor = {0.f, 0.f, 0.f},
              float lodBias = 0.0f);

private:
    struct PushConstants {
        Eigen::Matrix4f pvw;     // proj * view * world matrix
        Eigen::Vector3f ambient; // ambient color
        float           lodBias;
        int             skyMapType;
        int             skyboxValid;
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
    void updatePushContants(VkCommandBuffer, const Eigen::Matrix4f &, const Eigen::Matrix3f &, const Eigen::Vector3f &, float);
    void constructRenderpass();
    void setupImageAndSampler();
    void createDummySkyboxTexture();

    typedef ph::va::StagedBufferObject<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, Vertex>  VertexBuffer;
    typedef ph::va::StagedBufferObject<VK_BUFFER_USAGE_INDEX_BUFFER_BIT, uint16_t> IndexBuffer;

    ConstructParameters _cp;

    VertexBuffer _vertexBufferObj;
    IndexBuffer  _indexBufferObj;

    std::vector<VkVertexInputAttributeDescription> _vertexAttribDesc;
    VkPipeline                                     _skyboxPipeline {};
    VkDescriptorSetLayout                          _descriptorSetLayout {};
    VkPipelineLayout                               _pipelineLayout {};
    VkDescriptorPool                               _descriptorPool {};
    VkDescriptorSet                                _descriptorSet {};
    VkFormat                                       _colorFormat {VK_FORMAT_UNDEFINED};
    VkFormat                                       _depthFormat {VK_FORMAT_UNDEFINED};
    VkSampler                                      _cubemapSampler {};
    ph::va::ImageObject                            _dummy;
};
