/**
 *
 */
#pragma once

#include <ph/rt.h>

#include "accessor-reader.h"
#include "gltf.h"
#include "../skinning.h"
#include "../morphtargets.h"

namespace gltf {

/**
 * Constructs a mesh from a tiny gltf object.
 */
class GLTFMeshBuilder {
public:
    /**
     * @param world Used to create new objects.
     * @param model The tinygltf model who's items are being
     * instantiated in world.
     */
    GLTFMeshBuilder(ph::rt::World * world, const tinygltf::Model * model, skinning::SkinMap * skinnedMeshes = nullptr, MorphTargetMap * morphTargets = nullptr)
        : _world(world), _model(model), _accessorReader(model), _skinnedMeshes(skinnedMeshes), _morphTargets(morphTargets) {};

    /**
     *
     */
    virtual ~GLTFMeshBuilder() = default;

    /**
     * @return The world used to create new objects.
     */
    ph::rt::World * getWorld() { return _world; }

    /**
     * @return The tinygltf model who's items are being instantiated in world.
     */
    const tinygltf::Model * getModel() const { return _model; }

    /**
     * Converts given primitive to PhysRay equivelant objects.
     * This must be called AFTER convertMaterials().
     * @param name name of the mesh. This is optinoal. Set to nullptr, if there's none.
     * @param primitive The primitive being converted to a PhysRay mesh.
     * @param mesh The pointer the new PhysRay mesh will be saved to if succesful.
     * @param bbox Stores the bounding box of the mesh.
     * Note that this does NOT take into account the skin and will therefore be
     * inaccurate for a skinned mesh.
     * @return true if successfully converted, false otherwise.
     */
    bool build(const char * name, const tinygltf::Primitive & primitive, ph::rt::Mesh *& mesh, Eigen::AlignedBox3f & bbox);

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
    static Eigen::AlignedBox3f toAlignedBox(const tinygltf::Accessor & accessor, const std::vector<float> & positions);

    /**
     * World being used to create new lights.
     */
    ph::rt::World * _world;

    /**
     * The tinygltf model who's items are being instantiated in world.
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

    const tinygltf::Accessor * readPositions(int accessorId, std::vector<float> & positions, ph::rt::StridedBuffer<const float> & params, size_t & count);

    void readNormals(int accessorId, std::vector<float> & normals, ph::rt::StridedBuffer<const float> & params);

    void readTangents(int accessorId, std::vector<float> & tangents, ph::rt::StridedBuffer<const float> & params);

    skinning::SkinMap * _skinnedMeshes;
    MorphTargetMap *    _morphTargets;
};

} // namespace gltf
