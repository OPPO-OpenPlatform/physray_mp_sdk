/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>
#include "3rdparty/vulkan_minimal_compute/computeApplication.inl"
#include "shader/morph-targets.glsl"

using namespace ph;
using namespace ph::rt;
using namespace ph::va;

enum MorphMode {
    OFF,
    CPU,
    GPU,
};

// Target data for a given mesh
struct MorphTargetData {
    // Per-vertex attribute data for a given target
    struct TargetAttribs {
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> tangents;
        size_t             count; // position count - not sure if this is needed?
    };

    // target ID, target attribs
    std::vector<TargetAttribs> targets;

    TargetAttribs origAttribs;

    // Indexed by target ID
    std::vector<float> weights;

    bool dirty;
};

struct MorphTargetBuffer {
    // Static buffers
    va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, rt::device::Vertex> inputVertexBuffer;
    va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, rt::device::Vertex> outputVertexBuffer;
    va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, float>              weightsBuffer;
    va::StagedBufferObject<VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, rt::device::Vertex> targetsBuffer;

    PH_NO_COPY(MorphTargetBuffer);
    PH_NO_MOVE(MorphTargetBuffer);

    MorphTargetBuffer() = default;

    ~MorphTargetBuffer() {
        inputVertexBuffer.clear();
        outputVertexBuffer.clear();
        weightsBuffer.clear();
        targetsBuffer.clear();
    }
};

typedef std::map<ph::rt::Mesh *, MorphTargetData>   MorphTargetMap;
typedef std::map<ph::rt::Mesh *, MorphTargetBuffer> MorphBufferMap;

struct MorphTargetManager {

    MorphTargetManager() = default;

    ~MorphTargetManager() { cleanupApps(); }

private:
    // TODO: This currently assumes that everything will have vertex, normal, and tangent data. This
    //       needs to be changed so that the other cases (i.e. vertex only, vertex/normal) are handled.
    void allocateBuffers(MorphTargetData morphData, MorphTargetBuffer & morphBuffer);

    void cleanupApps();

    AutoHandle<VkShaderModule> createShader(const ConstRange<uint8_t> & data, const char * name);

    void initGPUMorphTargets();

    std::vector<uint8_t> loadEmbeddedResource(const std::string & name, bool quiet);

    void loadMorphTargetShader();

    void morphTargetsCPU();

    void morphTargetsGPU();

    bool reinitializeMorphTargets();

public:
    void describeImguiUI(MorphMode & settableMorphModeOption);

    MorphTargetMap * getMorphTargets() { return &_morphTargets; }

    const std::vector<float> & getWeights(const ph::rt::Mesh * mesh);

    void initializeMorphTargets(VulkanSubmissionProxy * vsp);

    bool setWeights(ph::rt::Mesh * mesh, const std::vector<float> & weights);

    void update(bool);

private:
    MorphTargetMap                         _morphTargets;
    MorphBufferMap                         _morphBuffers;
    MorphMode                              _morphMode;
    VulkanSubmissionProxy *                _vsp = nullptr;
    std::vector<MinimalComputeApplication> _appsToCleanup;
    va::AutoHandle<VkShaderModule>         _shaderModule;
    bool                                   _gpuInitialized = false;

    std::vector<float> empty;
};