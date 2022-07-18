#pragma once
#include <ph/rt.h>

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

typedef std::map<ph::rt::Mesh *, MorphTargetData> MorphTargetMap;

struct MorphTargetManager {
    MorphTargetMap             _morphTargets;
    std::vector<float>         empty;
    MorphTargetMap *           getMorphTargets() { return &_morphTargets; }
    const std::vector<float> & getWeights(const ph::rt::Mesh * mesh) {
        auto morphItr = _morphTargets.find(((ph::rt::Mesh *) mesh));
        if (morphItr == _morphTargets.end()) {
            return empty;
        } else {
            return morphItr->second.weights;
        }
    }
    bool setWeights(ph::rt::Mesh * mesh, const std::vector<float> & weights) {
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

            for (size_t i = 0; i < weights.size(); i++)
                morphTargetData.weights.push_back(float(weights[i]));
            morphTargetData.dirty = true;
            return true;
        }
    }

    void update(bool) {
        for (auto & [mesh, morphData] : _morphTargets) {
            if (morphData.dirty) {
                std::vector<float> newPositions;
                std::vector<float> newNormals;
                std::vector<float> newTangents;
                int                posCount = int(morphData.origAttribs.positions.size());
                // Assumes that if original data had normals and tangents,
                // all the targets should too. If this isn't the case, it will probably crash
                bool hasNormals  = morphData.origAttribs.normals.size() > 0;
                bool hasTangents = morphData.origAttribs.tangents.size() > 0;
                newPositions.resize(posCount);
                if (hasNormals) newNormals.resize(posCount);
                if (hasTangents) newTangents.resize(posCount);
                int targetCount = int(morphData.weights.size());
                for (int i = 0; i < posCount; i++) {
                    newPositions[i] = morphData.origAttribs.positions[i];
                    if (hasNormals) newNormals[i] = morphData.origAttribs.normals[i];
                    if (hasTangents) newTangents[i] = morphData.origAttribs.tangents[i];
                    for (int j = 0; j < targetCount; j++) {
                        newPositions[i] += morphData.weights[j] * morphData.targets[j].positions[i];
                        if (hasNormals) newNormals[i] += morphData.weights[j] * morphData.targets[j].normals[i];
                        if (hasTangents) newTangents[i] += morphData.weights[j] * morphData.targets[j].tangents[i];
                    }
                }

                ph::rt::Mesh::MorphParameters mp;
                mp.positions = {(const float *) newPositions.data(), 3 * sizeof(float)};
                if (hasNormals) mp.normals = {(const float *) newNormals.data(), 3 * sizeof(float)};
                if (hasTangents) mp.tangents = {(const float *) newTangents.data(), 3 * sizeof(float)};

                mesh->morph(mp);
                morphData.dirty = false;
            }
        }
    }
};