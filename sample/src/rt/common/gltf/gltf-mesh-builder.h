/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-mesh-builder.h
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

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

#include "accessor-reader.h"
#include "gltf.h"
#include "../skinning.h"
#include "../morphtargets.h"

// forward declarations
class SceneBuildBuffers;

namespace gltf {

/**
 * Constructs a mesh from a tiny gltf object.
 */
class GLTFMeshBuilder {
public:
    /**
     * @param scene Used to create new objects.
     * @param model The tinygltf model who's items are being
     * instantiated in scene.
     */
    GLTFMeshBuilder(ph::rt::Scene * scene, const tinygltf::Model * model, skinning::SkinMap * skinnedMeshes, MorphTargetMap *, SceneBuildBuffers *)
        : _scene(scene), _model(model), _accessorReader(model), _skinnedMeshes(skinnedMeshes) {} //, _morphTargets(morphTargets), _sbb(sbb) {};

    /**
     *
     */
    virtual ~GLTFMeshBuilder() = default;

    /**
     * @return The scene used to create new objects.
     */
    ph::rt::Scene * getScene() { return _scene; }

    /**
     * @return The tinygltf model who's items are being instantiated in scene.
     */
    const tinygltf::Model * getModel() const { return _model; }

    template<typename T>
    struct StridedBuffer {
        std::vector<T> vec;
        uint16_t       width = 1;
        uint16_t       stride() const { return width * sizeof(T); };
        T *            data() { return vec.data(); };
        size_t         size() const { return vec.size(); };
        bool           empty() const { return vec.empty(); };
        size_t         count() const { return vec.size() / width; };
    };

    struct MeshData {
        StridedBuffer<uint32_t> indices;
        StridedBuffer<float>    positions;
        StridedBuffer<float>    normals;
        StridedBuffer<float>    texCoords;
        StridedBuffer<float>    tangents;

        void append(MeshData & input);
    };

    /**
     * Converts given primitive to PhysRay equivelant objects.
     * This must be called AFTER convertMaterials().
     * @param primitive The primitive being converted to a PhysRay mesh.
     * @param meshData The mesh data will be saved to if successful
     * @param bbox Stores the bounding box of the mesh.
     * Note that this does NOT take into account the skin and will therefore be
     * inaccurate for a skinned mesh.
     * @return true if successfully converted, false otherwise.
     */
    bool build(const tinygltf::Primitive & primitive, MeshData & meshData, Eigen::AlignedBox3f & bbox, skinning::SkinningData & skinData);

private:
    /**
     * Determines the aligned box from the given accessor if available.
     * If not, calculates it from all of the positions.
     * @param accessor The accessor defining the min and max values.
     * @param positions The list of positions if the accessor
     * left the min and max values empty.
     * @param the bounding box containing the values of the accessor,
     * or the list of positions if the accessor didn't reference the min and max.
     */
    static Eigen::AlignedBox3f toAlignedBox(const tinygltf::Accessor & accessor, const StridedBuffer<float> & positions);

    /**
     * Scene being used to create new lights.
     */
    ph::rt::Scene * _scene;

    /**
     * The tinygltf model who's items are being instantiated in scene.
     */
    const tinygltf::Model * _model;

    /**
     * Used to read binary data from the model.
     */
    AccessorReader _accessorReader;

    /**
     * @param accessorId Id of the accessor to read weights from.
     * @param weights The vector to which the result will be saved to.
     * If accessor isn't a float type, this method will normalize to
     * the range [0..1] by dividing by its maximum value and cast it to float.
     */
    void readWeights(int accessorId, std::vector<float> & weights);

    const tinygltf::Accessor * readPositions(int accessorId, StridedBuffer<float> & positions);

    void readNormals(int accessorId, StridedBuffer<float> & normals);

    void readTangents(int accessorId, StridedBuffer<float> & tangents);

    skinning::SkinMap * _skinnedMeshes;
    // MorphTargetMap *    _morphTargets;
    // SceneBuildBuffers * _sbb;
};

} // namespace gltf
