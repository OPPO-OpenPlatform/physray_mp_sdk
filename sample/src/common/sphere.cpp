#include "pch.h"
#include "sphere.h"
#include <cmath>
#include <map>

using namespace ph;
using namespace Eigen;

// ---------------------------------------------------------------------------------------------------------------------
//
struct Mesh {
    std::vector<Vector3f> vertices;
    std::vector<uint32_t> triangles;

    void addTriangles(std::initializer_list<uint32_t> il) {
        PH_ASSERT(0 == (il.size() % 3));
        triangles.insert(triangles.end(), il.begin(), il.end());
    }

    std::vector<Vector3f> nonIndexed() const {
        std::vector<Vector3f> v;
        v.reserve(triangles.size());
        for (auto i : triangles)
            v.push_back(vertices[i]);
        return v;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// create basic 20 face icosahedron mesh
static Mesh icosahedron() {

    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    Mesh mesh;

    // Vertices
    mesh.vertices.push_back(Vector3f(-1.f, t, 0.f).normalized());
    mesh.vertices.push_back(Vector3f(1.f, t, 0.f).normalized());
    mesh.vertices.push_back(Vector3f(-1.f, -t, 0.f).normalized());
    mesh.vertices.push_back(Vector3f(1.f, -t, 0.f).normalized());
    mesh.vertices.push_back(Vector3f(0.f, -1.f, t).normalized());
    mesh.vertices.push_back(Vector3f(0.f, 1.f, t).normalized());
    mesh.vertices.push_back(Vector3f(0.f, -1.f, -t).normalized());
    mesh.vertices.push_back(Vector3f(0.f, 1.f, -t).normalized());
    mesh.vertices.push_back(Vector3f(t, 0.f, -1.f).normalized());
    mesh.vertices.push_back(Vector3f(t, 0.f, 1.f).normalized());
    mesh.vertices.push_back(Vector3f(-t, 0.f, -1.f).normalized());
    mesh.vertices.push_back(Vector3f(-t, 0.f, 1.f).normalized());

    // Faces
    mesh.addTriangles({
        0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4,  11, 10, 2,  10, 7, 6, 7, 1, 8,
        3, 9,  4, 3, 4, 2, 3, 2, 6, 3, 6, 8,  3, 8,  9,  4, 9, 5, 2, 4,  11, 6,  2,  10, 8,  6, 7, 9, 8, 1,
    });

    return mesh;
}

// ---------------------------------------------------------------------------------------------------------------------
//
struct Edge {
    uint32_t v0;
    uint32_t v1;

    Edge(uint32_t v0, uint32_t v1): v0(v0 < v1 ? v0 : v1), v1(v0 < v1 ? v1 : v0) {}

    bool operator<(const Edge & rhs) const { return v0 < rhs.v0 || (v0 == rhs.v0 && v1 < rhs.v1); }
};

// ---------------------------------------------------------------------------------------------------------------------
//
static uint32_t subdivideEdge(uint32_t f0, uint32_t f1, const Vector3f & v0, const Vector3f & v1, Mesh & io_mesh, std::map<Edge, uint32_t> & io_divisions) {

    const Edge edge(f0, f1);
    auto       it = io_divisions.find(edge);
    if (it != io_divisions.end()) { return it->second; }

    const Vector3f v = (0.5f * (v0 + v1)).normalized();
    const uint32_t f = (uint32_t) io_mesh.vertices.size();
    io_mesh.vertices.emplace_back(v);
    io_divisions.emplace(edge, f);
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------
// subdivide sphere mesh by inserting one vertex at the center of each triangle
static Mesh subdivideMesh(const Mesh & meshIn) {
    Mesh meshOut;

    meshOut.vertices = meshIn.vertices;

    std::map<Edge, uint32_t> divisions; // Edge -> new vertex

    for (uint32_t i = 0; i < meshIn.triangles.size() / 3; ++i) {
        const uint32_t f0 = meshIn.triangles[i * 3];
        const uint32_t f1 = meshIn.triangles[i * 3 + 1];
        const uint32_t f2 = meshIn.triangles[i * 3 + 2];

        const Vector3f v0 = meshIn.vertices[f0];
        const Vector3f v1 = meshIn.vertices[f1];
        const Vector3f v2 = meshIn.vertices[f2];

        const uint32_t f3 = subdivideEdge(f0, f1, v0, v1, meshOut, divisions);
        const uint32_t f4 = subdivideEdge(f1, f2, v1, v2, meshOut, divisions);
        const uint32_t f5 = subdivideEdge(f2, f0, v2, v0, meshOut, divisions);

        meshOut.addTriangles({f0, f3, f5});
        meshOut.addTriangles({f3, f1, f4});
        meshOut.addTriangles({f4, f2, f5});
        meshOut.addTriangles({f3, f4, f5});
    }

    return meshOut;
}

// ---------------------------------------------------------------------------------------------------------------------
//
std::vector<Eigen::Vector3f> buildIcosahedronUnitSphere(uint32_t subdivide) {

    Mesh m1 = icosahedron();

    for (size_t i = 0; i < subdivide; ++i) {
        m1 = subdivideMesh(m1);
    }

    return m1.nonIndexed();
}
