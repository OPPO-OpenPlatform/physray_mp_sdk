#pragma once
#include <ph/rt.h>

namespace scenedebug {
struct LightDebug { // Stores pointers needed to construct and update a debug mesh visualizing a light
    ph::rt::Node *     lightMeshNode;
    ph::rt::MeshView * lightMeshView;
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
        ph::rt::NodeTransform tfm = Eigen::Affine3f::Identity();
        auto &                ld  = light->desc();
        if (ld.type == ph::rt::Light::Type::POINT) {
            float scaling = ld.dimension[0];
            tfm.setScaling(Eigen::Vector3f(scaling, scaling, scaling));
        } else if ((ld.type == ph::rt::Light::Type::DIRECTIONAL) || (ld.type == ph::rt::Light::Type::SPOT)) {
            Eigen::Vector3f direction =
                (ld.type == ph::rt::Light::Type::DIRECTIONAL) ? Eigen::Vector3f(ld.directional.direction) : Eigen::Vector3f(ld.spot.direction);
            Eigen::Vector2f extents = Eigen::Vector2f(ld.dimension[0], ld.dimension[1]);
            Eigen::Vector3f nrm     = direction.normalized();
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
        ph::rt::NodeTransform zeroScaled = Eigen::Affine3f::Identity();
        zeroScaled.setScaling(Eigen::Vector3f::Zero());
        ph::rt::NodeTransform tfm = enabled ? transformFromLight(light) : zeroScaled;
        return scene->addNode({
            .parent    = &light->node(),
            .transform = tfm,
        });
    }

    ph::rt::Material * createLightMaterial(const ph::rt::Light * light) {
        auto   mcp = ph::rt::World::MaterialCreateParameters {};
        auto & ld  = light->desc();
        mcp.setEmission(ld.emission[2], ld.emission[1], ld.emission[0]);
        return world->create(light->name.c_str(), mcp);
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
        default:
            return nullptr;
            break;
        }
    }

    LightDebug & initLightDebug(const ph::rt::Light * light) {
        auto       node = createDebugNode(light, false);
        auto       mat  = createLightMaterial(light);
        LightDebug ld   = {
            .lightMeshNode = node,
            .lightMeshView = scene->addMeshView({
                .node     = node,
                .mesh     = getLightMesh(light),
                .material = mat,
            }),
            .lightMat      = mat,
            .enabled       = false,
        };
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

    void updateDebugLight(const ph::rt::Light * light) {
        auto debugDataItr = lightDebugData.find(light);
        if (debugDataItr != lightDebugData.end()) { // update
            auto debugData = debugDataItr->second;
            if (debugData.enabled) {
                // update transform
                debugData.lightMeshNode->setTransform(transformFromLight(light));

                // update material
                auto md = debugData.lightMat->desc();
                auto ld = light->desc();
                scene->debug_updateMaterial(debugData.lightMat, md.setEmission(ld.emission[0], ld.emission[1], ld.emission[2]));
            } else { // disable debug mesh by scaling transform way down?
                ph::rt::NodeTransform zeroScaled = Eigen::Affine3f::Identity();
                zeroScaled.setScaling(Eigen::Vector3f::Zero());
                debugData.lightMeshNode->setTransform(zeroScaled);
            }
        } else { // create
            initLightDebug(light);
        }
    }
};
} // namespace scenedebug