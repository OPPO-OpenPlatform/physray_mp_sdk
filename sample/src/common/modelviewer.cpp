#include "pch.h"

#include "modelviewer.h"
#include "animations/timeline.h"
#include "gltf-scene-reader.h"
#include "sphere.h"

#include <filesystem>
#include <limits>
#include <locale>

// TODO: Better programming model for adding new keypresses/custom buttons
// for scene-specific keypress handling
#if !defined(__ANDROID__)
    #include <GLFW/glfw3.h>
#endif

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

// ---------------------------------------------------------------------------------------------------------------------
//
ModelViewer::ModelViewer(SimpleApp & app, const Options & o)
    : SimpleScene({
          .app                    = app,
          .animated               = o.animated,
          .showUI                 = o.showUI,
          .showFrameTimeBreakdown = o.showFrameTimes,
      }),
      options(o), skinningManager(skinning::SkinningManager(o.skinMode)),
      ptConfig(PathTracerConfig(o.rpmode == ph::rt::World::RayTracingRenderPackCreateParameters::Mode::PATH_TRACING)),
      morphTargetManager(MorphTargetManager()) {
    targetMode = o.rpmode;

    // create asset system
    auto ascp = AssetSystem::CreateParameters {
        {
            ASSET_FOLDER,
            getExecutableFolder() + "/asset",
            getExecutableFolder() + "/../../src/asset",
        },
        1024,
    };
    for (auto f : o.additionalAssetFolders) {
        ascp.roots.push_back(f);
    }
    assetSys = AssetSystem::create(ascp);

    // Initialize texture cache so that images can be reused.
    textureCache.reset(new TextureCache(&dev().graphicsQ(), assetSys));

    // create RT world instance
    world = World::createWorld({dev().graphicsQ(),
                                {ASSET_FOLDER},
                                &cpuFrameTimes,
                                true, // GPU timestamps.
                                app.cp().rayQuery ? World::WorldCreateParameters::KHR_RAY_QUERY
                                : app.cp().gpuBvh ? World::WorldCreateParameters::AABB_GPU
                                                  : World::WorldCreateParameters::AABB_CPU});

    // create a new scene
    scene = world->createScene({});

    auto sphereMesh = createIcosahedron(1.0f, 2);
    auto circleMesh = createCircle(1.0f, 1.0f);
    auto quadMesh   = createQuad(1.0f, 1.0f);
    debugManager    = scenedebug::SceneDebugManager(world, scene, sphereMesh, circleMesh, quadMesh);
    // create default materials
    auto mcd   = rt::World::MaterialCreateParameters {};
    lambertian = world->create("lambertian", mcd);
    mcd.setRoughness(0.5f);
    glossy = world->create("glossy", mcd);

    // setup default record parameters
    recordParameters.shadowMode   = options.shadowMode;
    recordParameters.ambientLight = {.01f, .01f, .01f};

    // Creates the node and the default camera.
    _firstPersonNode = scene->addNode({});
    float w          = (float) sw().initParameters().width;
    float h          = (float) sw().initParameters().height;
    cameras.push_back(scene->addCamera({.node = _firstPersonNode,
                                        .desc {
                                            .yFieldOfView = h / std::min<float>(w, h), // roughly 60 degree vertical FOV.
                                            .handness     = Camera::RIGHT_HANDED,
                                            .zNear        = 1.0f,
                                            .zFar         = 3.0f,
                                        }}));
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::resize() {
    SimpleScene::resize();
    auto newW = cp().app.sw().initParameters().width;
    auto newH = cp().app.sw().initParameters().height;
    if (!pathRayTracingRenderPack || newW != _renderTargetSize.width || newH != _renderTargetSize.height) {
        recreateMainRenderPack();
        _renderTargetSize.width  = newW;
        _renderTargetSize.height = newH;
    }
    if (skybox) skybox->resize(newW, newH);
    PH_LOGI("[ModelViewer] resized to %ux%u", newW, newH);
}

// ---------------------------------------------------------------------------------------------------------------------
//
ph::rt::Node * ModelViewer::addMeshNode(ph::rt::Node * parent, const ph::rt::NodeTransform & transform, ph::rt::Mesh * mesh, ph::rt::Material * material) {
    // Create the node that will manage the transforms.
    ph::rt::Node * node = scene->addNode({.parent = parent, .transform = transform});

    node->name = mesh->name;

    // Create the object that will display the mesh.
    scene->addMeshView({
        .node     = node,
        .mesh     = mesh,
        .material = material,
    });

    return node;
}

// ---------------------------------------------------------------------------------------------------------------------
// Init the skybox, will be rendered as part of the the main pass.
void ModelViewer::addSkybox(float lodBias) {
    if (!mainColorPass()) {
        PH_THROW("Color pass is not created yet. Are you try calling addSkybox() in you scene's constructor?"
                 "Since skybox depends on swapchain, the best place to call it is inside the resize() method.");
    }

    auto irradiance = textureCache->loadFromAsset(options.irradianceMapAsset);
    auto reflection = textureCache->loadFromAsset(options.reflectionMapAsset);
    if (irradiance && reflection) {
        // Need to update environment map parameters too.
        recordParameters.irradianceMap = irradiance;
        recordParameters.reflectionMap = reflection;
        recordParameters.ambientLight  = Eigen::Vector3f(0.f, 0.f, 0.f);
    } else {
        recordParameters.irradianceMap = {};
        recordParameters.reflectionMap = {};
        recordParameters.ambientLight  = Eigen::Vector3f(0.2f, 0.2f, 0.2f);
    }

    Skybox::ConstructParameters cp = {dev().graphicsQ(), *assetSys};
    cp.width                       = sw().initParameters().width;
    cp.height                      = sw().initParameters().height;
    cp.pass                        = mainColorPass();
    cp.skymap                      = reflection;
    cp.skymapType                  = Skybox::SkyMapType::CUBE;
    skybox.reset(new Skybox(cp));
    skyboxLodBias = lodBias;
}

// ---------------------------------------------------------------------------------------------------------------------
//

void ModelViewer::setupDefaultCamera(const Eigen::AlignedBox3f & bbox) {
    // Calculate where we should be putting the camera.
    Eigen::Vector3f sceneCenter = bbox.center();
    float           sceneExtent = bbox.diagonal().norm();

    // Create the default camera.
    auto desc     = cameras[0]->desc();
    desc.handness = options.leftHanded ? Camera::LEFT_HANDED : Camera::RIGHT_HANDED;
    desc.zNear    = sceneExtent / 100.0f;
    desc.zFar     = sceneExtent * 4.0f;
    cameras[0]->reset(desc);
    defaultZFar = desc.zFar;

    firstPersonController.setHandness(desc.handness)
        .setMinimalOrbitalRadius(sceneExtent / 100.0f)
        .setMouseMoveSensitivity(sceneExtent / 1000.0f)
        .setMoveSpeed(Eigen::Vector3f::Constant(sceneExtent / 3.0f));

    if (options.flythroughCamera) {
        auto camPos = Eigen::Vector3f(sceneCenter.x(), sceneCenter.y(),
                                      // camera's initial Z coordinate is dpending on handness.
                                      sceneCenter.z() + sceneExtent * (options.leftHanded ? -1.0f : 1.0f));
        firstPersonController.setOrbitalCenter(nullptr).setPosition(camPos);
    } else {
        firstPersonController.setOrbitalCenter(&sceneCenter).setAngle({0.f, 0.f, 0.f}).setOrbitalRadius(sceneExtent);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
ph::rt::Light * ModelViewer::addPointLight(const Eigen::Vector3f & position, float range, const Eigen::Vector3f & emission, float radius,
                                           bool enableDebugMesh) {
    // Update light node transform.
    ph::rt::NodeTransform transform = ph::rt::NodeTransform::Identity();
    transform.translate(position);

    std::string name = formatstr("Point Light %zu", lights.size());

    // Create the light and the node.
    auto node   = scene->addNode({
        .transform = transform,
    });
    node->name  = name;
    auto light  = scene->addLight({.node = node,
                                  .desc {.type      = ph::rt::Light::Type::POINT,
                                         .dimension = {radius, 0.f},
                                         .emission  = {emission.x(), emission.y(), emission.z()},
                                         .point {.range = range}}});
    light->name = name;

    // Give the light a backing shadow map.
    light->shadowMap = textureCache->createShadowMapCube(name.c_str());

    // Save to the list of lights.
    lights.push_back(light);

    debugManager.setDebugEnable(light, enableDebugMesh);

    return light;
}

ph::rt::Light * ModelViewer::addSpotLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float range, float brightness,
                                          Eigen::Vector2f cones, Eigen::Vector2f dimensions, bool enableDebugMesh) {
    return addSpotLight(position, direction, range, Eigen::Vector3f(brightness, brightness, brightness), cones, dimensions, enableDebugMesh);
}

ph::rt::Light * ModelViewer::addSpotLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, float range, const Eigen::Vector3f & emission,
                                          Eigen::Vector2f cones, Eigen::Vector2f dimensions, bool enableDebugMesh) {
    // Up is +Y, forward is +Z
    // Update light node transform.
    ph::rt::NodeTransform transform = ph::rt::NodeTransform::Identity();
    transform.translate(position);

    // Create the light and the node.
    auto node = scene->addNode({
        .transform = transform,
    });

    if (cones.x() > HALF_PI) {
        PH_LOGI("Outer radius of a spot light cannot exceed PI/2 or 90 degrees.\n");
        cones.x() = HALF_PI;
    }

    if (cones.y() > cones.x()) {
        PH_LOGI("Inner radius of a spot light cannot exceed its outer radius.\n");
        cones.y() = cones.x();
    }

    auto light = scene->addLight({.node = node,
                                  .desc {.type      = ph::rt::Light::Type::SPOT,
                                         .dimension = {dimensions.x(), dimensions.y()},
                                         .emission  = {emission.x(), emission.y(), emission.z()},
                                         .spot {
                                             .direction = {direction.x(), direction.y(), direction.z()},
                                             .inner     = cones.y(),
                                             .outer     = cones.x(),
                                             .range     = range,
                                         }}});

    // TODO: Not tested with spot lights yet.
    light->shadowMap = textureCache->createShadowMap2D("spot light");
    lights.push_back(light);

    debugManager.setDebugEnable(light, enableDebugMesh);

    return light;
}

ph::rt::Light * ModelViewer::addDirectionalLight(const Eigen::Vector3f & position, const Eigen::Vector3f & dir, float brightness,
                                                 const Eigen::Vector2f * dimensions, bool enableDebugMesh) {
    return addDirectionalLight(position, dir, Eigen::Vector3f(brightness, brightness, brightness), dimensions, enableDebugMesh);
}
ph::rt::Light * ModelViewer::addDirectionalLight(const Eigen::Vector3f & position, const Eigen::Vector3f & direction, const Eigen::Vector3f & emission,
                                                 const Eigen::Vector2f * dimensions, bool enableDebugMesh) {
    // No need to manually set up transform matrix:
    // direction is normalized and composed with transform when populating light uniforms.
    ph::rt::NodeTransform transform = ph::rt::NodeTransform::Identity();
    transform.translate(position);
    auto            node = scene->addNode({
        .transform = transform,
    });
    Eigen::Vector2f dims = Eigen::Vector2f::Zero();
    if (dimensions) dims = *dimensions;

    auto light = scene->addLight({.node = node,
                                  .desc {.type      = ph::rt::Light::Type::DIRECTIONAL,
                                         .dimension = {dims.x(), dims.y()},
                                         .emission  = {emission.x(), emission.y(), emission.z()},
                                         .directional {
                                             .direction = {direction.x(), direction.y(), direction.z()},
                                         }}});

    // TODO haven't set up directional light shadow map rendering
    light->shadowMap = textureCache->createShadowMap2D("directional light");

    lights.push_back(light);

    debugManager.setDebugEnable(light, enableDebugMesh);

    return light;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::addCeilingLight(const Eigen::AlignedBox3f & bbox, float emission, float radius, bool enableDebugMesh) {
    Eigen::Vector3f position(bbox.center().x(), bbox.max().y() * 0.8f, bbox.center().z());
    float           range = bbox.diagonal().norm() * 5.0f;
    addPointLight(position, range, Eigen::Vector3f(emission, emission, emission), radius, enableDebugMesh);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::recreateMainRenderPack() {
    threadSafeDeviceWaitIdle(dev().vgi().device);
    world->deleteRayTracingRenderPack(pathRayTracingRenderPack);
    PH_ASSERT(!pathRayTracingRenderPack);
    auto w  = sw().initParameters().width;
    auto h  = sw().initParameters().height;
    auto cp = World::RayTracingRenderPackCreateParameters {options.rpmode}
                  .setTarget(sw().initParameters().colorFormat, w, h, VK_IMAGE_LAYOUT_UNDEFINED)
                  .setViewport(0.f, 0.f, (float) w, (float) h)
                  .setClear(true)
                  .setTracing(options.spp, options.maxSpp, options.accum);
    cp.targetIsSRGB                     = true;
    pathRayTracingRenderPack            = world->createRayTracingRenderPack(cp);
    recordParameters.maxDiffuseBounces  = options.diffBounces;
    recordParameters.maxSpecularBounces = options.specBounces;
    ptConfig.setupRp(recordParameters);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::setupShadowRenderPack() {
    world->deleteShadowMapRenderPack(shadowRenderPack);
    shadowRenderPack =
        world->createShadowMapRenderPack(World::ShadowMapRenderPackCreateParameters {}.set(shadowMapSize, shadowMapFormat, VK_IMAGE_LAYOUT_UNDEFINED));
}

// ---------------------------------------------------------------------------------------------------------------------
//
const SimpleScene::FrameTiming & ModelViewer::update() {
    if (options.rpmode != targetMode) {
        options.rpmode = targetMode;
        recreateMainRenderPack();
    }

    // Get the new frame time.
    auto & frameTiming = SimpleScene::update();

    // Records how much time has passed since last frame.
    // If fixed frame rate is set, this will be set to a consistent value.
    std::chrono::microseconds elapsedMicroseconds;

    // If min frame rate is a positive number.
    if (options.minFrameRate > 0) {
        // Calculate the maximum amount of time that is allowed to elapse each frame from the min frame rate.
        std::chrono::microseconds maxTimeElapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(1.0f / options.minFrameRate));

        // Make sure elapsed time is not bigger than the min frame rate.
        elapsedMicroseconds = std::min(frameTiming.sinceLastUpdate, maxTimeElapsed);

        // If min frame rate is zero or less.
    } else {
        // Then there is no max time. Just use the actual time elapsed.
        elapsedMicroseconds = frameTiming.sinceLastUpdate;
    }

    // If we have a max frame rate.
    if (std::isfinite(options.maxFrameRate)) {
        // Calculate the minimum amount of time that needs to elapse from the max frame rate.
        std::chrono::microseconds minTimeElapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(1.0f / options.maxFrameRate));

        // Make sure elapsed time is not smaller than max frame rate.
        elapsedMicroseconds = std::max(minTimeElapsed, elapsedMicroseconds);
    }

    // Update first person controller and node, only when the first person camera is selected.
    if (0 == selectedCameraIndex) {
        // Update the camera controller.
        firstPersonController.update((float) elapsedMicroseconds.count() / 1000000.0f);

        // Update the first person camera to the position of the camera controller.
        _firstPersonNode->setWorldTransform(firstPersonController.getWorldTransform());

        // automatically adjust znear and zfar
        if (firstPersonController.orbiting()) {
            auto                 r    = firstPersonController.orbitalRadius();
            ph::rt::Camera::Desc desc = cameras[0]->desc();
            desc.zNear                = std::max(r / 100.f, defaultZFar / 1000.0f); // TODO: should be r - scene size;
            desc.zFar                 = std::max(r * 2.f, defaultZFar);             // TODO: should be r + scene size
            cameras[0]->reset(desc);
        }
    }

    // TODO: update recordParameters.camera.zFar based on camera's distance to
    // the scene center to avoid clipping when camera is away from the scene.

    // Update the animations.
    if (animated()) {
        for (std::size_t index = 0; index < animations.size(); ++index) {
            animations[index]->tick(elapsedMicroseconds);
        }

        skinningManager.update(true);
        morphTargetManager.update(true);
    }

    return frameTiming;
}

void ModelViewer::prepare(VkCommandBuffer cb) {
    if (!scene) return;
    ph::rt::RayTracingRenderPack::RecordParameters rp = recordParameters;
    rp.scene                                          = scene;
    rp.commandBuffer                                  = cb;
    scene->prepareForRecording(cb);
    pathRayTracingRenderPack->prepareForRecording(rp);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::recordOffscreenPass(const PassParameters & p) {
    if (!scene) return;

    scene->prepareForRecording(p.cb);

    if (shadowRenderPack && (animated() || loop().frameCounter() == 0)) {
        for (auto & l : lights) {
            if (Light::Type::OFF == l->desc().type) continue;

            shadowParameters.commandBuffer = p.cb;
            shadowParameters.light         = l;
            shadowRenderPack->record(shadowParameters);

            // transfer shadow map layout from writing to reading
            va::setImageLayout(p.cb, l->shadowMap.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VkImageSubresourceRange {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
    }

    recordParameters.scene         = scene;
    recordParameters.commandBuffer = p.cb;
    recordParameters.targetView    = p.bb.view;
    recordParameters.depthView     = p.depthView;
    recordParameters.targetImage   = p.bb.image;
    recordParameters.camera        = cameras[selectedCameraIndex];
    ptConfig.setupRp(recordParameters);
    // All backbuffers must be re-rendered before time accumulation can complete
    recordParameters.timeAccumDone = pathRayTracingRenderPack->accumulationProgress(sw().initParameters().count, pauseTime()) >= 1.f;
    pathRayTracingRenderPack->record(recordParameters);
}

void ModelViewer::onKeyPress(int key, bool down) {
    auto & io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

#if !defined(__ANDROID__)
    // Update the first person controller.
    auto k = FirstPersonController::INVALID_KEY;
    switch (key) {
    case GLFW_KEY_A:
        k = FirstPersonController::MOVE_L;
        break;
    case GLFW_KEY_W:
        k = FirstPersonController::MOVE_F;
        break;
    case GLFW_KEY_S:
        k = FirstPersonController::MOVE_B;
        break;
    case GLFW_KEY_D:
        k = FirstPersonController::MOVE_R;
        break;
    case GLFW_KEY_LEFT:
        k = FirstPersonController::TURN_L;
        break;
    case GLFW_KEY_RIGHT:
        k = FirstPersonController::TURN_R;
        break;
    case GLFW_KEY_UP:
        k = FirstPersonController::TURN_D;
        break;
    case GLFW_KEY_DOWN:
        k = FirstPersonController::TURN_U;
        break;
    case GLFW_KEY_PAGE_UP:
        k = FirstPersonController::MOVE_U;
        break;
    case GLFW_KEY_PAGE_DOWN:
        k = FirstPersonController::MOVE_D;
        break;
    case GLFW_MOUSE_BUTTON_1:
        k = FirstPersonController::LOOK;
        break;
    case GLFW_MOUSE_BUTTON_2:
        k = FirstPersonController::PAN;
        break;
    };
    firstPersonController.onKeyPress(k, down);

    // Update scene controls.
    if (!down) {
        switch (key) {
        case GLFW_KEY_O:
            toggleShadowMode();
            break;
        case GLFW_KEY_SPACE:
            toggleAnimated();
            break;
        case GLFW_KEY_C:
            togglePrimaryCamera();
            break;
        };
    }
#else
    // Use inputs to avoid warning/errors
    key = (int) down;
#endif
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::onMouseMove(float x, float y) {
    auto & io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    firstPersonController.onMouseMove(x, y);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::onMouseWheel(float delta) {
    auto & io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    firstPersonController.onMouseWheel(delta);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::recordMainColorPass(const PassParameters & p) {
    if (skybox && (options.rpmode != ph::rt::World::RayTracingRenderPackCreateParameters::Mode::PATH_TRACING)) {
        // The camera's transformation matrix.
        auto            camera = cameras[selectedCameraIndex];
        Eigen::Matrix4f proj   = camera->calculateProj(((float) sw().initParameters().width) / ((float) sw().initParameters().height));
        // draw skybox as part of the main render pass
        skybox->draw(p.cb, proj, camera->node().worldTransform().rotation(), recordParameters.ambientLight, skyboxLodBias);
    }

    if (options.maxFrames > 0 && (loop().frameCounter() + 1) >= options.maxFrames) { loop().requestForQuit(); }
}

// ---------------------------------------------------------------------------------------------------------------------
//
Eigen::AlignedBox3f ModelViewer::addModelToScene(const LoadOptions & o) {
    Eigen::AlignedBox3f bbox;
    addModelNodeToScene(o, bbox);
    return bbox;
}

ph::rt::Node * ModelViewer::addModelNodeToScene(const LoadOptions & o, Eigen::AlignedBox3f & bbox) {
    std::filesystem::path modelPath = o.model;
    auto                  ext       = modelPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](auto c) { return (char) std::tolower(c); });

    // Pick a loader depending on the file extension.
    // If this is an OBJ file.
    if (ext == ".obj") {
        return loadObj(o, bbox);

        // If this is a GLTF file
    } else if (ext == ".gltf" || ext == ".glb") {
        // Use the tiny gltf parser.
        auto scene = loadGltf(o);
        bbox       = scene->getBounds();
        return scene->getNodes()[0]; // FIXME how do we get the parent node of a scene?

        // If this is any other type of model.
    } else {
        PH_LOGE("Unsupported file format: %s", modelPath.string().c_str());
        return nullptr;
    }
}

Eigen::AlignedBox3f ModelViewer::transformBbox(Eigen::AlignedBox3f bbox, ph::rt::NodeTransform t) {
    Eigen::AlignedBox3f transformedBbox;

    // Graphics Gems 1990 Arvo method for quickly transforming a bbox.
    // Leaves wasted space relative to the actual bounds of the transformed mesh,
    // but since modelviewer only uses bboxes to create other geometry it should be fine.

    // FIXME: It looks like scaling isn't applied to the bbox correctly.
    Eigen::Vector3f    translate;
    Eigen::Vector3f    scale;
    Eigen::Quaternionf rotate;
    Eigen::Matrix3f    rotationMatrix;
    t.decompose(&translate, &rotate, &scale);

    rotationMatrix = rotate;
    rotationMatrix(0, 0) *= scale.x();
    rotationMatrix(1, 1) *= scale.y();
    rotationMatrix(2, 2) *= scale.z();

    // Update min/max
    for (int i = 0; i < 3; i++) {
        transformedBbox.min()[i] = translate[i];
        transformedBbox.max()[i] = translate[i];
        for (int j = 0; j < 3; j++) {
            float e = rotationMatrix(i, j) * bbox.min()[j];
            float f = rotationMatrix(i, j) * bbox.max()[j];
            if (e < f) {
                transformedBbox.min()[i] += e;
                transformedBbox.max()[i] += f;
            } else {
                transformedBbox.min()[i] += f;
                transformedBbox.max()[i] += e;
            }
        }
    }

    return transformedBbox;
}

// ---------------------------------------------------------------------------------------------------------------------
//
ph::rt::Node * ModelViewer::loadObj(const LoadOptions & o, Eigen::AlignedBox3f & bbox) {
    // load asset into memory
    auto asset = assetSys->load(o.model.c_str()).get();
    PH_REQUIRE(!asset.content.empty());

    // load the obj mesh
    std::istringstream iss(std::string((const char *) asset.content.data(), asset.content.size()));
    auto               mesh = FatMesh::loadObj(iss);
    bbox                    = mesh.bbox;
    if (mesh.empty()) { PH_THROW("failed to load obj mesh: %s", o.model.c_str()); }

    // setup default material
    auto material = o.defaultMaterial ? o.defaultMaterial : glossy;

    // create PhysRay mesh and add it to the scene
    auto jmesh  = world->createMesh({
        mesh.position.size(),
        {(const float *) mesh.position.data(), sizeof(float) * 3},
        {(const float *) mesh.normal.data(), sizeof(float) * 3},
        {(const float *) mesh.texcoord.data(), sizeof(float) * 2},
        {(const float *) mesh.tangent.data(), sizeof(float) * 3},
    });
    jmesh->name = o.model;
    auto jnode  = addMeshNode(o.parent, ph::rt::NodeTransform::Identity(), jmesh, material);

    PH_LOGI(".OBJ mesh loaded with %zu vertices", mesh.position.size());

    return jnode;
}

// ---------------------------------------------------------------------------------------------------------------------
//
std::shared_ptr<const SceneAsset> ModelViewer::loadGltf(const LoadOptions & o) {
    // load GLTF scene
    GLTFSceneReader                   sceneReader(assetSys, textureCache.get(), world, scene, skinningManager.getSkinDataMap(), &morphTargetManager);
    std::shared_ptr<const SceneAsset> sceneAsset = sceneReader.read(o.model);

    // Add contents to the scene.
    loadSceneAsset(o, sceneAsset.get());

    return sceneAsset;
}

void ModelViewer::loadSceneAsset(const LoadOptions & o, const SceneAsset * sceneAsset) {
    // Record all the lights in this model.
    lights.insert(lights.end(), sceneAsset->getLights().begin(), sceneAsset->getLights().end());

    // Record all the cameras in this model.
    cameras.insert(cameras.end(), sceneAsset->getCameras().begin(), sceneAsset->getCameras().end());

    addModelAnimations(o, sceneAsset);
}

void ModelViewer::addModelAnimations(const LoadOptions & o, const SceneAsset * sceneAsset) {
    // Get the name mapping of all animations.
    auto nameToAnimations = sceneAsset->getNameToAnimations();

    // Print the names of all named animations.
    for (auto & keyPair : nameToAnimations) {
        PH_LOGI("Model contains animation '%s'", keyPair.first.c_str());
    }

    // Select the animations we want to use.
    // If user selected ALL animations.
    if (o.animation == "*") {
        // Add all animations.
        auto & modelAnimations = sceneAsset->getAnimations();
        animations.insert(animations.end(), modelAnimations.begin(), modelAnimations.end());

        // If user didn't select all animations.
    } else {
        // Records whether we've managed to add any animations.
        bool animationAdded = false;

        // Fetch the set of nameless animations.
        auto namelessIterator = nameToAnimations.find("");

        // If this has nameless animations.
        if (namelessIterator != nameToAnimations.end()) {
            // Nameless animations are selected automatically.
            // Add them to the list of animations to use.
            animations.insert(animations.end(), namelessIterator->second.begin(), namelessIterator->second.end());

            // Record that we added an animation.
            animationAdded = true;
        }

        // If user selected a named animation.
        if (o.animation != "") {
            // Select animations named via options.
            auto namedIterator = nameToAnimations.find(o.animation);

            // If the selected animations exist.
            if (namedIterator != nameToAnimations.end()) {
                // Add them to the list of animations to use.
                animations.insert(animations.end(), namedIterator->second.begin(), namedIterator->second.end());

                // Record that we added an animation.
                animationAdded = true;

                // If the selected animations do not exist.
            } else {
                // Warn the user.
                PH_LOGW("Animation '%s' does not exist.", o.animation.c_str());
            }
        }

        // If we weren't able to add any animations
        // but model does HAVE animations.
        if (!animationAdded && nameToAnimations.size() > 0) {
            // Add the first set of animations in the model.
            auto   beginIterator = nameToAnimations.begin();
            auto & animationSet  = beginIterator->second;
            animations.insert(animations.end(), animationSet.begin(), animationSet.end());

            // Tell user we are playing it.
            PH_LOGI("Playing animation '%s'", beginIterator->first.c_str());
        }
    }

    // All animations are played perpetually and therefore have their repeat count set to indefinite.
    for (auto & timeline : animations) {
        timeline->setRepeatCount(::animations::Timeline::REPEAT_COUNT_INDEFINITE);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::addCornellBoxToScene(const Eigen::AlignedBox3f & bbox) {
    // create box walls
    const float           l   = bbox.min().x();                                         // left
    const float           r   = bbox.max().x();                                         // right
    const float           f   = (options.leftHanded ? bbox.min().z() : bbox.max().z()); // front
    const float           k   = (options.leftHanded ? bbox.max().z() : bbox.min().z()); // back
    const float           t   = bbox.max().y();                                         // top
    const float           b   = bbox.min().y();                                         // bottom
    const Eigen::Vector3f v[] = {

        //     7-------6
        //   / |     / |
        //  3-------2  |
        //  |  |    |  |
        //  |  4----|--5
        //  | /     | /
        //  0-------1

        {l, b, f}, // 0
        {r, b, f}, // 1
        {r, t, f}, // 2
        {l, t, f}, // 3
        {l, b, k}, // 4
        {r, b, k}, // 5
        {r, t, k}, // 6
        {l, t, k}, // 7
    };
    const Eigen::Vector3f x(1.f, 0.f, 0.f);
    const Eigen::Vector3f y(0.f, 1.f, 0.f);
    const Eigen::Vector3f z(0.f, 0.f, 1.f);

    const Eigen::Vector3f wallPositions[] = {
        v[4], v[5], v[6], v[7], // back
        v[2], v[3], v[7], v[6], // top
        v[0], v[1], v[5], v[4], // bottom
        v[0], v[4], v[7], v[3], // left
        v[5], v[1], v[2], v[6], // right
    };
    float                 hmf           = (options.leftHanded ? -1.f : 1.f); // handedness multiplication factor
    const Eigen::Vector3f wallNormals[] = {
        z * hmf, z * hmf, z * hmf, z * hmf, // back
        -y,      -y,      -y,      -y,      // top
        y,       y,       y,       y,       // bottom
        x,       x,       x,       x,       // left
        -x,      -x,      -x,      -x,      // right
    };
    auto addWall = [&](uint32_t wall, Material * m) {
        const uint32_t indices[] = {wall * 4 + 0, wall * 4 + 1, wall * 4 + 2, wall * 4 + 0, wall * 4 + 2, wall * 4 + 3};

        std::vector<Eigen::Vector3f> vertices, normals;
        for (auto i : indices) {
            vertices.push_back(wallPositions[i]);
            normals.push_back(wallNormals[i]);
        }
        auto mesh  = world->createMesh({
            std::size(indices),
            {(float *) vertices.data(), sizeof(float) * 3},
            {(float *) normals.data(), sizeof(float) * 3},
        });
        mesh->name = formatstr("Cornell Box - wall #%d", wall);
        addMeshNode(nullptr, rt::NodeTransform::Identity(), mesh, m);
    };
    auto baseDesc = []() { return rt::World::MaterialCreateParameters {}; };
    auto white    = world->create("white", baseDesc());
    auto red      = world->create("red", baseDesc().setAlbedo(1.f, 0.f, 0.f));
    auto green    = world->create("green", baseDesc().setAlbedo(0.f, 1.f, 0.f));
    addWall(0, white); // back
    addWall(1, white); // top
    addWall(2, white); // bottom
    addWall(3, red);   // left
    addWall(4, green); // right
}

// ---------------------------------------------------------------------------------------------------------------------
//
Eigen::AlignedBox3f ModelViewer::addFloorPlaneToScene(const Eigen::Vector3f & center, float dimension) {
    const float h      = dimension / 2.0f;
    const float l      = center.x() - h; // left
    const float r      = center.x() + h; // right
    const float f      = center.z() + h; // front
    const float k      = center.z() - h; // back
    const float b      = center.y();     // bottom
    const float v[][3] = {
        {l, b, f},
        {r, b, f},
        {r, b, k},
        {l, b, k},
    };

    std::vector<float> vertices;
    vertices.push_back(v[0][0]);
    vertices.push_back(v[0][1]);
    vertices.push_back(v[0][2]);
    vertices.push_back(v[1][0]);
    vertices.push_back(v[1][1]);
    vertices.push_back(v[1][2]);
    vertices.push_back(v[2][0]);
    vertices.push_back(v[2][1]);
    vertices.push_back(v[2][2]);
    vertices.push_back(v[0][0]);
    vertices.push_back(v[0][1]);
    vertices.push_back(v[0][2]);
    vertices.push_back(v[2][0]);
    vertices.push_back(v[2][1]);
    vertices.push_back(v[2][2]);
    vertices.push_back(v[3][0]);
    vertices.push_back(v[3][1]);
    vertices.push_back(v[3][2]);

    std::vector<float> normals;
    for (size_t i = 0; i < 6; ++i) {
        normals.push_back(0.f);
        normals.push_back(1.f);
        normals.push_back(0.f);
    }

    auto mesh  = world->createMesh({
        .count     = vertices.size() / 3,
        .positions = {vertices.data(), sizeof(float) * 3},
        .normals   = {normals.data(), sizeof(float) * 3},
    });
    mesh->name = "floor";
    addMeshNode(nullptr, ph::rt::NodeTransform::Identity(), mesh, lambertian);

    return {Eigen::Vector3f(v[0]), Eigen::Vector3f(v[2])};
}

// ---------------------------------------------------------------------------------------------------------------------
//
rt::Mesh * ModelViewer::createIcosahedron(float radius, uint32_t subdivide) {
    // when we generate a sphere with many faces, make the normal smooth.
    bool smoothNormal = subdivide > 0;

    // generate sphere vertices
    auto vertices = buildIcosahedronUnitSphere(subdivide);

    // deal with handness
    if (options.leftHanded) {
        for (auto & v : vertices) {
            v.z() = -v.z();
        }
    }

    // generate normal. also scale the ball to proper size.
    std::vector<Eigen::Vector3f> normals(vertices.size());
    for (uint32_t i = 0; i < vertices.size(); i += 3) {
        auto & v0 = vertices[i];
        auto & v1 = vertices[i + 1];
        auto & v2 = vertices[i + 2];
        if (smoothNormal) {
            normals[i]     = v0;
            normals[i + 1] = v1;
            normals[i + 2] = v2;
        } else {
            auto n         = ((v0 + v1 + v2) / 3.0f).normalized();
            normals[i]     = n;
            normals[i + 1] = n;
            normals[i + 2] = n;
        }
        v0 *= radius;
        v1 *= radius;
        v2 *= radius;
    }
    PH_ASSERT(vertices.size() == normals.size());

    // now create the mesh
    auto mesh = world->createMesh({
        .count     = vertices.size(),
        .positions = {vertices[0].data(), sizeof(float) * 3},
        .normals   = {normals[0].data(), sizeof(float) * 3},
    });
    return mesh;
}

rt::Node * ModelViewer::addIcosahedron(const char * name, float radius, uint32_t subdivide, rt::Material * material, rt::Node * parent,
                                       const rt::NodeTransform & transform) {
    auto mesh = createIcosahedron(radius, subdivide);
    if (name) mesh->name = name;

    // add to scene
    return addMeshNode(parent, transform, mesh, material);
}

// ---------------------------------------------------------------------------------------------------------------------
//
rt::Mesh * ModelViewer::createQuad(float w, float h) {
    // To match addSpotLight, object-space orientation of quad points its normal in the +z direction.
    // With RH rendering, this requires using vert order (0, 1, 2), (0, 2, 3)
    // With LH rendering, this requires using vert order (0, 2, 1), (0, 3, 2)
    //  3-------2
    //  |       |
    //  |       |
    //  |       |
    //  0-------1
    const float z      = options.leftHanded ? -1.0f : 1.0f;
    const float l      = -w / 2.0f;
    const float r      = w / 2.0f;
    const float t      = h / 2.0f;
    const float b      = -h / 2.0f;
    const float v[][3] = {
        {l, b, 0.0f}, // 0
        {r, b, 0.0f}, // 1
        {r, t, 0.0f}, // 2
        {l, t, 0.0f}, // 3
    };

    std::vector<Eigen::Vector3f> vertices;
    std::vector<Eigen::Vector3f> normals;

    vertices.emplace_back(v[0]);
    vertices.emplace_back(options.leftHanded ? v[2] : v[1]);
    vertices.emplace_back(options.leftHanded ? v[1] : v[2]);
    vertices.emplace_back(v[0]);
    vertices.emplace_back(options.leftHanded ? v[3] : v[2]);
    vertices.emplace_back(options.leftHanded ? v[2] : v[3]);

    for (size_t i = 0; i < 6; i++)
        normals.emplace_back(Eigen::Vector3f(0.f, 0.f, z));

    PH_ASSERT(vertices.size() == normals.size());

    auto mesh = world->createMesh({
        .count     = vertices.size(),
        .positions = {(const float *) vertices.data(), sizeof(float) * 3},
        .normals   = {(const float *) normals.data(), sizeof(float) * 3},
    });
    return mesh;
}

rt::Node * ModelViewer::addQuad(const char * name, float w, float h, ph::rt::Material * material, ph::rt::Node * parent,
                                const ph::rt::NodeTransform & transform) {
    auto mesh  = createQuad(w, h);
    mesh->name = name;
    return addMeshNode(parent, transform, mesh, material);
}

rt::Mesh * ModelViewer::createCircle(float w, float h) {

    //        /--2
    //   _/---   |
    // 0 ------- 1
    int                          subdivisions      = 12;
    float                        degPerSubdivision = 360 / 12;
    const float                  z                 = options.leftHanded ? -1.0f : 1.0f;
    std::vector<Eigen::Vector3f> vertices;
    std::vector<Eigen::Vector3f> normals;
    for (int i = 0; i < subdivisions; i++) {
        vertices.emplace_back(Eigen::Vector3f::Zero()); // Center is always at the origin
        Eigen::Vector3f v1, v2;
        float           theta1 = i * degPerSubdivision * PI / 180.f;
        float           theta2 = (i + 1) * degPerSubdivision * PI / 180.f;
        v1                     = Eigen::Vector3f(w * cos(theta1), h * sin(theta1), 0.0f);
        v2                     = Eigen::Vector3f(w * cos(theta2), h * sin(theta2), 0.0f);
        vertices.emplace_back(options.leftHanded ? v2 : v1);
        vertices.emplace_back(options.leftHanded ? v1 : v2);

        // Same normal at every vertex
        normals.emplace_back(Eigen::Vector3f(0.f, 0.f, z));
        normals.emplace_back(Eigen::Vector3f(0.f, 0.f, z));
        normals.emplace_back(Eigen::Vector3f(0.f, 0.f, z));
    }

    PH_ASSERT(vertices.size() == normals.size());

    auto mesh = world->createMesh({
        .count     = vertices.size(),
        .positions = {(const float *) vertices.data(), sizeof(float) * 3},
        .normals   = {(const float *) normals.data(), sizeof(float) * 3},
    });
    return mesh;
}

// Todo: Subdivide
// Currently defaults to 12 triangles in a fan about the center.
// Repeats vertices; no indices.
rt::Node * ModelViewer::addCircle(const char * name, float w, float h, ph::rt::Material * material, ph::rt::Node * parent,
                                  const ph::rt::NodeTransform & transform) {
    auto mesh  = createCircle(w, h);
    mesh->name = name;
    return addMeshNode(parent, transform, mesh, material);
}

// ---------------------------------------------------------------------------------------------------------------------
//
rt::Node * ModelViewer::addBox(const char * name, float w, float h, float d, rt::Material * material, rt::Node * parent, const rt::NodeTransform & transform) {
    // create box walls
    const float z      = options.leftHanded ? -1.0f : 1.0f; // flip Z for left handed coordinate system.
    const float l      = -w / 2.0f;                         // left
    const float r      = w / 2.0f;                          // right
    const float f      = d / 2.0f * z;                      // front
    const float k      = -d / 2.0f * z;                     // back
    const float t      = h / 2.0f;                          // top
    const float b      = -h / 2.0f;                         // bottom
    const float v[][3] = {

        //     7-------6
        //   / |     / |
        //  3-------2  |
        //  |  |    |  |
        //  |  4----|--5
        //  | /     | /
        //  0-------1

        {l, b, f}, // 0
        {r, b, f}, // 1
        {r, t, f}, // 2
        {l, t, f}, // 3
        {l, b, k}, // 4
        {r, b, k}, // 5
        {r, t, k}, // 6
        {l, t, k}, // 7
    };

    std::vector<Eigen::Vector3f> vertices;
    std::vector<Eigen::Vector3f> normals;

    auto addWall = [&](int a, int b, int c, int d, float nx, float ny, float nz) {
        vertices.emplace_back(v[a]);
        vertices.emplace_back(v[b]);
        vertices.emplace_back(v[c]);
        vertices.emplace_back(v[a]);
        vertices.emplace_back(v[c]);
        vertices.emplace_back(v[d]);

        for (size_t i = 0; i < 6; ++i) {
            normals.emplace_back(nx, ny, nz);
        }
    };

    addWall(0, 1, 2, 3, 0.f, 0.f, 1.f * z);  // front
    addWall(5, 4, 7, 6, 0.f, 0.f, -1.f * z); // back
    addWall(3, 2, 6, 7, 0.f, 1.f, 0.f * z);  // top
    addWall(4, 5, 1, 0, 0.f, -1.f, 0.f * z); // bottom
    addWall(4, 0, 3, 7, -1.f, 0.f, 0.f * z); // left
    addWall(1, 5, 6, 2, 1.f, 0.f, 0.f * z);  // right

    PH_ASSERT(vertices.size() == normals.size());
    auto mesh  = world->createMesh({
        .count     = vertices.size(),
        .positions = {(const float *) vertices.data(), sizeof(float) * 3},
        .normals   = {(const float *) normals.data(), sizeof(float) * 3},
    });
    mesh->name = name;
    return addMeshNode(parent, transform, mesh, material);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::togglePrimaryCamera() { setPrimaryCamera((selectedCameraIndex + 1) % cameras.size()); }

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::setPrimaryCamera(size_t index) {
    if (index == selectedCameraIndex) return;

    // See if the index is valid
    if (index >= cameras.size()) {
        PH_LOGE("invalid camera index.");
        return;
    }

    // Save it to the parameters to change how the scene is rendered.
    selectedCameraIndex = index;
}

void ModelViewer::describeImguiUI() {
    if (options.showFrameTimes && ImGui::TreeNode("Ray Tracing GPU Perf")) {
        const auto & frameDuration = cp().app.loop().frameDuration();
        auto         drawPerfRow   = [](int level, const char * name, uint64_t durationNs, const uint64_t totalNs) {
            ImGui::TableNextColumn();
            std::stringstream ss;
            for (int j = 0; j < level; ++j)
                ss << " ";
            ss << name;
            ImGui::Text("%s", ss.str().c_str());

            // TODO: align to right
            ImGui::TableNextColumn();
            ImGui::Text("%s", ns2str(durationNs).c_str());

            ImGui::TableNextColumn();
            ImGui::Text("[%4.1f%%]", (double) durationNs * 100.0 / totalNs);
        };
        ImGui::BeginTable("Ray Tracing GPU Perf", 3, ImGuiTableFlags_Borders);
        for (const auto & i : scene->perfStats().gpuTimestamps) {
            drawPerfRow(0, i.name, i.durationNs, frameDuration.gpu.average);
        }
        for (const auto & i : pathRayTracingRenderPack->perfStats().gpuTimestamps) {
            drawPerfRow(0, i.name, i.durationNs, frameDuration.gpu.average);
        }
        if (shadowRenderPack) {
            for (const auto & i : shadowRenderPack->perfStats().gpuTimestamps) {
                drawPerfRow(0, i.name, i.durationNs, frameDuration.gpu.average);
            }
        }
        ImGui::EndTable();
        ImGui::TreePop();
    }
    if (options.showDebugUI && ImGui::TreeNode("Debug")) {
        if (ImGui::TreeNode("Quality")) {
            if (ImGui::TreeNode("Ray Bounces")) {
                if (options.rpmode == Options::RenderPackMode::PATH_TRACING) {
                    ImGui::SliderInt("Max Diffuse Bounces", (int *) &recordParameters.maxDiffuseBounces, 0, 5);
                }
                ImGui::SliderInt("Max Specular Bounces", (int *) &recordParameters.maxSpecularBounces, 0, 10);
                ImGui::TreePop();
            }
            ImGui::SliderFloat("Roughness Cutoff", &recordParameters.reflectionRoughnessCutoff, 0.0f, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Shadow")) {
            if (ImGui::BeginTable("Shadow Mode", 4)) {
                using ShadowMode = ph::rt::RayTracingRenderPack::ShadowMode;
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Ray Traced", recordParameters.shadowMode == ShadowMode::RAY_TRACED)) {
                    recordParameters.shadowMode = ShadowMode::RAY_TRACED;
                }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Rasterized", recordParameters.shadowMode == ShadowMode::RASTERIZED)) {
                    recordParameters.shadowMode = ShadowMode::RASTERIZED;
                }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Hybrid", recordParameters.shadowMode == ShadowMode::REFINED)) { recordParameters.shadowMode = ShadowMode::REFINED; }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Debug", recordParameters.shadowMode == ShadowMode::DEBUG)) { recordParameters.shadowMode = ShadowMode::DEBUG; }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Render Pack")) {
            if (ImGui::BeginListBox("", ImVec2(0, 4 * ImGui::GetTextLineHeightWithSpacing()))) {
                using RenderPackMode = Options::RenderPackMode;
                if (ImGui::Selectable("Rasterize", targetMode == RenderPackMode::RASTERIZED)) { targetMode = RenderPackMode::RASTERIZED; }
                if (ImGui::Selectable("Path Tracing", targetMode == RenderPackMode::PATH_TRACING)) { targetMode = RenderPackMode::PATH_TRACING; }
                if (ImGui::Selectable("Noise-Free Path Tracing", targetMode == RenderPackMode::NOISE_FREE)) { targetMode = RenderPackMode::NOISE_FREE; }
                if (ImGui::Selectable("Shadows Only Tracing", targetMode == RenderPackMode::SHADOW_TRACING)) { targetMode = RenderPackMode::SHADOW_TRACING; }
                ImGui::EndListBox();
            }
            ImGui::TreePop();
        }

        bool softwareRayQuery = !cp().app.cp().rayQuery;
        bool noiseFree        = Options::RenderPackMode::NOISE_FREE == options.rpmode || Options::RenderPackMode::SHADOW_TRACING == options.rpmode;
        if (softwareRayQuery || noiseFree) {
            // for now, this heat view only works when we use in-house BVH traversal.
            ImGui::Checkbox("Show heat view", &recordParameters.enableHeatMap);
            if (!noiseFree && recordParameters.enableHeatMap) {
                ImGui::SliderFloat("Max # traversal steps", &recordParameters.maxNumTraversalSteps, 0.0, 300.0, "%.1f");
            }
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Camera")) {
        const auto & p = firstPersonController.position();
        const auto & a = firstPersonController.angle();
        ImGui::Text("position: %f, %f, %f", p.x(), p.y(), p.z());
        ImGui::Text("angle   : %f, %f, %f", a.x(), a.y(), a.z());
        if (firstPersonController.orbiting()) {
            const auto & c = firstPersonController.orbitalCenter();
            ImGui::Text("orbital center: %f, %f, %f", c.x(), c.y(), c.z());
            ImGui::Text("orbital radius: %f", firstPersonController.orbitalRadius());
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Light")) {
        ImGui::ColorEdit3("Ambient", &recordParameters.ambientLight.x());
        ImGui::SliderFloat("Skybox Rotation", &recordParameters.skyboxRotation, 0.0f, 2.f * PI);
        ImGui::Text("Light Count: %u", (int) lights.size()); // show light amt
        for (size_t i = 0; i < lights.size(); ++i) {
            if (ImGui::TreeNode(formatstr("Light %zu", i))) {
                auto light = lights[i];
                auto desc  = light->desc();
                // bool enabled = desc.type > 0;
                // ImGui::Checkbox("Enabled", &enabled);
                // changed to color for ease of use
                ImGui::ColorEdit3("emission", desc.emission,
                                  ImGuiColorEditFlags_Float |              // use 0f-1f instead of 0 to 255
                                      ImGuiColorEditFlags_HDR |            // have values greater than 1
                                      ImGuiColorEditFlags_PickerHueWheel); // changed to a color wheel for ease of use
                ImGui::Checkbox("Enable Debug Mesh", debugManager.getDebugEnable(light));
                switch (desc.type) {
                case rt::Light::POINT:
                    ImGui::SliderFloat("range", &desc.point.range, 0.01f, 1000.0f);
                    ImGui::SliderFloat("radius", &desc.dimension[0], 0.0f, 100.0f);
                    break;
                case rt::Light::DIRECTIONAL:
                    ImGui::SliderFloat3("dir", &desc.directional.direction[0], -1.0f, 1.0f);
                    ImGui::SliderFloat3("bboxMin", &desc.directional.bboxMin[0], -1000.0f, 1000.0f);
                    ImGui::SliderFloat3("bboxMax", &desc.directional.bboxMax[0], -1000.0f, 1000.0f);
                    ImGui::SliderFloat("area light width", &desc.dimension[0], 0.0, 100.0f);
                    ImGui::SliderFloat("area light height", &desc.dimension[1], 0.0, 100.0f);
                    break;
                case rt::Light::SPOT:
                    ImGui::SliderFloat3("dir", &desc.spot.direction[0], -1.0f, 1.0f);
                    ImGui::SliderFloat("inner cone angle", &desc.spot.inner, 0.0f, floor(HALF_PI * 100.0f) / 100.0f);
                    ImGui::SliderFloat("outer cone angle", &desc.spot.outer, 0.0f, floor(HALF_PI * 100.0f) / 100.0f);
                    ImGui::SliderFloat("disk light width", &desc.dimension[0], 0.0, 100.0f);
                    ImGui::SliderFloat("disk light height", &desc.dimension[1], 0.0, 100.0f);
                    ImGui::SliderFloat("range", &desc.spot.range, 0.01f, 1000.0f);
                    break;
                default:
                    // do nothing
                    break;
                }
                light->reset(desc);
                debugManager.updateDebugLight(light);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    skinningManager.describeImguiUI(options.skinMode);

    if (options.rpmode == Options::RenderPackMode::PATH_TRACING) ptConfig.describeImguiUI();
};

ImageObject ModelViewer::createShadowCubeMap() {
    ImageObject shadowMap;
    // create a shadow map texture
    auto & vgi = dev().vgi();
    shadowMap.create("shadow map", vgi,
                     ImageObject::CreateInfo {}
                         .setCube(shadowMapSize)
                         .setFormat(shadowMapFormat)
                         .setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

    // clear shadow map to FLT_MAX
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(vgi.device, shadowMap.image, &requirements);
    BufferObject<VK_BUFFER_USAGE_TRANSFER_SRC_BIT, DeviceMemoryUsage::CPU_ONLY> sb;
    sb.allocate(vgi, requirements.size, "Shadow Map Staging Buffer");
    auto shadowMapPixels = sb.map<uint8_t>();
    for (size_t i = 0; i < requirements.size / sizeof(float); ++i) {
        auto smp = reinterpret_cast<float *>(shadowMapPixels.range.data());
        smp[i]   = std::numeric_limits<float>::max();
    }
    shadowMapPixels.unmap();
    SingleUseCommandPool cmdpool(dev().graphicsQ());
    auto                 cb                          = cmdpool.create();
    VkBufferImageCopy    bufferCopyRegion            = {};
    bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel       = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount     = 6;
    bufferCopyRegion.imageExtent.width               = shadowMapSize;
    bufferCopyRegion.imageExtent.height              = shadowMapSize;
    bufferCopyRegion.imageExtent.depth               = 1;
    bufferCopyRegion.bufferOffset                    = 0;
    setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6});
    vkCmdCopyBufferToImage(cb, sb.buffer, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
    setImageLayout(cb, shadowMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6});

    cmdpool.finish(cb);
    return shadowMap;
}
