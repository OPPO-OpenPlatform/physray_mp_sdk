/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : morphtargets.cpp
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
#include "morphtargets.h"
#include "ui.h" // for imgui

#include <cmrc/cmrc.hpp>
CMRC_DECLARE(sampleasset);

// ---------------------------------------------------------------------------------------------------------------------
// PRIVATE FUNCTIONS

// TODO: This currently assumes that everything will have vertex, normal, and tangent data. May need
//       to change this so that the other cases (i.e. vertex only, vertex/normal) are handled.
void MorphTargetManager::allocateBuffers(MorphTargetData morphData, MorphTargetBuffer & morphBuffer) {
    const auto & vgi = _vsp->vgi();

    // Allocate input/output vertex buffers
    std::vector<rt::device::Vertex> vertexData;
    auto                            origAttrs = morphData.origAttribs;

    // Allocate input/output Vertex data buffers
    for (size_t i = 0; i < origAttrs.positions.size(); i += 3) {
        rt::device::Vertex v = {};
        v.position           = rt::device::vec3(origAttrs.positions[i], origAttrs.positions[i + 1], origAttrs.positions[i + 2]);
        v.normal             = rt::device::vec3(origAttrs.normals[i], origAttrs.normals[i + 1], origAttrs.normals[i + 2]);
        v.tangent            = rt::device::vec3(origAttrs.tangents[i], origAttrs.tangents[i + 1], origAttrs.tangents[i + 2]);
        vertexData.emplace_back(v);
    }
    ConstRange<rt::device::Vertex> vData(vertexData);
    morphBuffer.inputVertexBuffer.allocate(vgi, vData);
    morphBuffer.outputVertexBuffer.allocate(vgi, vData);

    // Allocate weights buffer
    PH_ASSERT(morphData.weights.size() == morphData.targets.size());
    ConstRange<float> wData(morphData.weights);
    morphBuffer.weightsBuffer.allocate(vgi, wData);

    // Allocate the targets buffer
    auto                            targets = morphData.targets;
    std::vector<rt::device::Vertex> targetData;
    for (size_t i = 0; i < targets.size(); i++) {
        for (size_t j = 0; j < targets[i].positions.size(); j += 3) {
            rt::device::Vertex t = {};
            t.position           = rt::device::vec3(targets[i].positions[j], targets[i].positions[j + 1], targets[i].positions[j + 2]);
            t.normal             = rt::device::vec3(targets[i].normals[j], targets[i].normals[j + 1], targets[i].normals[j + 2]);
            t.tangent            = rt::device::vec3(targets[i].tangents[j], targets[i].tangents[j + 1], targets[i].tangents[j + 2]);
            targetData.emplace_back(t);
        }
    }

    ConstRange<rt::device::Vertex> tData(targetData);
    morphBuffer.targetsBuffer.allocate(vgi, tData);

    // Sync the buffers to the gpu
    ph::va::SingleUseCommandPool pool(*_vsp);
    pool.syncexec([&](auto cb) {
        morphBuffer.inputVertexBuffer.sync2gpu(cb);
        morphBuffer.outputVertexBuffer.sync2gpu(cb);
        morphBuffer.weightsBuffer.sync2gpu(cb);
        morphBuffer.targetsBuffer.sync2gpu(cb);
    });
}

void MorphTargetManager::cleanupApps() {
    for (auto & app : _appsToCleanup) app.cleanup();
    _appsToCleanup.clear();
}

AutoHandle<VkShaderModule> MorphTargetManager::createShader(const ConstRange<uint8_t> & data, const char * name) {
    try {
        using namespace std::chrono;
        auto begin    = high_resolution_clock::now();
        auto shader   = createSPIRVShader(_vsp->vgi(), data, name);
        auto duration = duration_cast<nanoseconds>(high_resolution_clock::now() - begin).count();
        PH_LOGV("createSPIRVShader() returns in %s for shader %s", ns2str(duration).c_str(), name);
        return shader;
    } catch (...) { return {}; }
}

void MorphTargetManager::initGPUMorphTargets() {
    // TODO: Is there any case where this has to be checked again here?
    // Ensure we have morph target data
    // if (_morphTargets.empty()) {
    //    PH_LOGE("initGPUMorphTargets - Cannot initialize Morph Targets if _morphTargets is empty."
    //            " _morphMode will be set to OFF.");
    //    _morphMode = MorphMode::OFF;
    //    return;
    //}

    // Allocate buffers for each mesh with morph targets
    for (auto & [mesh, morphData] : _morphTargets) {
        MorphTargetBuffer & mb = _morphBuffers[mesh];
        allocateBuffers(morphData, mb);
    }
}

std::vector<uint8_t> MorphTargetManager::loadEmbeddedResource(const std::string & name, bool quiet) {
    auto fs = cmrc::sampleasset::get_filesystem();
    if (!fs.is_file(name)) {
        PH_LOGE("%s not found.", name.c_str());
        return {};
    }
    auto file = fs.open(name);
    if (!quiet) PH_LOGI("Embedded resource %s loaded.", name.c_str());
    return std::vector<uint8_t>((const uint8_t *) file.begin(), (const uint8_t *) file.end());
}

void MorphTargetManager::loadMorphTargetShader() {
    // Try to load from the embedded assets
    auto blob = loadEmbeddedResource("shader/morph-targets.comp.spirv", false);
    if (!blob.empty()) {
        auto shader = createShader(blob, "morph-targets.spirv");
        if (shader) {
            _shaderModule = shader;
        } else {
            PH_LOGE("The morph target shader embedded resource was located but could not be loaded."
                    " _morphMode will be set to OFF.");
            _morphMode = MorphMode::OFF;
        }
    } else {
        PH_LOGE("Could not load the morph target shader from embedded resources. _morphMode will "
                "be set to OFF.");
        _morphMode = MorphMode::OFF;
    }
}

void MorphTargetManager::morphTargetsCPU() {
    PH_THROW("obsolete");
    // for (auto & [mesh, morphData] : _morphTargets) {
    //     if (morphData.dirty) {
    //         std::vector<float> newPositions;
    //         std::vector<float> newNormals;
    //         std::vector<float> newTangents;
    //         int                posCount = int(morphData.origAttribs.positions.size());
    //         // Assumes that if original data had normals and tangents,
    //         // all the targets should too. If this isn't the case, it will probably crash
    //         bool hasNormals  = morphData.origAttribs.normals.size() > 0;
    //         bool hasTangents = morphData.origAttribs.tangents.size() > 0;
    //         newPositions.resize(posCount);
    //         if (hasNormals) newNormals.resize(posCount);
    //         if (hasTangents) newTangents.resize(posCount);
    //         int targetCount = int(morphData.weights.size());
    //         for (int i = 0; i < posCount; i++) { // TODO: Flip these loops so that we can optimize the weight == 0 case?
    //             newPositions[i] = morphData.origAttribs.positions[i];
    //             if (hasNormals) newNormals[i] = morphData.origAttribs.normals[i];
    //             if (hasTangents) newTangents[i] = morphData.origAttribs.tangents[i];
    //             for (int j = 0; j < targetCount; j++) { // TODO: Flip these loops so that we can optimize the weight == 0 case?
    //                 newPositions[i] += morphData.weights[j] * morphData.targets[j].positions[i];
    //                 if (hasNormals) newNormals[i] += morphData.weights[j] * morphData.targets[j].normals[i];
    //                 if (hasTangents) newTangents[i] += morphData.weights[j] * morphData.targets[j].tangents[i];
    //             }
    //         }

    //         ph::rt::Mesh::MorphParameters mp;
    //         mp.positions = {(const float *) newPositions.data(), 3 * sizeof(float)};
    //         if (hasNormals) mp.normals = {(const float *) newNormals.data(), 3 * sizeof(float)};
    //         if (hasTangents) mp.tangents = {(const float *) newTangents.data(), 3 * sizeof(float)};

    //         mesh->morph(mp);

    //         morphData.dirty = false;
    //     }
    // }
}

void MorphTargetManager::morphTargetsGPU() {
    for (auto & [mesh, morphData] : _morphTargets) {

        // Temp code for running the ComputeShader code for this mesh, a shared pool/command buffer will
        // be used in the near future once the larger code refactoring is complete
        ph::va::SingleUseCommandPool pool(*_vsp);
        auto                         cb = pool.create();

        // Update the weights buffer
        if (morphData.dirty) {
            ConstRange<float> crWeights(morphData.weights);
            _morphBuffers[mesh].weightsBuffer.update(0, crWeights);
            _morphBuffers[mesh].weightsBuffer.sync2gpu(cb);
        }

        // Set up a VkDescriptorBufferInfo for this mesh
        std::vector<VkDescriptorBufferInfo> descBufInfos = {
            {
                /*.buffer = */ _morphBuffers[mesh].inputVertexBuffer.g.buffer,
                /*.offset = */ 0,
                /*.range = */ _morphBuffers[mesh].inputVertexBuffer.g.size,
            },
            {
                /*.buffer = */ _morphBuffers[mesh].outputVertexBuffer.g.buffer,
                /*.offset = */ 0,
                /*.range = */ _morphBuffers[mesh].outputVertexBuffer.g.size,
            },
            {
                /*.buffer = */ _morphBuffers[mesh].weightsBuffer.g.buffer,
                /*.offset = */ 0,
                /*.range = */ _morphBuffers[mesh].weightsBuffer.g.size,
            },
            {
                /*.buffer = */ _morphBuffers[mesh].targetsBuffer.g.buffer,
                /*.offset = */ 0,
                /*.range = */ _morphBuffers[mesh].targetsBuffer.g.size,
            },
        };

        // Set up the MinimalComputeApplication for the morph targets
        _appsToCleanup.push_back(_vsp->vgi().device);
        MinimalComputeApplication & app = _appsToCleanup.back();
        app.createDesriptorSetLayout((int) descBufInfos.size());
        app.createDescriptorSet(descBufInfos);

        // Load the compute shader used for morph targets
        PH_REQUIRE(!_shaderModule.empty());
        auto ssci = ph::va::util::shaderStageCreateInfo(_shaderModule.get(), VK_SHADER_STAGE_COMPUTE_BIT);
        app.createComputePipeline(ssci);

        // Bind, dispatch, and excute the pipeline
        app.bindAndDispatch(cb, _morphBuffers[mesh].inputVertexBuffer.size());

        // TODO: need to hook up with the new dynamic scene interface
        // // Use the buffers version of morph() to update the mesh
        // ph::rt::Mesh::VertexBuffer vb = {};
        // vb.buffer                     = _morphBuffers[mesh].outputVertexBuffer.g.buffer;
        // vb.stride                     = sizeof(rt::device::Vertex);
        // vb.vertexCount                = _morphBuffers[mesh].outputVertexBuffer.g.size / vb.stride;
        // vb.offset                     = 0;
        // vb.offsets                    = {offsetof(device::Vertex, position), offsetof(device::Vertex, normal)};
        // mesh->morph(vb, cb);
        // pool.finish(cb);
        // mesh->syncVertexFromBuffer();
    }
}

// TODO: FINISH MODIFYING THIS FUNCTION
bool MorphTargetManager::reinitializeMorphTargets() {
    if (_morphTargets.empty()) {
        PH_LOGI("MorphTargetsManager cannot be initialized without _morphTargets having valid "
                "data. _morphMode will be set to OFF and Morph Targets will not run.");
        _morphMode = MorphMode::OFF;
        return false;
    }

    if (_morphMode == MorphMode::GPU) {
        loadMorphTargetShader();
        initGPUMorphTargets();
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// PUBLIC FUNCTIONS

void MorphTargetManager::describeImguiUI(MorphMode & settableMorphModeOption) {
    if (ImGui::TreeNode("Morph Mode")) {
        if (ImGui::BeginListBox("", ImVec2(0, 4 * ImGui::GetTextLineHeightWithSpacing()))) {
            if (ImGui::Selectable("Off", _morphMode == MorphMode::OFF)) {
                _morphMode = settableMorphModeOption = MorphMode::OFF;
                // DEBUG
                PH_LOGI("Morph Mode = OFF");
            }
            if (ImGui::Selectable("CPU", _morphMode == MorphMode::CPU)) {
                _morphMode = settableMorphModeOption = MorphMode::CPU;
                bool success                         = reinitializeMorphTargets();
                if (!success) { _morphMode = settableMorphModeOption = MorphMode::OFF; }
                // DEBUG
                std::string modeStr = _morphMode == MorphMode::OFF ? "OFF" : "CPU";
                PH_LOGI("Morph Mode = " + modeStr);
            }
            if (ImGui::Selectable("GPU", _morphMode == MorphMode::GPU)) {
                _morphMode = settableMorphModeOption = MorphMode::GPU;
                if (!_gpuInitialized) {
                    bool success = reinitializeMorphTargets();
                    if (!success) { _morphMode = settableMorphModeOption = MorphMode::OFF; }
                }
                // DEBUG
                std::string modeStr = _morphMode == MorphMode::OFF ? "OFF" : "GPU";
                PH_LOGI("Morph Mode = " + modeStr);
            }
            ImGui::EndListBox();
        }
        ImGui::TreePop();
    }
}

const std::vector<float> & MorphTargetManager::getWeights(const ph::rt::Mesh * mesh) {
    auto morphItr = _morphTargets.find(((ph::rt::Mesh *) mesh));
    if (morphItr == _morphTargets.end()) {
        return empty;
    } else {
        return morphItr->second.weights;
    }
}

void MorphTargetManager::initializeMorphTargets(VulkanSubmissionProxy * vsp) {
    if (!vsp) {
        throw new std::runtime_error("SkinnedMeshManager cannot be initialized with a nullptr value"
                                     "for the VulkanSubmissionProxy.");
    }
    _vsp = vsp;
    if (_morphTargets.empty()) {
        PH_LOGI("MorphTargetsManager cannot be initialized without _morphTargets having valid "
                "data. _morphMode will be set to OFF and Morph Targets will not run.");
        _morphMode = MorphMode::OFF;
        return;
    }

    if (_morphMode == MorphMode::GPU) {
        loadMorphTargetShader();
        initGPUMorphTargets();
    }
}

bool MorphTargetManager::setWeights(ph::rt::Mesh * mesh, const std::vector<float> & weights) {
    auto morphItr = _morphTargets.find(mesh);
    if (morphItr == _morphTargets.end())
        return false;
    else {
        MorphTargetData & morphTargetData = morphItr->second;

        // Already set
        if (morphTargetData.weights.size() == weights.size()) {
            morphTargetData.dirty   = morphTargetData.weights != weights;
            morphTargetData.weights = weights;
            return true;
        }

        size_t numTargets = morphTargetData.targets.size();

        // No morph target data
        if (numTargets <= 0) return false;

        // Morph target data mismatches weights
        if (numTargets != weights.size()) {
            PH_LOGW("Primitive morph target count does not match mesh weight count.");
            return false;
        }

        for (size_t i = 0; i < weights.size(); i++) morphTargetData.weights.push_back(float(weights[i]));
        morphTargetData.dirty = true;
        return true;
    }
}

void MorphTargetManager::update(bool) {
    if (_morphMode == MorphMode::CPU) {
        morphTargetsCPU();
    } else if (_morphMode == MorphMode::GPU) {
        morphTargetsGPU();
        cleanupApps();
    }
}
