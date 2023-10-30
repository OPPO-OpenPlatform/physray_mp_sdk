/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/rt-utils.h>

namespace scenedebug {
struct LightDebug { // Stores pointers needed to construct and update a debug mesh visualizing a light
    ph::rt::Node *     lightMeshNode;
    ph::rt::Model *    lightModel;
    ph::rt::Material * lightMat;
    bool               enabled = false;
};

typedef std::map<const ph::rt::Light *, LightDebug> LightDebugMap;

struct SceneDebugManager {
    // Need these to create materials and meshes
    ph::rt::World * world = nullptr;
    ph::rt::Scene * scene = nullptr;

    // Unit meshes to reuse/scale
    ph::rt::Mesh * sphereMesh = nullptr;
    ph::rt::Mesh * diskMesh   = nullptr;
    ph::rt::Mesh * quadMesh   = nullptr;

    LightDebugMap lightDebugData;

    SceneDebugManager() {}
    SceneDebugManager(ph::rt::World * w, ph::rt::Scene * s, ph::rt::Mesh * sm, ph::rt::Mesh * dm, ph::rt::Mesh * qm)
        : world(w), scene(s), sphereMesh(sm), diskMesh(dm), quadMesh(qm) {}

    ph::rt::NodeTransform transformFromLight(const ph::rt::Light * light) {
        ph::rt::NodeTransform tfm;
        auto &                ld = light->desc();
        if (ld.type == ph::rt::Light::Type::POINT) {
            float scaling = ld.dimension[0];
            tfm.setScaling(Eigen::Vector3f(scaling, scaling, scaling));
        } else if ((ld.type == ph::rt::Light::Type::DIRECTIONAL) || (ld.type == ph::rt::Light::Type::SPOT)) {
            Eigen::Vector3f direction = toEigen((ld.type == ph::rt::Light::Type::DIRECTIONAL) ? ld.directional.direction : ld.spot.direction);
            Eigen::Vector2f extents   = Eigen::Vector2f(ld.dimension[0], ld.dimension[1]);
            Eigen::Vector3f nrm       = direction.normalized();
            Eigen::Vector3f tan, bit;
            float           z2 = nrm.z() * nrm.z();
            if (std::abs(nrm.x()) > std::abs(nrm.y()))
                tan = Eigen::Vector3f(-nrm.z(), 0.f, nrm.x()) / sqrt(nrm.x() * nrm.x() + z2);
            else
                tan = Eigen::Vector3f(0.f, nrm.z(), -nrm.y()) / sqrt(nrm.y() * nrm.y() + z2);
            bit                 = nrm.cross(tan);
            tfm.matrix().col(0) = tan * extents.x();
            tfm.matrix().col(1) = bit * extents.y();
            tfm.matrix().col(2) = nrm;
        }
        return tfm;
    }

    ph::rt::Node * createDebugNode(const ph::rt::Light * light, bool enabled) {
        ph::rt::NodeTransform zeroScaled;
        zeroScaled.setScaling(Eigen::Vector3f::Zero());
        ph::rt::NodeTransform tfm = enabled ? transformFromLight(light) : zeroScaled;
        // First create the node, then set the transform
        auto node = scene->createNode({.parent = light->nodes()[0]});
        node->setTransform(tfm);
        return node;
    }

    ph::rt::Material * createLightMaterial(const ph::rt::Light * light) {
        auto   mcp = ph::rt::Material::Desc {};
        auto & ld  = light->desc();
        mcp.setEmission(ld.emission[2], ld.emission[1], ld.emission[0]);
        auto mat = scene->create(light->name, mcp);
        return mat;
    }

    ph::rt::Mesh * getLightMesh(const ph::rt::Light * light) {
        switch (light->desc().type) {
        case (ph::rt::Light::Type::POINT):
            return sphereMesh;
            break;
        case (ph::rt::Light::Type::DIRECTIONAL):
            return quadMesh;
            break;
        case (ph::rt::Light::Type::SPOT):
            return diskMesh;
            break;
        case (ph::rt::Light::Type::GEOM):
            for (auto c : light->nodes()[0]->components()) {
                if (c->type() == ph::rt::NodeComponent::Type::MODEL) return &(((ph::rt::Model *) c)->mesh());
            }
            PH_LOGW("Failed to find model for geometry light");
            return nullptr;
            break;
        default:
            return nullptr;
            break;
        }
    }

    LightDebug & initLightDebug(const ph::rt::Light * light) {
        LightDebug ld = {
            .lightMeshNode = nullptr,
            .lightModel    = nullptr,
            .lightMat      = nullptr,
            .enabled       = false,
        };
        if (light->desc().type != ph::rt::Light::Type::OFF) {
            if (light->desc().type == ph::rt::Light::Type::GEOM) {
                auto            n = light->nodes()[0];
                ph::rt::Model * m = nullptr;
                for (auto c : n->components()) {
                    if (c->type() == ph::rt::NodeComponent::Type::MODEL) m = (ph::rt::Model *) c;
                }
                if (m) {
                    ld = {
                        .lightMeshNode = (ph::rt::Node *) n,
                        .lightModel    = m,
                        .lightMat      = m->subsets()[0].material,
                        .enabled       = false,
                    };
                }
            } else {
                auto node = createDebugNode(light, false);
                auto mat  = createLightMaterial(light);
                ld        = {
                    .lightMeshNode = node,
                    .lightModel    = scene->createModel({
                        .mesh     = *getLightMesh(light),
                        .material = *mat,
                    }),
                    .lightMat      = mat,
                    .enabled       = false,
                };
                node->attachComponent(ld.lightModel);
            }
        }

        lightDebugData.emplace(std::make_pair(light, ld));
        auto ddItr = lightDebugData.find(light);
        return ddItr->second;
    }

    bool * getDebugEnable(const ph::rt::Light * light) {
        auto debugDataItr = lightDebugData.find(light);
        if (debugDataItr != lightDebugData.end()) {
            return &debugDataItr->second.enabled;
        } else {
            return &initLightDebug(light).enabled;
        }
    }

    void setDebugEnable(const ph::rt::Light * light, bool enable) {
        auto toSet = getDebugEnable(light);
        *toSet     = enable;
        if (enable) updateDebugLight(light);
    }

    void onDeleteLight(const ph::rt::Light * light) {
        auto debugDataItr = lightDebugData.find(light);
        if (debugDataItr != lightDebugData.end()) {
            LightDebug & debugData = debugDataItr->second;
            for (auto c : debugData.lightMeshNode->components()) { debugData.lightMeshNode->detachComponent(c); }
            scene->destroy(debugData.lightModel);
            scene->destroy(debugData.lightMat);
            // Destroying nodes seems to cause assertions due to deleted nodes, so let
            // the scene manage its own nodes.
            // scene->deleteNodeAndSubtree(debugData.lightMeshNode);
        }
        lightDebugData.erase(light);
    }

    void updateDebugLight(const ph::rt::Light * light) {
        if ((light->desc().type == ph::rt::Light::Type::GEOM) || (light->desc().type == ph::rt::Light::Type::OFF)) return;
        auto debugDataItr = lightDebugData.find(light);
        if (debugDataItr != lightDebugData.end()) { // update
            auto debugData = debugDataItr->second;
            if (debugData.enabled) {
                // update transform
                debugData.lightMeshNode->setTransform(transformFromLight(light));

                // update material
                auto md = debugData.lightMat->desc();
                auto ld = light->desc();
                debugData.lightMat->setDesc(md.setEmission(ld.emission[0], ld.emission[1], ld.emission[2]));
                // ph::va::SingleUseCommandPool pool(vsp);
                // auto cb = pool.create();
                // scene->refreshGpuData(cb);
                // pool.finish(cb);
            } else { // disable debug mesh by scaling transform way down?
                ph::rt::NodeTransform zeroScaled;
                zeroScaled.setScaling(Eigen::Vector3f::Zero());
                debugData.lightMeshNode->setTransform(zeroScaled);
            }
        } else { // create
            initLightDebug(light);
        }
    }
};
} // namespace scenedebug