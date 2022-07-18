/**
 *
 */
#include "pch.h"
#include "gltf-mesh-builder.h"
#include "mesh-utils.h"
#include <cfloat>

namespace gltf {

bool GLTFMeshBuilder::build(const char * meshName, const tinygltf::Primitive & primitive, ph::rt::Mesh *& mesh, Eigen::AlignedBox3f & bbox) {
    // Only triangles are currently supported.
    // If this isn't a triangle.
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
        // Log a warning.
        PH_LOGW("Does not support non-triangle primitive mode (%d) yet.", primitive.mode);

        // Fail.
        return false;
    }

    // Position accessor. Used to calculate bounding box.
    const tinygltf::Accessor * positionAccessor = nullptr;

    // Create objects to store attributes in array form.
    std::vector<uint32_t> indices;
    std::vector<float>    positions;
    std::vector<float>    normals;
    std::vector<float>    texCoords;
    std::vector<float>    tangents;
    std::vector<float>    weights;
    std::vector<uint32_t> joints;

    // Parameters used to generate the mesh.
    ph::rt::World::MeshCreateParameters parameters = {};

    // If this primitive has indices.
    if (primitive.indices >= 0) {
        // Fetch the indices first.
        _accessorReader.readAccessor(primitive.indices, indices);
        parameters.indices = {indices.data(), indices.size(), 4};
    }

    // Get the set of attributes for this primitive.
    const auto & attributes = primitive.attributes;

    // Iterate the primitive's set of attributes.
    skinning::SkinningData skinData;
    bool                   getSkinnedData = (_skinnedMeshes != nullptr);
    for (auto iterator = attributes.begin(); iterator != attributes.end(); ++iterator) {
        // Which attribute of this primitive is being processed,
        // such as whether this attribute contains the primitive's
        // positions, normals, or texture coordinates.
        const std::string & name = iterator->first;

        // Parse according to the current attribute.
        // Mesh positions defining location of each triangle.
        if (name == "POSITION") {
            positionAccessor = readPositions(iterator->second, positions, parameters.positions, parameters.count);

            if (getSkinnedData) skinData.origPositions = positions;

            // Mesh normals
        } else if (name == "NORMAL") {
            readNormals(iterator->second, normals, parameters.normals);

            if (getSkinnedData) skinData.origNormals = normals;

            // Mesh texture coordinates.
        } else if (name == "TEXCOORD_0") {
            // Read the attribute.
            _accessorReader.readAccessor(iterator->second, texCoords);

            // Save it to the mesh parameters as vec2
            parameters.texcoords = {texCoords.data(), sizeof(float) * 2};

            // Mesh tangents.
        } else if (name == "TANGENT") {
            readTangents(iterator->second, tangents, parameters.tangents);
            // Mesh joints for skinned meshes.
        } else if (getSkinnedData) {
            if (name == "JOINTS_0") {
                // Read the attribute.
                _accessorReader.readAccessor(iterator->second, joints);

                // Save it to the mesh parameters as ivec4
                skinData.joints = joints;

                // Mesh weights for skinned meshes.
            } else if (name == "WEIGHTS_0") {
                // Read the attribute and ensure it is normalized.
                readWeights(iterator->second, weights);

                // Save it to the mesh parameters as vec4
                skinData.weights = weights;
            }
            // If it's anything else.
        } else {
            // Log that there was an unsupported attribute.
            PH_LOGW("Unsupported attribute type '%s'", name.c_str());
        }
    }
    auto checkPositions = [](std::vector<float> & positions) {
        bool pass = (positions.size() > 0);
        if (!pass) PH_LOGW("Primitive does not contain positions.");
        return pass;
    };

    auto checkNormals = [](size_t count, std::vector<float> & normals, std::vector<float> & positions, std::vector<uint32_t> & indices,
                           ph::rt::StridedBuffer<const float> & params) {
        // Check normal
        if (normals.size() / 3 != count) {
            // Save what happened to the log.
            PH_LOGW("The mesh primitive has no normal. Generating normal from mesh positions...");

            // Calculate a default value for the normals
            // by calculating the direction of the face of each triangle.
            normals = calculateTriangleNormals(indices, positions);
            params  = {normals.data(), sizeof(float) * 3};
        }
    };

    auto checkTangents = [](size_t count, std::vector<float> & tangents, std::vector<float> & positions, std::vector<uint32_t> & indices,
                            std::vector<float> & texCoords, ph::rt::StridedBuffer<const float> & params) {
        // Check tangent
        if (tangents.size() / 4 != count) {
            if (texCoords.empty()) {
                PH_LOGW("The mesh primitive is missing both tangent and texcoord.");
                params.clear();
            } else {
                PH_LOGW("The mesh primitive is missing tangent. Generating from position and texcoord...");
                tangents = calculateTriangleTangents(indices, positions, texCoords);
                // Jedi uses tangents as vec3s, and the default calculation
                // saves them as vec3s as well so there is no need to
                // skip a w component as we do for GLTF tangents.
                params = {tangents.data(), sizeof(float) * 3};
            }
        }
    };

    // If positions were not defined, then that means to either skip this mesh or
    // that they are provided by some extension we probably aren't supporting.
    if (!checkPositions(positions)) return false;

    // Calculate from the position accessor
    // and iterating all positions if accessor does not define the min and max.
    // TODO: Update bounding box from morphs?
    bbox = toAlignedBox(*positionAccessor, positions);

    checkNormals(parameters.count, normals, positions, indices, parameters.normals);

    // Check texcoord
    if (texCoords.size() / 2 != parameters.count) {
        PH_LOGW("Missing or incomplete texture coordinates.");
        parameters.texcoords.clear();
        texCoords.clear();
    }

    checkTangents(parameters.count, tangents, positions, indices, texCoords, parameters.tangents);

    // Check the skin.
    // If we have a skin in the first place.
    bool skinIncomplete = false;
    if (getSkinnedData && (joints.size() > 0)) {
        // Records whether or not skin was determined to be incomplete.
        // If joints are incomplete.
        if (joints.size() / 4 != parameters.count) {
            // Log what went wrong.
            PH_LOGW("Incomplete joints.");

            // Record that we cannot use skin.
            skinIncomplete = true;
        }

        // If weights are incomplete.
        if (weights.size() / 4 != parameters.count) {
            // Log what went wrong.
            PH_LOGW("Incomplete weights.");

            // Record that we cannot use skin.
            skinIncomplete = true;
        }
    }

    // Create the mesh and save it to the result.
    mesh = _world->createMesh(parameters);
    if ((_skinnedMeshes != nullptr) && (!skinIncomplete)) { _skinnedMeshes->emplace(std::make_pair(mesh, skinData)); }

    // Morph targets!
    if (_morphTargets) { // only bother doing this if a morph target manager has been set up
        const auto & morphtargets = primitive.targets;
        if (morphtargets.size() > 0) {
            bool            morphTargetIncomplete = false;
            MorphTargetData morphTargetData;
            morphTargetData.origAttribs.positions = positions;
            morphTargetData.origAttribs.normals   = normals;
            morphTargetData.origAttribs.tangents  = tangents;
            morphTargetData.origAttribs.count     = parameters.count;
            for (size_t targetId = 0; targetId < morphtargets.size(); targetId++) {
                auto &                             target = morphtargets[targetId];
                ph::rt::StridedBuffer<const float> morphPositions;
                ph::rt::StridedBuffer<const float> morphNormals;
                ph::rt::StridedBuffer<const float> morphTangents;
                MorphTargetData::TargetAttribs     morphParams;
                for (auto iterator = target.begin(); iterator != target.end(); ++iterator) {
                    const std::string & name = iterator->first;

                    if (name == "POSITION") {
                        readPositions(iterator->second, morphParams.positions, morphPositions, morphParams.count);
                        // Check attributes

                        if (morphParams.count != parameters.count) {
                            PH_LOGW("Morph target positions are a different length than original mesh positions.");
                            morphTargetIncomplete = true;
                            break; // If any morph target is incomplete, don't add any morph data at all.
                        }

                        morphTargetIncomplete = !checkPositions(morphParams.positions);
                        if (morphTargetIncomplete) break;
                    } else if (name == "NORMAL") {
                        readNormals(iterator->second, morphParams.normals, morphNormals);
                    } else if (name == "TANGENT") {
                        readTangents(iterator->second, morphParams.tangents, morphTangents);
                    }
                }

                // Use base mesh's indices for these checks/generations
                checkNormals(morphParams.count, morphParams.normals, morphParams.positions, indices, morphNormals);
                checkTangents(morphParams.count, morphParams.tangents, morphParams.positions, indices, texCoords, morphTangents);

                if ((_morphTargets != nullptr) && (!morphTargetIncomplete)) { morphTargetData.targets.emplace_back(morphParams); }
            }

            if ((_morphTargets != nullptr) && (!morphTargetIncomplete)) { _morphTargets->emplace(std::make_pair(mesh, morphTargetData)); }
        }
    }

    mesh->name = meshName;

    // Since we were able to create a mesh, return true.
    return true;
}

/**
 * Normalizes the given collection of weights to floats in
 * the range [0..1].
 * @param <T> Type of the collection to be normalized.
 * @param weights The collection of weights to be normalized.
 * @param normalizedWeights The collection of weights casted to floats and
 * normalized to the range [0..1]
 */
template<typename T>
static void normalizeWeights32(const std::vector<T> & weights, std::vector<float> & normalizedWeights) {
    // Make the weights array big enough to
    // hold everything we are adding to it.
    normalizedWeights.reserve(weights.size());

    // Get the maximum value of this type, casted to float.
    const float maxValue = (float) std::numeric_limits<T>::max();

    // Iterate all the weights to be converted.
    for (std::size_t index = 0; index < weights.size(); ++index) {
        // Get the value to be casted.
        T weight = weights[index];

        // Normalize the value.
        float normalizedWeight = ((float) weight) / maxValue;

        // Save it to the list of normalized weights.
        normalizedWeights.push_back(normalizedWeight);
    }
}

/**
 * Normalizes the given collection of weights to floats in
 * the range [0..1].
 *
 * Casts the type to doubles during division before casting to float to
 * preserve as much data as possible and handle integer types bigger than what
 * float can store.
 * @param <T> Type of the collection to be normalized.
 * @param weights The collection of weights to be normalized.
 * @param normalizedWeights The collection of weights casted to floats and
 * normalized to the range [0..1]
 */
template<typename T>
static void normalizeWeights64(const std::vector<T> & weights, std::vector<float> & normalizedWeights) {
    // Make the weights array big enough to
    // hold everything we are adding to it.
    normalizedWeights.reserve(weights.size());

    // Get the maximum value of this type, casted to float.
    const double maxValue = (double) std::numeric_limits<T>::max();

    // Iterate all the weights to be converted.
    for (std::size_t index = 0; index < weights.size(); ++index) {
        // Get the value to be casted.
        T weight = weights[index];

        // Normalize the value.
        double normalizedWeight = ((double) weight) / maxValue;

        // Save it to the list of normalized weights.
        normalizedWeights.push_back((float) normalizedWeight);
    }
}

void GLTFMeshBuilder::readWeights(int accessorId, std::vector<float> & weights) {
    // Fetch the desired accessor.
    const tinygltf::Accessor & accessor = _model->accessors[accessorId];

    // Get the number of weights impacting each vertex.
    std::size_t weightsPerVertex = AccessorReader::getComponentCount(accessor);

    // GLTF's specification says that weights should always be a vec4,
    // in other words, 4 weights per vertex.
    // Double check whether this gltf file defies that expectation.
    if (weightsPerVertex != 4) {
        // Tell user this gltf file is using an
        // unsupported number of weights per vertex.
        PH_LOGW("This GLTF file uses a non-standard number of weights per vertex, "
                "%d. Current implementation only ever expects this value to be 4.",
                weightsPerVertex);

        // End method without reading the weights since they are not supported.
        return;
    }

    // Weights are usually saved as floats, but can also be saved
    // as unsigned bytes and shorts.
    // Depending on the accessor type, we might need to normalize it.
    // The code below is designed to handle several types that aren't mentioned
    // by the specification just in case, such as double and unsigned int,
    // using the same rules as the types that are.
    // Signed integer types however are not supported at all and
    // will leave the collection empty.
    switch (accessor.componentType) {
    // If this is already a floating type.
    // Note that double isn't actually mentioned by the standard,
    // but just in case it shows up anyways, this is designed to
    // cast it to float and treat it the same way.
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        // We don't need to normalize it.
        // Just cast it to a float if necessary.
        _accessorReader.readAccessor(accessorId, weights);
        break;

    // If this is an unsigned int type, cast to floating point type and
    // divide it by its maximum value to normalize to the [0..1] range.
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        // Read the data in its original format.
        std::vector<uint8_t> unnormalizedWeights;
        _accessorReader.readAccessor(accessor, unnormalizedWeights);

        // Convert it to weighted floats.
        normalizeWeights32(unnormalizedWeights, weights);
    } break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        // Read the data in its original format.
        std::vector<uint16_t> unnormalizedWeights;
        _accessorReader.readAccessor(accessor, unnormalizedWeights);

        // Convert it to weighted floats.
        normalizeWeights32(unnormalizedWeights, weights);
    } break;
        // This type isn't mentioned by the standard, but just in case it
        // appears anyways, handle it in the same way as the smaller
        // unsigned ints.
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        // Read the data in its original format.
        std::vector<uint32_t> unnormalizedWeights;
        _accessorReader.readAccessor(accessor, unnormalizedWeights);

        // Convert it to weighted floats.
        normalizeWeights64(unnormalizedWeights, weights);
    } break;

    // If this is a type who's normalization process
    // isn't defined by the specification and its
    // implementation cannot be guessed.
    default:
        // Log a warning.
        PH_LOGW("Mesh weight attribute used unsupported component type %d", accessor.componentType);
        break;
    }
}

const tinygltf::Accessor * GLTFMeshBuilder::readPositions(int accessorId, std::vector<float> & positions, ph::rt::StridedBuffer<const float> & params,
                                                          size_t & count) {
    // Read the attribute.
    _accessorReader.readAccessor(accessorId, positions);

    // Save it to the mesh parameters as vec3
    params = {positions.data(), sizeof(float) * 3};

    // Tell parameters how many vertices there are.
    // Since all supported meshes are guaranteed to have positions, but
    // not indices, calculate the number of vertices from the count of
    // positions.
    // Positions are stored as VEC3s.
    // Total Vec3s = Total Components / (Number of components in a Vec3)
    count = positions.size() / 3;

    // Store the accessor so that we can calculate the bounding box from it.
    return &(_model->accessors[accessorId]);
}

void GLTFMeshBuilder::readNormals(int accessorId, std::vector<float> & normals, ph::rt::StridedBuffer<const float> & params) {
    // Read the attribute.
    _accessorReader.readAccessor(accessorId, normals);

    // Save it to the mesh parameters as vec3
    params = {normals.data(), sizeof(float) * 3};
}

void GLTFMeshBuilder::readTangents(int accessorId, std::vector<float> & tangents, ph::rt::StridedBuffer<const float> & params) {
    // Read the attribute.
    _accessorReader.readAccessor(accessorId, tangents);

    // Save it to the mesh parameters.
    // Jedi tangents have type float3,
    // but GLTF tangents have type VEC4, where the w component is a
    // sign value indicating handedness of the tangent basis.
    // Use a component count of 3 and a stride of 4
    // to skip the w component.
    params = {tangents.data(), sizeof(float) * 4};
}

Eigen::AlignedBox3f GLTFMeshBuilder::toAlignedBox(const tinygltf::Accessor & accessor, const std::vector<float> & positions) {
    // Records the min and max positions found.
    Eigen::Vector3f min;
    Eigen::Vector3f max;

    // If this accessor's min values are assigned.
    if (accessor.minValues.size() >= 3) {
        // Then save them directly.
        min = Eigen::Vector3f((float) accessor.minValues[0], (float) accessor.minValues[1], (float) accessor.minValues[2]);

        // If not available, calculate by hand.
    } else {
        // Calculate total number of positions to check.
        std::size_t positionCount = positions.size() / 3;

        // If there are no positions, set to zero.
        if (positionCount <= 0) {
            min = Eigen::Vector3f(0, 0, 0);

            // If there are positions.
        } else {
            // Initialize min to the maximum possible values to ensure they are overwritten.
            min = Eigen::Vector3f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

            // Iterate all positions.
            for (std::size_t positionIndex = 0; positionIndex < positionCount; ++positionIndex) {
                // Calculate starting index of this position.
                std::size_t positionStartIndex = positionIndex * 3;

                // Determine the min for each dimension.
                min.x() = std::min(min.x(), positions[positionStartIndex + 0]);
                min.y() = std::min(min.y(), positions[positionStartIndex + 1]);
                min.z() = std::min(min.z(), positions[positionStartIndex + 2]);
            }
        }
    }

    // If this accessor's max values are assigned.
    if (accessor.maxValues.size() >= 3) {
        // Then save them directly.
        max = Eigen::Vector3f((float) accessor.maxValues[0], (float) accessor.maxValues[1], (float) accessor.maxValues[2]);

        // If not available, calculate by hand.
    } else {
        // Calculate total number of positions to check.
        std::size_t positionCount = positions.size() / 3;

        // If there are no positions, set to zero.
        if (positionCount <= 0) {
            max = Eigen::Vector3f(0, 0, 0);

            // If there are positions.
        } else {
            // Initialize max to the minimum possible values to ensure they are overwritten.
            max = Eigen::Vector3f(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());

            // Iterate all positions.
            for (std::size_t positionIndex = 0; positionIndex < positionCount; ++positionIndex) {
                // Calculate starting index of this position.
                std::size_t positionStartIndex = positionIndex * 3;

                // Determine the max for each dimension.
                max.x() = std::max(max.x(), positions[positionStartIndex + 0]);
                max.y() = std::max(max.y(), positions[positionStartIndex + 1]);
                max.z() = std::max(max.z(), positions[positionStartIndex + 2]);
            }
        }
    }

    // Aligned box to be returned.
    return Eigen::AlignedBox3f(min, max);
}

} // namespace gltf
