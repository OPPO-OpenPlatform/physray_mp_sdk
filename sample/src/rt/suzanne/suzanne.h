/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "../common/modelviewer.h"

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct SuzanneScene : ModelViewer {

    struct Options : ModelViewer::Options {
        std::string model;
        std::string animation;
        bool        cornellBox    = false;
        bool        floorPlane    = false;
        float       skyboxLodBias = 0.0f; ///< set to negative value to disable skybox.
        Options() {
            // Enable noise free mode by default.
            rpmode = RenderPackMode::NOISE_FREE;
        }
    };

    SuzanneScene(SimpleApp & app, const Options & o): ModelViewer(app, o), _options(o) {
        // check default model and default material
        Material * material = nullptr;
        auto       model    = std::filesystem::path(o.model);
        if (model.empty()) {
            model     = "model/suzanne/15K.obj";
            material  = world->createMaterial();
            auto desc = Material::Desc {};
            desc.setAlbedoMap(textureCache->loadFromAsset("model/suzanne/albedo-mipmapped-astc.ktx2"));
            desc.setNormalMap(textureCache->loadFromAsset("model/suzanne/normal-astc.ktx2"));
            desc.setOrmMap(textureCache->loadFromAsset("model/suzanne/orm-mipmapped-astc.ktx2"));
            material->setDesc(desc);
        } else if (std::filesystem::is_directory(model)) {
            auto gltf = searchForGLTF(model);
            if (gltf.empty()) PH_THROW("No GLTF/GLB model found in folder %s", model.string().c_str());
            model = gltf;
        }

        // load model
        scene->name = o.model;
        auto bbox   = addModelToScene({model.string(), o.animation, material});

        // create a cornell box around the model
        if (o.cornellBox) {
            Eigen::Vector3f center = bbox.center();
            Eigen::Vector3f size   = bbox.sizes() * (1.5f / 2.0f);
            float           extent = std::max(size.x(), std::max(size.y(), size.z()));
            bbox.min()             = Eigen::Vector3f(center.x(), bbox.min().y(), center.z()) - Eigen::Vector3f(extent, 0, extent);
            bbox.max()             = Eigen::Vector3f(center.x(), bbox.min().y(), center.z()) + Eigen::Vector3f(extent, extent * 2, extent);
            addCornellBoxToScene(bbox);
        } else if (o.floorPlane) {
            Eigen::Vector3f floorCenter = bbox.center();
            floorCenter.y()             = bbox.min().y();
            float floorSize             = bbox.diagonal().norm() * 1.5f;
            auto  floorBox              = addFloorPlaneToScene(floorCenter, floorSize);
            floorBox.max().y()          = bbox.max().y() * 2.f - bbox.min().y();
            bbox                        = bbox.merged(floorBox);
        }

        // setup camera
        setupDefaultCamera(bbox);

        // setup light
        if (o.cornellBox || o.floorPlane) { addCeilingLight(bbox, 2.0f, 0.1f * bbox.sizes().x()); }

        setupShadowRenderPack();

        if (o.skyboxLodBias >= 0.0f) { addSkybox(o.skyboxLodBias); }
    }

private:
    const Options _options;
};
