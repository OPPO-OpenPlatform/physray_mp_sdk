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

#pragma once

#include <ph/va.h>
using namespace ph;

template<typename T>
static void unvalidatedTangentFromNormal(const Eigen::Matrix<T, 3, 1> & n, Eigen::Matrix<T, 3, 1> & t, bool useX) {
    T z2 = n.z() * n.z();

    if (useX)
        t = Eigen::Matrix<T, 3, 1>(-n.z(), T(0), n.x()) / std::sqrt(n.x() * n.x() + z2);
    else
        t = Eigen::Matrix<T, 3, 1>(T(0), n.z(), -n.y()) / std::sqrt(n.y() * n.y() + z2);
    // t.normalize();
}

static inline bool useXAniso(const float * aniso) { return (aniso != nullptr) ? (*aniso <= 0) : true; }

template<typename T>
static void validTangentFromNormal(const Eigen::Matrix<T, 3, 1> & n, Eigen::Matrix<T, 3, 1> & t, const float * aniso) {
    T    z2   = n.z() * n.z();
    bool useX = (float(z2) == 0.f) ? (std::isnormal(n.x())) : useXAniso(aniso);
    unvalidatedTangentFromNormal(n, t, useX);
}

template<typename T>
static bool tangentValid(Eigen::Matrix<T, 3, 1> tangent) {
    bool ret = std::isfinite(tangent.x()) && std::isfinite(tangent.y()) && std::isfinite(tangent.z());
    ret &= std::isnormal(tangent.norm());
    return ret;
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
static std::vector<T> calculateSmoothTangents(const std::vector<uint32_t> & indices, const std::vector<T> & positions,
                                              const std::vector<T> & textureCoordinates, const std::vector<T> & normals, const float * aniso = nullptr) {
    // Vector type used for positions.
    typedef Eigen::Matrix<T, 3, 1> VectorType3;

    // Vector type used for texture coordinates.
    typedef Eigen::Matrix<T, 2, 1> VectorType2;

    // Calculate total number of positions to calculate from
    // Number of position components / number of position dimensions.
    std::size_t positionCount = positions.empty() ? normals.size() / 3 : positions.size() / 3;

    // Total number of triangles to calculate for.
    std::size_t triangleCount = indices.empty() ? (positionCount / 3) : (indices.size() / 3);

    struct Tangent {
        std::vector<VectorType3> values;
        VectorType3              ave;
        std::set<size_t>         neighbors;
        std::size_t              index;
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

        if (!textureCoordinates.empty() && !positions.empty()) {
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

            // add the tangent to value array only if it contains finite, non-zero tangent
            if (tangentValid(tangent)) {
                tangents[v0].values.push_back(tangent);
                tangents[v1].values.push_back(tangent);
                tangents[v2].values.push_back(tangent);
            }
        } else {
            // To prevent discontinuities in anisotropic surface appearance,
            // try generating tangents along a consistent direction based on anisotropy
            // and averaging to fill in gaps where this results in invalid tangents.
            VectorType3 normal0(normals[v0 * 3 + 0], normals[v0 * 3 + 1], normals[v0 * 3 + 2]);
            VectorType3 normal1(normals[v1 * 3 + 0], normals[v1 * 3 + 1], normals[v1 * 3 + 2]);
            VectorType3 normal2(normals[v2 * 3 + 0], normals[v2 * 3 + 1], normals[v2 * 3 + 2]);

            VectorType3 tan0, tan1, tan2;
            bool        anisoDir = useXAniso(aniso);
            unvalidatedTangentFromNormal(normal0, tan0, anisoDir);
            unvalidatedTangentFromNormal(normal1, tan1, anisoDir);
            unvalidatedTangentFromNormal(normal2, tan2, anisoDir);
            if (tangentValid(tan0)) tangents[v0].values.push_back(tan0);
            if (tangentValid(tan1)) tangents[v1].values.push_back(tan1);
            if (tangentValid(tan2)) tangents[v2].values.push_back(tan2);
        }

        // update neighbors for each vertex.
        tangents[v0].neighbors.insert(v1);
        tangents[v0].neighbors.insert(v2);
        tangents[v1].neighbors.insert(v0);
        tangents[v1].neighbors.insert(v2);
        tangents[v2].neighbors.insert(v0);
        tangents[v2].neighbors.insert(v1);
        tangents[v0].index = v0;
        tangents[v1].index = v1;
        tangents[v2].index = v2;
    }

    auto getAverage = [](const std::vector<VectorType3> & values) -> VectorType3 {
        VectorType3 ave = VectorType3::Zero();
        for (const auto & v : values) { ave += v / (T) values.size(); }
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
            if (!tangentValid(t.ave)) invalid.insert(i);
        }
    }

    // Process all vertices that does not have tangent by averaging tangent of its neighbors
    while (!invalid.empty()) {
        // remember the current count of invalid vertices.
        auto oldCount = invalid.size();

        // go through all invalid vertices one by one
        for (auto iter = invalid.begin(); iter != invalid.end();) {
            auto & t = tangents[*iter];
            // PH_ASSERT(t.values.empty());

            // gather neighbors tangent value into array "v"
            std::vector<VectorType3> v;
            for (auto & n : t.neighbors) {
                auto & a = tangents[n].ave;
                if (tangentValid(a)) { v.push_back(a); }
            }

            // If none of the neighbor contains valid tangent, then we have to skip this vertex for now.
            if (v.empty()) {
                ++iter;
                continue;
            }

            // Calculate average value of neighbour's tangent value, assign to current vertex
            t.ave = getAverage(v);

            // This vertex now has a finite tangent value. So remove it from the invalid list.
            if (tangentValid(t.ave))
                iter = invalid.erase(iter);
            else
                ++iter; // current average produces an invalid tangent. skip for now.
        }

        if (oldCount == invalid.size()) {
            // We have gone through all invalid vertices. But can't generate even a single new tangent.
            // Fall back to non-averaged normals for the remaining.
            PH_LOGW("Can't generate valid tangent for all vertices. Falling back to non-averaged normal-based tangents.");

            for (auto iter = invalid.begin(); iter != invalid.end(); iter++) {
                auto &      t = tangents[*iter];
                VectorType3 n(normals[t.index * 3 + 0], normals[t.index * 3 + 1], normals[t.index * 3 + 2]);
                validTangentFromNormal(n, t.ave, aniso);
                if (!tangentValid(t.ave)) {
                    PH_LOGW("Can't generate valid tangent at all. Assigning (1, 0, 0).");
                    t.ave = VectorType3(1.f, 0.f, 0.f);
                }
            }
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

/**
 * Assigns tangents based on normal and aniso values. This may cause
 * discontinuities, since tangents are not averaged.
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
static std::vector<T> calculateNonAveragedTangents(const std::vector<uint32_t> & indices, const std::vector<T> & positions, const std::vector<T> & normals,
                                                   const float * aniso = nullptr) {
    // Vector type used for positions.
    typedef Eigen::Matrix<T, 3, 1> VectorType3;

    // Calculate total number of positions to calculate from
    // Number of position components / number of position dimensions.
    std::size_t positionCount = positions.empty() ? normals.size() / 3 : positions.size() / 3;

    // Total number of triangles to calculate for.
    std::size_t triangleCount = indices.empty() ? (positionCount / 3) : (indices.size() / 3);

    std::vector<T> tangents(normals.size());

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

        VectorType3 t(tangents[v0], tangents[v1], tangents[v2]);
        VectorType3 n(normals[v0], normals[v1], normals[v2]);
        validTangentFromNormal(n, t, aniso);
        if (tangentValid(t)) {
            tangents[v0] = t.x();
            tangents[v1] = t.y();
            tangents[v2] = t.z();
        } else {
            PH_LOGW("Can't generate valid tangent at all. Assigning (1, 0, 0).");
            tangents[v0] = 1.f;
            tangents[v1] = 0.f;
            tangents[v2] = 0.f;
        }
    }

    return tangents;
}