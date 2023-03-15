/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-mesh-builder.cpp
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
#include "pch.h"
#include "gltf-mesh-builder.h"
#include "mesh-utils.h"
#include <cfloat>

namespace gltf {

bool GLTFMeshBuilder::build(const tinygltf::Primitive & primitive, MeshData & meshData, Eigen::AlignedBox3f & bbox, skinning::SkinningData & skinData) {
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
    std::vector<float>    weights;
    std::vector<uint32_t> joints;

    // If this primitive has indices.
    if (primitive.indices >= 0) {
        // Fetch the indices first.
        _accessorReader.readAccessor(primitive.indices, meshData.indices.vec);
    }

    // Get the set of attributes for this primitive.
    const auto & attributes = primitive.attributes;

    // Iterate the primitive's set of attributes.
    // skinning::SkinningData skinData;
    bool getSkinnedData = (_skinnedMeshes != nullptr);
    for (auto iterator = attributes.begin(); iterator != attributes.end(); ++iterator) {
        // Which attribute of this primitive is being processed,
        // such as whether this attribute contains the primitive's
        // positions, normals, or texture coordinates.
        const std::string & name = iterator->first;

        // Parse according to the current attribute.
        // Mesh positions defining location of each triangle.
        if (name == "POSITION") {
            positionAccessor = readPositions(iterator->second, meshData.positions);

            if (getSkinnedData) skinData.origPositions = meshData.positions.vec;

            // Mesh normals
        } else if (name == "NORMAL") {
            readNormals(iterator->second, meshData.normals);

            if (getSkinnedData) skinData.origNormals = meshData.normals.vec;

            // Mesh texture coordinates.
        } else if (name == "TEXCOORD_0") {
            // Read the attribute.
            _accessorReader.readAccessor(iterator->second, meshData.texCoords.vec);

            // Stride of vec2
            meshData.texCoords.width = 2;

            // Mesh tangents.
        } else if (name == "TANGENT") {
            readTangents(iterator->second, meshData.tangents);
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
            } else if (name == "JOINTS_1" || name == "WEIGHTS_1") {
                // This means the mesh has more than 4 joints/weights.
                PH_LOGE("More than 4 joints/weights per-vertex is currently not supported. The mesh is ignored.");
                return false;
            }
            // If it's anything else.
        } else {
            // Log that there was an unsupported attribute.
            PH_LOGW("Unsupported attribute type '%s'", name.c_str());
        }
    }
    auto checkPositions = [](StridedBuffer<float> & positions) {
        bool pass = (positions.size() > 0);
        if (!pass) PH_LOGW("Primitive does not contain positions.");
        return pass;
    };

    auto checkNormals = [](StridedBuffer<float> & normals, StridedBuffer<float> & positions, StridedBuffer<uint32_t> & indices) {
        // Check normal
        if (normals.count() != positions.count()) {
            // Save what happened to the log.
            PH_LOGW("The mesh primitive has no normal. Generating normal from mesh positions...");

            // Calculate a default value for the normals
            // by calculating the direction of the face of each triangle.
            auto calculatedNormals = calculateTriangleNormals(indices.vec, positions.vec);
            normals.vec.assign(calculatedNormals.begin(), calculatedNormals.end());
            normals.width = 3;
        }
    };

    auto checkTangents = [](StridedBuffer<float> & tangents, StridedBuffer<float> & positions, StridedBuffer<uint32_t> & indices,
                            StridedBuffer<float> & texCoords, StridedBuffer<float> & normals) {
        // Check tangent
        if (tangents.count() != positions.count()) {
            if (texCoords.empty()) {
                PH_LOGW("The mesh primitive is missing both tangent and texcoord. Generating from normal and aniso...");
                auto nonAveragedTangents = calculateNonAveragedTangents(indices.vec, positions.vec, normals.vec);
                tangents.vec.assign(nonAveragedTangents.begin(), nonAveragedTangents.end());
                tangents.width = 3;
            } else {
                PH_LOGW("The mesh primitive is missing tangent. Generating from position and texcoord...");
                auto calculatedTangents = calculateSmoothTangents(indices.vec, positions.vec, texCoords.vec, normals.vec);
                tangents.vec.assign(calculatedTangents.begin(), calculatedTangents.end());
                // PhysRay uses tangents as vec3s, and the default calculation
                // saves them as vec3s as well so there is no need to
                // skip a w component as we do for GLTF tangents.
                tangents.width = 3;
            }
        }
    };

    // If positions were not defined, then that means to either skip this mesh or
    // that they are provided by some extension we probably aren't supporting.
    if (!checkPositions(meshData.positions)) return false;

    // Calculate from the position accessor
    // and iterating all positions if accessor does not define the min and max.
    // TODO: Update bounding box from morphs?
    bbox = toAlignedBox(*positionAccessor, meshData.positions);

    checkNormals(meshData.normals, meshData.positions, meshData.indices);

    // Check texcoord
    if (meshData.texCoords.count() != meshData.positions.count()) {
        PH_LOGW("Missing or incomplete texture coordinates.");
        meshData.texCoords.vec.clear();
    }

    checkTangents(meshData.tangents, meshData.positions, meshData.indices, meshData.texCoords, meshData.normals);

    // Since we were able to complete the mesh data, return true
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

const tinygltf::Accessor * GLTFMeshBuilder::readPositions(int accessorId, StridedBuffer<float> & positions) {
    // Read the attribute.
    _accessorReader.readAccessor(accessorId, positions.vec);

    // Three floats per position
    positions.width = 3;

    // Store the accessor so that we can calculate the bounding box from it.
    return &(_model->accessors[accessorId]);
}

void GLTFMeshBuilder::readNormals(int accessorId, StridedBuffer<float> & normals) {
    // Read the attribute.
    _accessorReader.readAccessor(accessorId, normals.vec);

    // Three floats per normal
    normals.width = 3;
}

void GLTFMeshBuilder::readTangents(int accessorId, StridedBuffer<float> & tangents) {
    // Read the attribute.
    _accessorReader.readAccessor(accessorId, tangents.vec);

    // PhysRay tangents have type float3,
    // but GLTF tangents have type VEC4, where the w component is a
    // sign value indicating handedness of the tangent basis.
    // Use a component count of 3 and a stride of 4
    // to skip the w component.
    tangents.width = 4;
}

Eigen::AlignedBox3f GLTFMeshBuilder::toAlignedBox(const tinygltf::Accessor & accessor, const StridedBuffer<float> & positions) {
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
        std::size_t positionCount = positions.count();

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
                std::size_t positionStartIndex = positionIndex * positions.width;

                // Determine the min for each dimension.
                min.x() = std::min(min.x(), positions.vec[positionStartIndex + 0]);
                min.y() = std::min(min.y(), positions.vec[positionStartIndex + 1]);
                min.z() = std::min(min.z(), positions.vec[positionStartIndex + 2]);
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
        std::size_t positionCount = positions.count();

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
                std::size_t positionStartIndex = positionIndex * positions.width;

                // Determine the max for each dimension.
                max.x() = std::max(max.x(), positions.vec[positionStartIndex + 0]);
                max.y() = std::max(max.y(), positions.vec[positionStartIndex + 1]);
                max.z() = std::max(max.z(), positions.vec[positionStartIndex + 2]);
            }
        }
    }

    // Aligned box to be returned.
    return Eigen::AlignedBox3f(min, max);
}

void GLTFMeshBuilder::MeshData::append(GLTFMeshBuilder::MeshData & input) {
    PH_ASSERT(positions.empty() == indices.empty());
    PH_ASSERT(!input.positions.empty());
    uint32_t vertexBase = (uint32_t) positions.count();

    if (input.indices.empty()) {
        if (!indices.empty()) {
            indices.vec.reserve(indices.vec.size() + input.positions.count());
            for (uint32_t i = 0; i < input.positions.count(); i++) { indices.vec.push_back(i + vertexBase); }
        }
    } else {
        if (indices.empty()) {
            indices.vec.reserve(vertexBase + input.indices.vec.size());
            for (uint32_t i = 0; i < vertexBase; i++) { indices.vec.push_back(i); }
        }
        indices.vec.reserve(indices.vec.size() + input.indices.vec.size());
        for (uint32_t index : input.indices.vec) { indices.vec.push_back(index + vertexBase); }
    }

    if (positions.width != input.positions.width) {
        PH_ASSERT(positions.empty());
        positions.width = input.positions.width;
    }
    positions.vec.insert(positions.vec.end(), input.positions.vec.begin(), input.positions.vec.end());

    PH_ASSERT(input.normals.count() == input.positions.count());
    if (normals.width != input.normals.width) {
        PH_ASSERT(normals.empty());
        normals.width = input.normals.width;
    }
    normals.vec.insert(normals.vec.end(), input.normals.vec.begin(), input.normals.vec.end());

    if (input.texCoords.empty()) {
        if (!texCoords.empty()) {
            PH_LOGW("Previous mesh primitive(s) in this mesh had texture coordinates, but this one does not.");
            texCoords.vec.resize(positions.count() * texCoords.width);
        }
    } else {
        if (texCoords.empty()) {
            if (vertexBase > 0) {
                PH_LOGW("Previous mesh primitive(s) in this mesh did not have texture coordinates, but this one does.");
                texCoords.vec.reserve(positions.count() * input.texCoords.width);
                texCoords.vec.resize(vertexBase * input.texCoords.width);
            }
            texCoords.width = input.texCoords.width;
        }
        if (!texCoords.empty()) { PH_ASSERT(texCoords.width == input.texCoords.width); }
        texCoords.vec.insert(texCoords.vec.end(), input.texCoords.vec.begin(), input.texCoords.vec.end());
        PH_ASSERT(input.texCoords.count() == input.positions.count());
    }

    if (input.tangents.empty()) {
        if (!tangents.empty()) {
            PH_LOGW("Previous mesh primitive(s) in this mesh had tangents, but this one does not.");
            tangents.vec.resize(positions.count() * tangents.width);
        }
    } else {
        if (tangents.empty()) {
            if (vertexBase > 0) {
                PH_LOGW("Previous mesh primitive(s) in this mesh did not have tangents, but this one does.");
                tangents.vec.reserve(positions.count() * input.tangents.width);
                tangents.vec.resize(vertexBase * input.tangents.width);
            }
            tangents.width = input.tangents.width;
        }
        if (tangents.width == input.tangents.width) {
            tangents.vec.insert(tangents.vec.end(), input.tangents.vec.begin(), input.tangents.vec.end());
        } else {
            PH_LOGW("Mesh includes primitives with mixed tangent strides: %d and %d", tangents.width, input.tangents.width);
            tangents.vec.reserve(tangents.vec.size() + tangents.width * input.tangents.count());
            auto width = std::min(tangents.width, input.tangents.width);
            for (size_t i = 0; i < input.tangents.size(); i++) {
                for (size_t j = 0; j < width; j++) { tangents.vec.push_back(input.tangents.vec[i * input.tangents.width + j]); }
                for (size_t j = tangents.width; j < input.tangents.width; j++) { tangents.vec.push_back(0); }
            }
        }
        PH_ASSERT(input.tangents.count() == input.positions.count());
    }
}

} // namespace gltf
