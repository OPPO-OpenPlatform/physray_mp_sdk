/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : skinning.h
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

#include <ph/rt-utils.h>

#include <memory>
#include <exception>

#include "sbb.h"
#include "shader/skinned-mesh.glsl"

namespace skinning {

struct SkinningData {
    // Indexed by joint id
    std::vector<ph::rt::Node *>        jointMatrices;
    std::vector<ph::rt::NodeTransform> prevJointMatrices;
    std::vector<Eigen::Matrix4f>       inverseBindMatrices;

    // Per-vertex data
    std::vector<uint32_t> joints;
    std::vector<float>    weights;
    std::vector<float>    origPositions;
    std::vector<float>    origNormals;

    // Vertex offsets within the overall mesh
    size_t submeshOffset;
    size_t submeshSize;
};

struct SkinningBuffer {
    ph::va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, Vertex>        inputVertexBuffer;
    ph::va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, Vertex>        outputVertexBuffer;
    ph::va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, WeightedJoint> weightsBuffer;
    ph::va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, mat4>          invBindMatricesBuffer;
    ph::va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, mat4>          jointsBuffer;
};

// Per-mesh skinning data (CPU)
typedef std::map<ph::rt::Mesh * const, std::vector<SkinningData>> SkinMap;

struct SkinnedMeshManager {
    SkinnedMeshManager(const ph::va::VulkanGlobalInfo & vgi);

    ~SkinnedMeshManager() { cleanup(); }

    SkinMap * skinDataMap() { return &_skinnedMeshes; };

    void initializeSkinning(ph::va::VulkanSubmissionProxy & vsp);

    void record(ph::va::DeferredHostOperation & dho, VkCommandBuffer cb);

private:
    void cleanup();

    bool allocateBuffers(ph::va::VulkanSubmissionProxy & vsp, const SkinningData & skinData, SkinningBuffer & skinBuffer);

    void applySkinning(ph::va::DeferredHostOperation & dho, VkCommandBuffer cb);

    void applyGPUSkinning(ph::rt::Mesh * meshPtr, ph::va::DeferredHostOperation & dho, VkCommandBuffer cb);

    bool checkForSkeletonChanges(SkinningData & skinnedMesh);

    ph::va::SimpleCompute::ConstructParameters createComputeCP(const ph::va::VulkanGlobalInfo & vgi);

    VkDescriptorBufferInfo getDescriptor(ph::va::BufferObject & bufferObj);

    VkDescriptorBufferInfo getDescriptor(const ph::va::BufferObject & bufferObj); // To handle the const return val of DynamicBuffer.g()

    void initGPUSkinning();

    void initPrevSkinMatrices();

    void loadSkinningShader(const ph::va::VulkanGlobalInfo &);

    std::vector<uint8_t> loadEmbeddedResource(const std::string & name, bool quiet);

    void updateJointMatrixBuffer(ph::va::DeferredHostOperation & dho, VkCommandBuffer cb, SkinningData & skinData, SkinningBuffer & skinBuffer);

private:
    // Per-mesh buffers for GPU skinning
    typedef std::map<ph::rt::Mesh * const, std::vector<SkinningBuffer>> BufferMap;

    ph::va::AutoHandle<VkShaderModule> _shaderModule;
    SkinMap                            _skinnedMeshes;
    BufferMap                          _skinningBuffers;
    ph::va::SimpleCompute *            _compute = nullptr;
};

} // namespace skinning
