/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"

#include <ph/va.h>
#include "fatmesh.h"
#include "mesh-utils.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// ---------------------------------------------------------------------------------------------------------------------
//
static Eigen::Vector3f calcualteFaceNormal(const Eigen::Vector3f & a, const Eigen::Vector3f & b, const Eigen::Vector3f & c) {
    Eigen::Vector3f ab = (b - a).normalized();
    Eigen::Vector3f ac = (c - a).normalized();
    // Eigen::Vector3f bc = (c - b).normalized();
    // float dotA = ab.dot(ac);
    // float dotB = (-ab).do(bc);
    // float dotC = (-ac).do(-bc);

    // // Use the corner with the smallest cosine value as the origin to
    // // to calculate the normal
    // if (dotA <= dotB && dotA <= dotB) {
    //     return ab.cross(ac);
    // } else if (dotB <= dotA && dotB <= dotC)
    //     return (-ab).cross(bc);
    // } else
    //     return (-ac).cross(-bc);
    // }
    return ab.cross(ac);
};
// ---------------------------------------------------------------------------------------------------------------------
//
ph::FatMesh ph::FatMesh::loadObj(std::istream & stream) {
    // load the obj mesh
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream)) {
        if (!warn.empty()) PH_LOGW("%s", warn.c_str());
        if (!err.empty()) PH_LOGE("%s", err.c_str());
        return {};
    }

    FatMesh mesh;

    // Loop over shapes
    for (const auto & shape : shapes) {

        auto getVertex = [&](const tinyobj::index_t & i) -> Eigen::Vector3f {
            return {attrib.vertices[3 * i.vertex_index + 0], attrib.vertices[3 * i.vertex_index + 1], attrib.vertices[3 * i.vertex_index + 2]};
        };

        for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
            const auto & index = shape.mesh.indices[i];

            // load position
            mesh.position.emplace_back(attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                                       attrib.vertices[3 * index.vertex_index + 2]);

            // load normal
            if (index.normal_index >= 0) {
                mesh.normal.emplace_back(attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1],
                                         attrib.normals[3 * index.normal_index + 2]);
            } else {
                const auto &    i0 = shape.mesh.indices[i / 3 * 3 + 0];
                const auto &    i1 = shape.mesh.indices[i / 3 * 3 + 1];
                const auto &    i2 = shape.mesh.indices[i / 3 * 3 + 2];
                Eigen::Vector3f v0 = getVertex(i0);
                Eigen::Vector3f v1 = getVertex(i1);
                Eigen::Vector3f v2 = getVertex(i2);
                mesh.normal.push_back(calcualteFaceNormal(v0, v1, v2));
            }

            // load texcoord 0
            if (index.texcoord_index >= 0) {
                mesh.texcoord.emplace_back(attrib.texcoords[2 * index.texcoord_index + 0], 1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);
            } else {
                mesh.texcoord.emplace_back(0.f, 0.f);
            }

            if (mesh.bbox.isEmpty()) {
                mesh.bbox.min() = mesh.position.back();
                mesh.bbox.max() = mesh.position.back();
            } else {
                mesh.bbox.min() = mesh.position.back().cwiseMin(mesh.bbox.min());
                mesh.bbox.max() = mesh.position.back().cwiseMax(mesh.bbox.max());
            }
        }
    }

    // Calculate tangent vectors, but only when there is texture coordinate
    mesh.tangent.assign(mesh.position.size(), Eigen::Vector3f::Zero());
    std::vector<uint32_t> iempty;
    std::vector<float>    posflat, normflat, tcflat, tanflat;
    posflat.resize(mesh.position.size() * 3);
    normflat.resize(mesh.normal.size() * 3);
    tcflat.resize(mesh.position.size() * 2);
    for (size_t i = 0; i < mesh.normal.size(); i++) {
        const Eigen::Vector3f & n = mesh.normal[i];
        size_t                  i0, i1, i2;
        i0                        = i * 3 + 0;
        i1                        = i * 3 + 1;
        i2                        = i * 3 + 2;
        normflat[i0]              = n.x();
        normflat[i1]              = n.y();
        normflat[i2]              = n.z();
        const Eigen::Vector3f & p = mesh.position[i];
        posflat[i0]               = p.x();
        posflat[i1]               = p.y();
        posflat[i2]               = p.z();
        const Eigen::Vector2f & t = mesh.texcoord[i];
        tcflat[i * 2 + 0]         = t.x();
        tcflat[i * 2 + 1]         = t.y();
    }
    tanflat = calculateSmoothTangents(iempty, posflat, tcflat, normflat);
    for (size_t i = 0; i < mesh.normal.size(); i++) { mesh.tangent[i] = Eigen::Vector3f(tanflat[i * 3 + 0], tanflat[i * 3 + 1], tanflat[i * 3 + 2]); }

    return mesh;
}