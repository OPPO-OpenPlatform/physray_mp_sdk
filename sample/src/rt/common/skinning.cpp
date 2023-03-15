/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : skinning.cpp
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
#include "skinning.h"

#include <cmrc/cmrc.hpp>
CMRC_DECLARE(sampleasset);

using namespace ph;
using namespace ph::va;
using namespace ph::rt;
using namespace skinning;

static AutoHandle<VkShaderModule> createShader(const VulkanGlobalInfo & vgi, const ConstRange<uint8_t> & data, const char * name) {
    try {
        using namespace std::chrono;
        auto begin    = high_resolution_clock::now();
        auto shader   = createSPIRVShader(vgi, data, name);
        auto duration = duration_cast<nanoseconds>(high_resolution_clock::now() - begin).count();
        PH_LOGV("createSPIRVShader() returns in %s for shader %s", ns2str(duration).c_str(), name);
        return shader;
    } catch (...) { return {}; }
}

// ---------------------------------------------------------------------------------------------------------------------
// Constructor / Destructor

SkinnedMeshManager::SkinnedMeshManager(const VulkanGlobalInfo & vgi) {
    // Load the skinning compute shader from embedded resources
    PH_ASSERT(_shaderModule.empty());
    loadSkinningShader(vgi);
    // If shader module is still empty then setup failed
    if (_shaderModule.empty()) return;

    va::SimpleCompute::ConstructParameters cp {vgi};
    cp.cs = _shaderModule;
    for (size_t i = 0; i < 5; ++i) cp.bindings[i] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    cp.pushConstantsSize = 0; // This compute shader has no push constants
    _compute             = new SimpleCompute(cp);
}

// ---------------------------------------------------------------------------------------------------------------------
// Private Functions

static bool validateJointWeight(float weight, size_t joint, size_t maxJoints) {
    (void) weight;
    if (joint >= maxJoints) {
        PH_LOGE("joint index %zu is out of range [0, %zu)", joint, maxJoints);
        return false;
    }
    return true;
}

void SkinnedMeshManager::cleanup() { safeDelete(_compute); }

// TODO: This function needs to be updated when the buffers are converted from StagedBufferObject and
// DynamicBufferObject to simple BufferObjects.
bool SkinnedMeshManager::allocateBuffers(ph::va::VulkanSubmissionProxy & vsp, const skinning::SkinningData & skinData, SkinningBuffer & skinBuffer) {
    const auto & vgi = vsp.vgi();

    // Allocate input/output vertex buffers
    std::vector<Vertex> vertexData;
    for (size_t i = 0; i < skinData.origPositions.size(); i += 3) {
        Vertex v   = {};
        v.position = vec3(skinData.origPositions[i], skinData.origPositions[i + 1], skinData.origPositions[i + 2]);
        v.normal   = vec3(skinData.origNormals[i], skinData.origNormals[i + 1], skinData.origNormals[i + 2]);
        vertexData.emplace_back(v);
    }
    ConstRange<Vertex> vData(vertexData);
    skinBuffer.inputVertexBuffer.allocate(vgi, vData);
    skinBuffer.outputVertexBuffer.allocate(vgi, vData);

    // Allocate weights buffer
    PH_ASSERT(skinData.weights.size() == skinData.joints.size());
    std::vector<WeightedJoint> weightData;
    for (size_t i = 0; i < skinData.weights.size(); i += 4) {
        WeightedJoint wj = {};
        wj.weights       = vec4(skinData.weights[i], skinData.weights[i + 1], skinData.weights[i + 2], skinData.weights[i + 3]);
        wj.joints        = uvec4(skinData.joints[i], skinData.joints[i + 1], skinData.joints[i + 2], skinData.joints[i + 3]);
        if (!validateJointWeight(wj.weights.x(), wj.joints.x(), skinData.jointMatrices.size())) return false;
        weightData.emplace_back(wj);
    }
    skinBuffer.weightsBuffer.allocate(vgi, weightData);

    // Ensure that joint matrices and inverse bind matrices are valid
    if (skinData.inverseBindMatrices.size() != skinData.jointMatrices.size()) {
        PH_LOGE("incorrect inverse bind matrix array size.");
        return false;
    }

    // Allocate inverse bind matrices buffer
    skinBuffer.invBindMatricesBuffer.allocate(vgi, skinData.inverseBindMatrices);

    // Allocate joint matrices buffer
    std::vector<mat4> joints;
    for (size_t i = 0; i < skinData.jointMatrices.size(); i++) { joints.emplace_back(ph::rt::toEigen(skinData.jointMatrices[i]->worldTransform())); }
    skinBuffer.jointsBuffer.allocate(vgi, joints.size());

    // Sync the buffers to the gpu
    ph::va::SingleUseCommandPool pool(vsp);
    pool.syncexec([&](auto cb) {
        skinBuffer.inputVertexBuffer.sync2gpu(cb);
        skinBuffer.outputVertexBuffer.sync2gpu(cb);
        skinBuffer.weightsBuffer.sync2gpu(cb);
        skinBuffer.invBindMatricesBuffer.sync2gpu(cb);
        skinBuffer.jointsBuffer.sync2gpu(cb);
    });

    // done
    return true;
}

void SkinnedMeshManager::applyGPUSkinning(ph::rt::Mesh * meshPtr, DeferredHostOperation & dho, VkCommandBuffer cb) {
    va::beginVkDebugLabel(cb, meshPtr->name.c_str());
    auto & submeshes      = _skinnedMeshes[meshPtr];
    auto & submeshBuffers = _skinningBuffers[meshPtr];
    for (size_t i = 0; i < submeshes.size(); i++) {
        // Update the data in joint matrix buffer associated with this submesh
        auto & skinBuffer = submeshBuffers[i];
        updateJointMatrixBuffer(dho, cb, submeshes[i], skinBuffer);

        // Set up a the dispatch parameters for this submesh and dispatch the compute
        auto dp        = SimpleCompute::DispatchParameters {dho, cb};
        dp.bindings[0] = std::vector<VkDescriptorBufferInfo> {getDescriptor(skinBuffer.inputVertexBuffer.g)};
        dp.bindings[1] = std::vector<VkDescriptorBufferInfo> {getDescriptor(skinBuffer.outputVertexBuffer.g)};
        dp.bindings[2] = std::vector<VkDescriptorBufferInfo> {getDescriptor(skinBuffer.weightsBuffer.g)};
        dp.bindings[3] = std::vector<VkDescriptorBufferInfo> {getDescriptor(skinBuffer.invBindMatricesBuffer.g)};
        dp.bindings[4] = std::vector<VkDescriptorBufferInfo> {getDescriptor(skinBuffer.jointsBuffer.g)};
        dp.width       = skinBuffer.inputVertexBuffer.size();
        _compute->dispatch(dp);

        // Setup memory barrier to ensure the output is complete before it is used elsewhere
        VkBuffer        outputBuffer = skinBuffer.outputVertexBuffer.g.buffer;
        VkMemoryBarrier mBarrier     = {.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                    .pNext         = nullptr,
                                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mBarrier, 0, nullptr, 0, nullptr);

        // Call mesh.morph() to add this mesh to the queue of modified meshes that need to be processed
        auto vi = Mesh::VertexInput {
            .position = Mesh::VertexElement(outputBuffer, offsetof(Vertex, position), sizeof(Vertex)),
            .normal   = Mesh::VertexElement(outputBuffer, offsetof(Vertex, normal), sizeof(Vertex)),
        };
        meshPtr->morph(vi, submeshes[i].submeshOffset, submeshes[i].submeshSize);
    }
    va::endVkDebugLabel(cb);
}

bool SkinnedMeshManager::checkForSkeletonChanges(SkinningData & skinnedMesh) {
    bool result = false;
    for (size_t i = 0; i < skinnedMesh.jointMatrices.size(); i++) {
        const auto & nt   = skinnedMesh.jointMatrices[i]->worldTransform();
        const auto & prev = skinnedMesh.prevJointMatrices[i];
        if (!result && nt != (ph::rt::Float3x4) prev) result = true;
        skinnedMesh.prevJointMatrices[i] = nt;
    }
    return result;
}

va::SimpleCompute::ConstructParameters SkinnedMeshManager::createComputeCP(const VulkanGlobalInfo & vgi) {
    va::SimpleCompute::ConstructParameters cp {vgi};
    cp.cs = _shaderModule;
    for (size_t i = 0; i < 5; ++i) cp.bindings[i] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    cp.pushConstantsSize = 0; // This compute shader has no push constants
    return cp;
}

VkDescriptorBufferInfo SkinnedMeshManager::getDescriptor(BufferObject & bufferObj) {
    VkDescriptorBufferInfo info;
    info.buffer = bufferObj.buffer;
    info.offset = 0;
    info.range  = bufferObj.size;
    return info;
}

// TODO: Remove this function when DynamicBufferObject is replaced with simple BufferObject
VkDescriptorBufferInfo SkinnedMeshManager::getDescriptor(const BufferObject & bufferObj) {
    VkDescriptorBufferInfo info;
    info.buffer = bufferObj.buffer;
    info.offset = 0;
    info.range  = bufferObj.size;
    return info;
}

void SkinnedMeshManager::initPrevSkinMatrices() {
    for (auto & [mesh, submeshes] : _skinnedMeshes) {
        for (auto & skinnedMesh : submeshes) {
            for (size_t i = 0; i < skinnedMesh.jointMatrices.size(); i++) {
                NodeTransform nt = skinnedMesh.jointMatrices[i]->worldTransform();
                NodeTransform ntCopy(nt);
                skinnedMesh.prevJointMatrices.emplace_back(ntCopy);
            }
        }
    }
}

void SkinnedMeshManager::loadSkinningShader(const VulkanGlobalInfo & vgi) {
    auto blob = loadEmbeddedResource("shader/skinned-mesh.comp.spirv", false);
    if (blob.empty()) return;
    _shaderModule = createShader(vgi, blob, "skinned-mesh.spirv");
}

std::vector<uint8_t> SkinnedMeshManager::loadEmbeddedResource(const std::string & name, bool quiet) {
    auto fs = cmrc::sampleasset::get_filesystem();
    if (!fs.is_file(name)) {
        PH_LOGE("%s not found.", name.c_str());
        return {};
    }
    auto file = fs.open(name);
    if (!quiet) PH_LOGI("Embedded resource %s loaded.", name.c_str());
    return std::vector<uint8_t>((const uint8_t *) file.begin(), (const uint8_t *) file.end());
}

void SkinnedMeshManager::updateJointMatrixBuffer(DeferredHostOperation & dho, VkCommandBuffer cb, SkinningData & skinData, SkinningBuffer & skinBuffer) {
    static std::vector<mat4> jointMatrices;
    jointMatrices.resize(skinData.jointMatrices.size());
    for (size_t i = 0; i < jointMatrices.size(); ++i) {
        auto & m = jointMatrices[i];
        auto & n = skinData.jointMatrices[i];
        m        = ph::rt::toEigen(n->worldTransform());
    }
    dho.cmdUploadToGpu(cb, skinBuffer.jointsBuffer.g.buffer, 0, jointMatrices);
}

// ---------------------------------------------------------------------------------------------------------------------
// Public Functions

void SkinnedMeshManager::initializeSkinning(VulkanSubmissionProxy & vsp) {
    if (_skinnedMeshes.empty()) {
        cleanup();
        return;
    }

    // TODO: uncomment this line if joints checking is reenabled in update
    initPrevSkinMatrices();

    // Allocate required GPU buffers for all the loaded skinned meshes
    for (const auto & kv : _skinnedMeshes) {
        const auto & mesh      = kv.first;
        const auto & submeshes = kv.second;
        _skinningBuffers[mesh].resize(submeshes.size());
        for (size_t i = 0; i < submeshes.size(); i++) {
            SkinningBuffer & sb = _skinningBuffers[mesh][i];
            if (!allocateBuffers(vsp, submeshes[i], sb)) {
                PH_LOGE("Failed to allocate GPU buffers for submesh %zu of mesh %s", i, mesh->name.c_str());
                cleanup();
                return;
            }
        }
    }
}

void SkinnedMeshManager::record(DeferredHostOperation & dho, VkCommandBuffer cb) {
    if (!_compute) return;

    // NOTE: Skinning seems to run significantly faster WITHOUT the checks for whether the transforms have
    // changed. This does mean that skinning costs are still incurred even if the model hasn't been changed
    // BUT the difference seems to be ~20-40 fps with the dragonfly.gltf model, so, until the transform
    // checks can be optimized, this seems like the preferable solution. I left the alternative commented out
    // so that we can revisit it in the near future.
    for (auto & [mesh, skinnedMeshes] : _skinnedMeshes) {
        for (auto & skinnedMesh : skinnedMeshes) {
            if (checkForSkeletonChanges(skinnedMesh)) {
                applyGPUSkinning(mesh, dho, cb);
                break;
            }
        }
    }
}
