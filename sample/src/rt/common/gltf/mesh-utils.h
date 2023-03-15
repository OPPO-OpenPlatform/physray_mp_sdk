/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : mesh-utils.h
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
#include "../mesh-utils.h"

#include <algorithm>
#include <cstdint>
#include <vector>
#include <set>

namespace gltf {

/**
 * Flattens a set of indexed elements into a simple array.
 * For example, if you have the following elements with a component count of 2:
 * indices = {0, 1, 2, 0, 3, 2}, elements = {1, 0, 2, 1, 3, 2, 4, 2},
 * it would flatten to the following:
 * array={1, 0, 2, 1, 3, 2, 1, 0, 3, 2}
 * @param indices Container of indices.
 * @param buffer Container of values to be flattened.
 * @param indexOffset Starting position in the indices container to flatten from.
 * @param indexCount Number of indices to flatten.
 * @param elementStride Number of items between each element in the buffer.
 * @param componentCount Number of items in each element in the buffer container.
 * @param result The vector the results will be saved to.
 */
template<typename IndexType, typename BufferType>
static void flattenIndices(const std::vector<IndexType> & indices, const std::vector<BufferType> & buffer, std::size_t indexOffset, std::size_t indexCount,
                           std::size_t elementStride, std::size_t componentCount, std::vector<BufferType> & result) {
    // Make sure the result is big enough to hold everything we are adding to it.
    result.reserve(result.size() + indexCount * componentCount);

    // Calculate the index one past the last we want to iterate.
    std::size_t endIndex = indexOffset + indexCount;

    // Iterate all the indices we want to transfer.
    for (std::size_t index = indexOffset; index < endIndex; ++index) {
        // Get the index of the next element in the buffer.
        IndexType bufferIndex = indices[index];

        // Cast it to the index type used by std::vector and multiply by stride to get the component index.
        std::size_t componentStartIndex = ((std::size_t) bufferIndex) * elementStride;

        // Iterate all the components to be transfered.
        for (std::size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex) {
            // Get the desired element of the buffer.
            BufferType bufferValue = buffer[componentStartIndex + componentIndex];

            // Add it to the list of results.
            result.push_back(bufferValue);
        }
    }
}

/**
 * Flattens a set of indexed elements into a simple array.
 * For example, if you have the following elements with a component count of 2:
 * indices = {0, 1, 2, 0, 3, 2}, elements = {1, 0, 2, 1, 3, 2, 4, 2},
 * it would flatten to the following:
 * array={1, 0, 2, 1, 3, 2, 1, 0, 3, 2}.
 *
 * Uses 0 as the offset, indices.size() as the indexCount,
 * and componentCount as the elementStride
 * @param indices Container of indices.
 * @param buffer Container of values to be flattened.
 * @param componentCount Number of items in each element in the buffer container.
 * @param result The vector the results will be saved to.
 */
template<typename IndexType, typename BufferType>
static void flattenIndices(const std::vector<IndexType> & indices, const std::vector<BufferType> & buffer, std::size_t componentCount,
                           std::vector<BufferType> & result) {
    flattenIndices(indices, buffer, 0, indices.size(), componentCount, componentCount, result);
}

/**
 * Estimates normals from the clockwise cross product of 3d triangle positions.
 * @param <T> Type of the position and result arrays.
 * @param positions 3d coordinates of each triangle, with every 3 coordinates forming one triangle.
 * @return Array to which this will save the calculated 3d normals,
 * saving one normal for each coordinate.
 */
template<typename T>
static std::vector<T> calculateTriangleNormals(const std::vector<uint32_t> & indices, const std::vector<T> & positions) {
    // Type used for positions and vectors.
    typedef Eigen::Matrix<T, 3, 1> VectorType3;

    // Calculate total number of positions to calculate from
    // Number of position components / number of position dimensions.
    std::size_t positionCount = positions.size() / 3;

    // Total number of triangles to calculate for.
    std::size_t triangleCount = indices.empty() ? (positionCount / 3) : (indices.size() / 3);

    // collection of normals for each vertex
    std::vector<std::vector<VectorType3>> normals(positionCount);

    // Calculate face normals for each triangle
    for (std::size_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
        auto v0 = triangleIndex * 3;
        auto v1 = triangleIndex * 3 + 1;
        auto v2 = triangleIndex * 3 + 2;
        if (!indices.empty()) {
            v0 = indices[v0];
            v1 = indices[v1];
            v2 = indices[v2];
        }

        // Retrieve the 3 points of the triangle.
        VectorType3 position0(positions[v0 * 3 + 0], positions[v0 * 3 + 1], positions[v0 * 3 + 2]);
        VectorType3 position1(positions[v1 * 3 + 0], positions[v1 * 3 + 1], positions[v1 * 3 + 2]);
        VectorType3 position2(positions[v2 * 3 + 0], positions[v2 * 3 + 1], positions[v2 * 3 + 2]);

        // Calculate the face normal from the cross product of the 3 positions.
        VectorType3 edge1        = position1 - position0;
        VectorType3 edge2        = position2 - position1;
        VectorType3 crossProduct = edge1.cross(edge2);
        VectorType3 normal       = crossProduct.normalized();

        // Save this to the normal array.
        // All 3 points of the triangle will have the same value.
        normals[v0].push_back(normal);
        normals[v1].push_back(normal);
        normals[v2].push_back(normal);
    }

    auto getAverage = [](const std::vector<VectorType3> & values) -> VectorType3 {
        VectorType3 ave = VectorType3::Zero();
        for (const auto & v : values) { ave += v / (T) values.size(); }
        return ave;
    };

    // Calculate average normals for each vertex. Store the average to result array
    std::vector<T> result(normals.size() * 3);
    for (size_t i = 0; i < normals.size(); ++i) {
        auto n            = getAverage(normals[i]);
        result[i * 3 + 0] = n.x();
        result[i * 3 + 1] = n.y();
        result[i * 3 + 2] = n.z();
    }

    // done
    return result;
}

} // namespace gltf
