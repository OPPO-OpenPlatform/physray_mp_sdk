/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "../common/modelviewer.h"
#include "../common/mesh-utils.h"

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct TriangleScene : ModelViewer {

    struct Options : ModelViewer::Options {
        Options() { rpmode = World::RayTracingRenderPackCreateParameters::SHADOW_TRACING; }
    };

    TriangleScene(SimpleApp & app, const Options & o): ModelViewer(app, o) {
        const float h = 5.0f;  // height
        const float l = -5.0f; // left
        const float r = 5.0f;  // right
        const float z = 0.0f;  // z value
        const float b = 0.0f;  // bottom

        const float v[][3] = {
            {l, b, z},
            {r, b, z},
            {0.0f, h, z},
        };

        std::vector<float> vertices;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) { vertices.push_back(v[i][j]); }
        }

        std::vector<float> normals;
        normals.push_back(0.0f);
        normals.push_back(1.0f);
        normals.push_back(0.0f);

        std::vector<uint32_t> iempty;
        std::vector<float>    empty;
        std::vector<float>    tangents;
        tangents = calculateSmoothTangents(iempty, empty, empty, normals, &lambertian->desc().anisotropic);

        auto mesh  = createNonIndexedMesh(vertices.size() / 3, vertices.data(), normals.data(), nullptr, tangents.data());
        mesh->name = "triangle";
        addMeshNode(nullptr, ph::rt::NodeTransform::Identity(), mesh, lambertian);

        const float         bmax[3] = {r, h, z};
        Eigen::AlignedBox3f bbox;
        bbox = {Eigen::Vector3f(v[0]), Eigen::Vector3f(bmax)};

        setupDefaultCamera(bbox);

        // add light
        ph::rt::NodeTransform lightTransform = ph::rt::NodeTransform::Identity();
        Eigen::Vector3f       _lightPosition;
        _lightPosition.x() = bbox.center().x();
        _lightPosition.y() = 20.0f;
        _lightPosition.z() = 20.0f;
        lightTransform.translate(_lightPosition);

        ph::rt::Node * _lightNode;
        _lightNode = this->scene->createNode({});
        _lightNode->setTransform(lightTransform);

        ph::rt::Light * _light = this->scene->createLight({});
        _lightNode->attachComponent(_light);

        ph::rt::Material::TextureHandle _shadowMapCube = textureCache->createShadowMapCube("point");
        lights.push_back(_light);

        auto desc         = _light->desc();
        desc.type         = ph::rt::Light::Type::POINT;
        desc.dimension[0] = 0.0f;
        desc.dimension[1] = 0.0f;
        desc.range        = 20.0f;
        desc.setEmission(100.f, 100.f, 100.f);
        _light->reset(desc);
        _light->shadowMap = _shadowMapCube;

        _light->shadowMapBias      = 0.001f;
        _light->shadowMapSlopeBias = 0.003f;
    }

private:
    const Options _options;
};