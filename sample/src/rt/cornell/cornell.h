/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "../common/modelviewer.h"

#include <ph/rt-utils.h>

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct CornellBoxScene : ModelViewer {
    struct Options : ModelViewer::Options {
        float scaling = 1.0f;
    };

    CornellBoxScene(SimpleApp & app, const Options & o): ModelViewer(app, o), _options(o) {
        // remove skybox
        recordParameters.irradianceMap = {};
        recordParameters.reflectionMap = {};

        // add ambient lighting
        recordParameters.ambientLight = {.01f, .01f, .01f};

        recordParameters.transparencySettings.fresnelCutoff = 0.2f;

        // create materials
        auto baseDesc = []() { return rt::Material::Desc {}; };
        // auto red      = world->create("red",      baseDesc().setAlbedo(1.f, 0.f, 0.f));
        // auto green    = world->create("green",    baseDesc().setAlbedo(0.f, 1.f, 0.f));
        auto yellow = world->create("yellow", baseDesc().setAlbedo(1.f, 1.f, 0.f));
        // auto magenta  = world->create("magenta",  baseDesc().setAlbedo(1.f, 0.f, 1.f));
        auto mirror = world->create("mirror", baseDesc().setAlbedo(0.f, 1.0f, 1.f).setMetalness(1.f).setRoughness(0.f));
        auto glass  = world->create("glass", baseDesc().setMetalness(0.f).setRoughness(0.f).setOpaqueness(0.f).setAlbedo(1.0f, .3f, 1.f));

        const float scaling  = o.scaling; // scaling factor for X and Y
        const float handness = o.leftHanded ? -1.0f : 1.0f;

        _meshNode1 = addBox("box1", 0.5f * scaling, 0.5f * scaling, 0.5f * scaling, glass, nullptr,
                            sg::Transform::make({-0.5f * scaling, 0.f * scaling, 0.5f * scaling * handness}, // position
                                                Eigen::Quaternionf::Identity()                               // rotation
                                                ));

        _meshNode2 = addIcosahedron("sphere0", .4f * scaling, 2, mirror, nullptr,
                                    sg::Transform::make({0.6f * scaling, 0.1f * scaling, -0.3f * scaling * handness}, // position
                                                        Eigen::Quaternionf::Identity()                                // rotation
                                                        ));

        _meshNode3 = addIcosahedron("sphere1", 1.f * scaling, 0, yellow, nullptr,
                                    sg::Transform::make({-0.4f * scaling, -0.4f * scaling, -0.4f * scaling * handness}, // position
                                                        Eigen::Quaternionf::Identity(),                                 // rotation
                                                        {.6f * scaling, .6f * scaling, .6f * scaling}                   // scaling
                                                        ));

        // addDeformableMesh();

        Eigen::AlignedBox3f bbox;
        bbox.min() << -scaling, -scaling, -scaling;
        bbox.max() << scaling, scaling, scaling;
        addCornellBoxToScene(bbox);
        setupDefaultCamera(bbox);
        setupShadowRenderPack();

        addCeilingLight(bbox, 10.0f, 0.3f * scaling, o.isPathTraced());
    }

    void update() override {
        using namespace std::chrono_literals;
        ModelViewer::update();

        auto totalMicroseconds = app().gameTime().sinceBeginning;

        // animate _lightNode
        if (animated()) {
            constexpr auto cycle = std::chrono::duration_cast<std::chrono::microseconds>(8s).count();
            auto           angle = PI * 2.0f * (float) (totalMicroseconds.count() % cycle) / (float) cycle;
            if (_options.leftHanded) angle *= -1.f;
            auto radius = 0.7f * _options.scaling;
            auto lightX = std::sin(angle) * radius;
            auto lightZ = std::cos(angle) * radius;

            sg::Node &    lightNode = *lights.back();
            sg::Transform transform = lightNode.worldTransform();
            transform.translation() = Eigen::Vector3f(lightX, transform.translation().y(), lightZ);
            lightNode.setWorldTransform(transform);
        }

        // animate _meshNode1
        if (animated() && _meshNode1) {
            constexpr auto         cycle           = std::chrono::duration_cast<std::chrono::microseconds>(5s).count();
            auto                   angle           = PI * -2.0f * (float) (totalMicroseconds.count() % cycle) / (float) cycle;
            static Eigen::Vector3f baseTranslation = _meshNode1->transform().translation();
            sg::Transform          tr              = _meshNode1->transform();

            // Do _NOT_ use auto to declare "t" for the reason described here:
            // http://eigen.tuxfamily.org/dox-devel/TopicPitfalls.html#title3
            // If you do, the whole calcuation will result in garbage in release build.
            // In short, Eigen library is not auto friendly :(
            Eigen::Vector3f translation = baseTranslation + Eigen::Vector3f(0.f, 0.5f * _options.scaling * std::sin(angle), 0.f);

            auto r = Eigen::AngleAxisf(angle, Eigen::Vector3f::UnitY()) * Eigen::AngleAxisf(PI * 0.25f, Eigen::Vector3f(1.0f, 1.0f, 1.0f).normalized());
            tr.translation() = translation;
            tr.linear()      = r.matrix();
            _meshNode1->setTransform(tr);
        }

        // animate the _meshNode3
        if (animated() && _meshNode3) {
            // calculate scaling factor
            constexpr auto cycle   = std::chrono::duration_cast<std::chrono::microseconds>(1s).count();
            auto           angle   = PI * -2.0f * (float) (totalMicroseconds.count() % cycle) / (float) cycle;
            float          scaling = std::sin(angle) * 0.25f + 0.75f; // scaling in range [0.5, 1.0];

            static sg::Transform baseTransform = _meshNode3->transform();
            auto                 newTransform  = baseTransform;
            newTransform.scale(Eigen::Vector3f(1.0f, scaling, 1.f)); // do non-uniform scaling.
            _meshNode3->setTransform(newTransform);
        }

        // morph mesh4 vertices
        if (animated() && _mesh4) {
            // PH_ASSERT(_mesh4Positions.size() == 8);
            std::vector<Eigen::Vector3f> newPositions = _mesh4Positions;
            constexpr auto               cycle        = std::chrono::duration_cast<std::chrono::microseconds>(10s).count();
            float                        factor       = (float) (totalMicroseconds.count() % cycle) / (float) cycle;
            const float                  handedness   = _options.leftHanded ? 1.0f : -1.0f;
            float                        offset       = ((factor < 0.5f) ? factor : (1.f - factor)) * 2.0f * handedness;
            for (auto i : _morphingIndices) { newPositions[i].z() += offset; }
            _mesh4->morph({
                .position = {VK_NULL_HANDLE, sizeof(newPositions[0])},
            });
        }
    }

private:
    const Options _options;

    sg::Node *_meshNode1 = nullptr, *_meshNode2 = nullptr, *_meshNode3 = nullptr;

    rt::Mesh *                   _mesh4 = nullptr;
    std::vector<Eigen::Vector3f> _mesh4Positions;
    std::vector<size_t>          _morphingIndices;

private:
    // void addDeformableMesh() {
    //     const float scaling  = _options.scaling; // scaling factor for X and Y
    //     const float handness = _options.leftHanded ? -1.0f : 1.0f;
    //     auto        blue     = scene->createMaterial();
    //     blue->name           = "blue";
    //     blue->setDesc(rt::Material::Desc {}.setAlbedo(0.f, 0.f, 1.f));
    //     _meshNode4 = addBox("box3", 1.f * scaling, .2f * scaling, .2f * scaling, blue, nullptr,
    //                         sg::Transform::make({0.5f * scaling, -.8f * scaling, .3f * scaling * handness}, // position
    //                                             Eigen::Quaternionf::Identity()                              // rotation
    //                                             ));
    //     for (auto c : _meshNode4->components()) {
    //         if (c->type() == NodeComponent::MODEL) {
    //             _mesh4 = &((ph::rt::Model *) c)->mesh();
    //             _mesh4Positions.clear();
    //             for (const auto & pos : _mesh4->positions()) { _mesh4Positions.push_back(ph::rt::pos); }
    //             break;
    //         }
    //     }

    //     // find all mesh4 vertices of left-back edge.
    //     for (size_t i = 0; i < _mesh4Positions.size(); ++i) {
    //         const auto & v = _mesh4Positions[i];
    //         if (v.x() < 0.f && v.z() < 0.f) { _morphingIndices.push_back(i); }
    //     }
    // }
};
