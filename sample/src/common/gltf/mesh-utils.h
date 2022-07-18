/**
 *
 */
#pragma once

#include <ph/rt.h>

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
        for (const auto & v : values) {
            ave += v / (T) values.size();
        }
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

/**
 * Estimates the tangents from the position and texture coordinate arrays.
 * @param <T> Type of the position and result arrays.
 * @param positions 3d coordinates of each triangle,
 * with every 3 coordinates forming one triangle.
 * @param textureCoordinates 2d texture coordinates of each triangle,
 * with every 3 coordinates forming one triangle.
 * @return Array to which this will save the calculated tangents,
 * saving one tangent for each coordinate. Tangents are saved as float3,
 * ignoring the w component.
 */
template<typename T>
static std::vector<T> calculateTriangleTangents(const std::vector<uint32_t> & indices, const std::vector<T> & positions,
                                                const std::vector<T> & textureCoordinates) {
    // Vector type used for positions.
    typedef Eigen::Matrix<T, 3, 1> VectorType3;

    // Vector type used for texture coordinates.
    typedef Eigen::Matrix<T, 2, 1> VectorType2;

    // Calculate total number of positions to calculate from
    // Number of position components / number of position dimensions.
    std::size_t positionCount = positions.size() / 3;

    // Total number of triangles to calculate for.
    std::size_t triangleCount = indices.empty() ? (positionCount / 3) : (indices.size() / 3);

    struct Tangent {
        std::vector<VectorType3> values;
        VectorType3              ave;
        std::set<size_t>         neighbors;
    };

    std::vector<Tangent> tangents(positionCount);

    // Calculate tangents for each triangle
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

        // Retrieve the 3 texture coordinates of the triangle.
        VectorType2 textureCoordinates0(textureCoordinates[v0 * 2 + 0], textureCoordinates[v0 * 2 + 1]);
        VectorType2 textureCoordinates1(textureCoordinates[v1 * 2 + 0], textureCoordinates[v1 * 2 + 1]);
        VectorType2 textureCoordinates2(textureCoordinates[v2 * 2 + 0], textureCoordinates[v2 * 2 + 1]);

        // Calculate tangent from position and UV coordinates.
        VectorType3     edge1      = position1 - position0;
        VectorType3     edge2      = position2 - position0;
        VectorType2     deltaUV1   = textureCoordinates1 - textureCoordinates0;
        VectorType2     deltaUV2   = textureCoordinates2 - textureCoordinates0;
        float           detInverse = 1.0f / (deltaUV1.x() * deltaUV2.y() - deltaUV2.x() * deltaUV1.y());
        Eigen::Vector3f tangent    = Eigen::Vector3f(detInverse * (deltaUV2.y() * edge1 - deltaUV1.y() * edge2)).normalized();

        // add the tangent to value array only if it contains finite value.
        if (std::isfinite(tangent.x()) && std::isfinite(tangent.y()) && std::isfinite(tangent.z())) {
            tangents[v0].values.push_back(tangent);
            tangents[v1].values.push_back(tangent);
            tangents[v2].values.push_back(tangent);
        }

        // update neighbors for each vertex.
        tangents[v0].neighbors.insert(v1);
        tangents[v0].neighbors.insert(v2);
        tangents[v1].neighbors.insert(v0);
        tangents[v1].neighbors.insert(v2);
        tangents[v2].neighbors.insert(v0);
        tangents[v2].neighbors.insert(v1);
    }

    auto getAverage = [](const std::vector<VectorType3> & values) -> VectorType3 {
        VectorType3 ave = VectorType3::Zero();
        for (const auto & v : values) {
            ave += v / (T) values.size();
        }
        return ave;
    };

    // Calculate average tangent value for each vertex
    std::set<size_t> invalid;
    for (size_t i = 0; i < tangents.size(); ++i) {
        auto & t = tangents[i];
        if (t.values.empty()) {
            invalid.insert(i);
        } else {
            t.ave = getAverage(t.values);
        }
    }

    // Process all vertices that does not have tangent by averaging tangent of its neighbors
    while (!invalid.empty()) {
        // remember the current count of invalid vertices.
        auto oldCount = invalid.size();

        // go through all invalid vertices one by one
        for (auto iter = invalid.begin(); iter != invalid.end();) {
            auto & t = tangents[*iter];
            PH_ASSERT(t.values.empty());

            // gather neighbors tangent value into array "v"
            std::vector<VectorType3> v;
            for (auto & n : t.neighbors) {
                auto & a = tangents[n].ave;
                if (std::isfinite(a.x()) && std::isfinite(a.y()) && std::isfinite(a.z())) { v.push_back(a); }
            }

            // If none of the neighbor contains valid tangent, then we have to skip this vertex for now.
            if (v.empty()) {
                ++iter;
                continue;
            }

            // Calculate average value of neighbour's tangent value, assign to current vertex
            t.ave = getAverage(v);

            // This vertex now has a finite tangent value. So remove it from the invalid list.
            iter = invalid.erase(iter);
        }

        if (oldCount == invalid.size()) {
            // We have gone through all invalid vertices. But can't generate even a single new tangent.
            // This is where we give up...
            PH_LOGE("Can't generate valid tangent for all vertices.");
            break;
        }
    }

    // TODO: for the remaining invalid vertices, calculate a tangent from normal.

    // Done. Store result to T array.
    std::vector<T> result(tangents.size() * 3);
    for (size_t i = 0; i < tangents.size(); ++i) {
        auto & t          = tangents[i].ave;
        result[i * 3 + 0] = t.x();
        result[i * 3 + 1] = t.y();
        result[i * 3 + 2] = t.z();
    }
    return result;
}

} // namespace gltf
