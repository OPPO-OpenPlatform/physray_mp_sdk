/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"

#include "modelviewer.h"
#include "animations/timeline.h"
#include "mesh-utils.h"
#include "fatmesh.h"
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
struct RtCommandQueueProxy : ph::rt::CommandQueueProxy {
    ph::va::VulkanSubmissionProxy & sp;
    RtCommandQueueProxy(ph::va::VulkanSubmissionProxy & sp_): sp(sp_) {}

    uint32_t queueFamilyIndex() const override { return sp.queueFamilyIndex(); }

    VkResult submit(const ph::rt::BlobProxy<SubmitInfo> & range, VkFence signalFence = VK_NULL_HANDLE) override {
        static_assert(sizeof(SubmitInfo) == sizeof(ph::va::VulkanSubmissionProxy::SubmitInfo), "");
        static_assert(sizeof(range) == sizeof(ph::ConstRange<ph::va::VulkanSubmissionProxy::SubmitInfo>), "");
        return sp.submit((const ph::ConstRange<ph::va::VulkanSubmissionProxy::SubmitInfo> &) range, signalFence);
    }

    // wait for the queue to be completely idle (including both CPU and GPU)
    VkResult waitIdle() override { return sp.waitIdle(); }
};

// ---------------------------------------------------------------------------------------------------------------------
//
ModelViewer::ModelViewer(SimpleApp & app, const Options & o)
    : SimpleScene(app), options(o), skinningManager(app.dev().vgi()), ptConfig(ph::rt::World::RayTracingRenderPackCreateParameters::isStochastic(o.rpmode)),
      sbb(app.dev()) {

    // create main color pass
    recreateColorRenderPass();

    // create asset system
    auto ascp = AssetSystem::CreateParameters {
        {
            ASSET_FOLDER,
            getExecutableFolder() + "/asset",
        },
        1024,
    };
    for (auto f : o.additionalAssetFolders) { ascp.roots.push_back(f); }
    assetSys = AssetSystem::create(ascp);

    // Initialize texture cache so that images can be reused.
    textureCache.reset(new TextureCache(&dev().graphicsQ(), assetSys, shadowMapFormat, shadowMapSize));

    cmdProxy = new RtCommandQueueProxy(dev().graphicsQ());

    // create RT world instance
    const auto & vgi = dev().vgi();
    auto         wcp = World::WorldCreateParameters {vgi.allocator,
                                             vgi.instance,
                                             vgi.phydev,
                                             vgi.device,
                                             vgi.vmaAllocator,
                                             cmdProxy,
                                             std::vector<ph::rt::StrA> {ph::rt::StrA(ASSET_FOLDER)},
                                             nullptr, // &cpuFrameTimes,
                                             true,    // GPU timestamps.
                                             app.cp().rayQuery ? World::WorldCreateParameters::KHR_RAY_QUERY : World::WorldCreateParameters::AABB_GPU};
    world            = World::createWorld(wcp);
    resetScene();
    // pause the animation if asked.
    if (!o.animated) setAnimated(false);
}

void ModelViewer::resetScene() {
    threadSafeDeviceWaitIdle(dev().vgi().device);
    if (debugManager) debugManager.reset();
    cameras.clear();
    lights.clear();

    // Create new scene (delete old one first)
    world->deleteScene(scene);
    scene = world->createScene({});

    // initialize debug scene manager
    if (options.enableDebugGeometry) {
        sphereMesh   = createIcosahedron(1.0f, 2);
        circleMesh   = createCircle(1.0f, 1.0f);
        quadMesh     = createQuad(1.0f, 1.0f);
        debugManager = std::make_unique<scenedebug::SceneDebugManager>(world, scene, sphereMesh, circleMesh, quadMesh);
    }

    // create default materials
    auto mcd   = rt::Material::Desc {};
    lambertian = scene->create("lambertian", mcd);
    mcd.setRoughness(0.5f);
    glossy = scene->create("glossy", mcd);

    // setup default record parameters
    recordParameters.shadowMode   = options.shadowMode;
    recordParameters.ambientLight = {.01f, .01f, .01f};

    // Creates the node and the default camera.
    _firstPersonNode      = scene->createNode({});
    float  w              = (float) sw().initParameters().width;
    float  h              = (float) sw().initParameters().height;
    Camera firstPersonCam = {.yFieldOfView = h / std::min<float>(w, h), // roughly 60 degree vertical FOV.
                             .handness     = Camera::RIGHT_HANDED,
                             .zNear        = 1.0f,
                             .zFar         = 3.0f,
                             .node         = _firstPersonNode};
    cameras.push_back(firstPersonCam);
}

std::vector<Eigen::Vector3f> ModelViewer::calculateTriangleTangents(const std::vector<Eigen::Vector3f> & normals, const float * aniso) {
    std::vector<Eigen::Vector3f> tangents;
    tangents.resize(normals.size());
    std::vector<float>    normflat;
    std::vector<uint32_t> iempty;
    std::vector<float>    empty;
    std::vector<float>    tanflat;
    normflat.resize(normals.size() * 3);
    tanflat.resize(normals.size() * 3);
    for (size_t i = 0; i < normals.size(); i++) {
        const Eigen::Vector3f & n = normals[i];
        normflat[i * 3 + 0]       = n.x();
        normflat[i * 3 + 1]       = n.y();
        normflat[i * 3 + 2]       = n.z();
    }
    tanflat = calculateSmoothTangents(iempty, empty, empty, normflat, aniso);
    for (size_t i = 0; i < normals.size(); i++) { tangents[i] = Eigen::Vector3f(tanflat[i * 3 + 0], tanflat[i * 3 + 1], tanflat[i * 3 + 2]); }
    return tangents;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::recreateColorRenderPass() {
    // create main color pass
    auto newFormat = sw().initParameters().colorFormat;
    if (_colorPass && _colorTargetFormat == newFormat) {
        // skip redundant creation.
        return;
    }

    auto & vgi         = app().dev().vgi();
    _colorPass         = createRenderPass(vgi, newFormat, options.clearColorOnMainPass, VK_FORMAT_D24_UNORM_S8_UINT, options.clearDepthOnMainPass);
    _colorTargetFormat = newFormat;
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::resized() {
    auto & vgi = app().dev().vgi();

    recreateColorRenderPass();

    // create depth buffer
    _depthBuffer.create("depth buffer", vgi,
                        ImageObject::CreateInfo {}
                            .set2D(sw().initParameters().width, sw().initParameters().height)
                            .setFormat(VK_FORMAT_D24_UNORM_S8_UINT)
                            .setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                            .setMemoryUsage(DeviceMemoryUsage::GPU_ONLY));

    // clear depth stencil buffer
    auto                 sr = VkImageSubresourceRange {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    SingleUseCommandPool pool(dev().graphicsQ());
    auto                 cb = pool.create();
    setImageLayout(cb, _depthBuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, sr, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    auto cv = VkClearDepthStencilValue {1.0f, 0};
    vkCmdClearDepthStencilImage(cb, _depthBuffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cv, 1, &sr);
    setImageLayout(cb, _depthBuffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, sr,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    pool.finish(cb);

    // create frame buffer object for each back buffer
    uint32_t w = sw().initParameters().width;
    uint32_t h = sw().initParameters().height;
    _frameBuffers.resize(sw().backBufferCount());
    for (size_t i = 0; i < _frameBuffers.size(); ++i) {
        auto &      bb = sw().backBuffer(i);
        VkImageView views[] {bb.view, _depthBuffer.view};
        auto        ci = va::util::framebufferCreateInfo(_colorPass, std::size(views), views, w, h);
        PH_VA_REQUIRE(vkCreateFramebuffer(vgi.device, &ci, vgi.allocator, _frameBuffers[i].colorFb.prepare(vgi)));
    }

    auto newW = sw().initParameters().width;
    auto newH = sw().initParameters().height;
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
    ph::rt::Node * node = scene->createNode({parent});
    node->setTransform(transform);

    node->name = mesh->name;

    auto model = scene->createModel({
        .mesh     = *mesh,
        .material = *material,
    });

    // Create the object that will display the mesh.
    node->attachComponent(model);

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
    } else {
        recordParameters.irradianceMap = {};
        recordParameters.reflectionMap = {};
    }

    Skybox::ConstructParameters cp = {loop(), *assetSys};
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
    cameras[0].handness = options.leftHanded ? Camera::LEFT_HANDED : Camera::RIGHT_HANDED;
    cameras[0].zNear    = sceneExtent / 100.0f;
    cameras[0].zFar     = sceneExtent * 4.0f;

    firstPersonController.setHandness(cameras[0].handness)
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
    setBounds(bbox);
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
    auto node = scene->createNode({});
    node->setTransform(transform);
    node->name = name;
    auto light = scene->createLight({});
    node->attachComponent(light);
    light->reset(ph::rt::Light::Desc()
                     .setPoint(ph::rt::Light::Point())
                     .setDimension(radius, radius)
                     .setRange(range)
                     .setEmission(emission.x(), emission.y(), emission.z()));
    light->name = name;

    // Give the light a backing shadow map.
    light->shadowMap = textureCache->createShadowMapCube(name.c_str());

    // Save to the list of lights.
    lights.push_back(light);

    if (debugManager) debugManager->setDebugEnable(light, enableDebugMesh);

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
    auto node = scene->createNode({});
    node->setTransform(transform);

    if (cones.x() > HALF_PI) {
        PH_LOGI("Outer radius of a spot light cannot exceed PI/2 or 90 degrees.\n");
        cones.x() = HALF_PI;
    }

    if (cones.y() > cones.x()) {
        PH_LOGI("Inner radius of a spot light cannot exceed its outer radius.\n");
        cones.y() = cones.x();
    }

    auto light = scene->createLight({});
    node->attachComponent(light);
    light->reset(ph::rt::Light::Desc()
                     .setSpot(ph::rt::Light::Spot().setDir(direction.x(), direction.y(), direction.z()).setFalloff(cones.y(), cones.x()))
                     .setDimension(dimensions.x(), dimensions.y())
                     .setEmission(emission.x(), emission.y(), emission.z())
                     .setRange(range));

    // TODO: Not tested with spot lights yet.
    light->shadowMap = textureCache->createShadowMap2D("spot light");
    lights.push_back(light);

    if (debugManager) debugManager->setDebugEnable(light, enableDebugMesh);

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
    auto node = scene->createNode({});
    node->setTransform(transform);
    Eigen::Vector2f dims = Eigen::Vector2f::Zero();
    if (dimensions) dims = *dimensions;

    auto light = scene->createLight({});
    node->attachComponent(light);
    light->reset(ph::rt::Light::Desc()
                     .setDirectional(ph::rt::Light::Directional().setDir(direction.x(), direction.y(), direction.z()))
                     .setDimension(dims.x(), dims.y())
                     .setEmission(emission.x(), emission.y(), emission.z()));

    // TODO haven't set up directional light shadow map rendering
    light->shadowMap = textureCache->createShadowMap2D("directional light");

    lights.push_back(light);

    if (debugManager) debugManager->setDebugEnable(light, enableDebugMesh);

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
    _renderPackDirty = false;
    _accumDirty      = true;
    threadSafeDeviceWaitIdle(dev().vgi().device);
    world->deleteRayTracingRenderPack(pathRayTracingRenderPack);
    PH_ASSERT(!pathRayTracingRenderPack);
    auto w  = sw().initParameters().width;
    auto h  = sw().initParameters().height;
    auto cp = World::RayTracingRenderPackCreateParameters {options.rpmode}
                  .setTarget(sw().initParameters().colorFormat, w, h, VK_IMAGE_LAYOUT_UNDEFINED)
                  .setViewport(0.f, 0.f, (float) w, (float) h)
                  .setClear(true);
    cp.targetIsSRGB                     = true;
    cp.usePrecompiledShaderParameters   = options.usePrecompiledShaderParameters;
    cp.refractionAndRoughReflection     = options.refractionAndRoughReflection;
    pathRayTracingRenderPack            = world->createRayTracingRenderPack(cp);
    recordParameters.spp                = options.spp;
    recordParameters.maxDiffuseBounces  = options.diffBounces;
    recordParameters.maxSpecularBounces = options.specBounces;
    recordParameters.spp                = options.spp;
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
void ModelViewer::update() {
    if (_renderPackDirty) recreateMainRenderPack();

    // Update first person controller and node, only when the first person camera is selected.
    if (0 == selectedCameraIndex) {
        // Update the camera controller.
        firstPersonController.update((float) app().gameTime().sinceLastUpdate.count() / 1000000.0f);

        // Update the first person camera to the position of the camera controller.
        _firstPersonNode->setWorldTransform(firstPersonController.getWorldTransform());

        // Automatically adjust z-far for orbital camera.
        if (firstPersonController.orbiting()) {
            static float initialZFar = cameras[0].zFar;
            auto         r           = firstPersonController.orbitalRadius();
            cameras[0].zNear         = std::max(r / 100.f, initialZFar / 1000.0f);
            cameras[0].zFar          = std::max(r * 2.f, initialZFar);
        }
    }

    // TODO: update recordParameters.camera.zFar based on camera's distance to
    // the scene center to avoid clipping when camera is away from the scene.

    // DEBUG
    // auto                      startAnim = std::chrono::high_resolution_clock::now();

    // Update the animations.
    if (animated() && !animations.empty()) {
        bool running = false;
        for (auto & a : animations) {
            // Check if any animation is playing. Check this before ticking for the current frame,
            // so that there is time for animation updating logic (like in ptdemo) to play other
            // animations before a requestForQuit is set.
            if (!running && a->getPlayCount() < a->getRepeatCount()) running = true;
            a->tick(app().gameTime().sinceLastUpdate);
        }
        if (!running)
            loop().requestForQuit();
        else
            overrideAnimations();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
VkImageLayout ModelViewer::record(const SimpleRenderLoop::RecordParameters & rp) {
    PassParameters pp = {
        rp.cb,
        sw().backBuffer(rp.backBufferIndex),
        _depthBuffer.view,
    };
    PH_ASSERT(pp.bb.image);
    PH_ASSERT(pp.bb.view);
    PH_ASSERT(pp.depthView);

    // call offscreen pass first.
    {
        SimpleCpuFrameTimes::ScopedTimer c(app().cpuTimes(), "OffscreenPass");
        AsyncTimestamps::ScopedQuery     q(app().gpuTimes(), rp.cb, "OffscreenPass");
        recordOffscreenPass(pp);
    }

    // Then do the main color pass
    VkClearValue clearValues[2];
    clearValues[0].color        = clearColor;
    clearValues[1].depthStencil = clearDepthStencil;
    VkRenderPassBeginInfo info  = {};
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass             = _colorPass;
    info.framebuffer            = _frameBuffers[rp.backBufferIndex].colorFb;
    info.renderArea.extent      = pp.bb.extent;
    info.clearValueCount        = 2;
    info.pClearValues           = clearValues;
    vkCmdBeginRenderPass(rp.cb, &info, VK_SUBPASS_CONTENTS_INLINE);

    // render to main color buffer
    {
        SimpleCpuFrameTimes::ScopedTimer c(app().cpuTimes(), "MainColorPass");
        AsyncTimestamps::ScopedQuery     q(app().gpuTimes(), rp.cb, "MainColorPass");
        recordMainColorPass(pp);
    }

    // render UI
    if (options.showUI) {
        SimpleCpuFrameTimes::ScopedTimer c(app().cpuTimes(), "UIPass");
        AsyncTimestamps::ScopedQuery     q(app().gpuTimes(), rp.cb, "UIPass");
        app().ui().record({mainColorPass(), rp.cb,
                           [&](void * user) {
                               auto p = (ModelViewer *) user;
                               p->drawUI();
                           },
                           this});
    }

    // end of main color pass
    vkCmdEndRenderPass(rp.cb);

    // done
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

bool ModelViewer::accumDirty() {
    return (0 == options.accum || animated() || _lastCameraPosition != firstPersonController.position() ||
            _lastCameraRotation != firstPersonController.angle() || _accumDirty);
}

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::recordOffscreenPass(const PassParameters & p) {
    if (!scene) return;

    auto & renderLoop   = app().loop();
    auto   frameCounter = renderLoop.frameCounter();
    auto   safeFrame    = renderLoop.safeFrame();
    world->updateFrameCounter(frameCounter, safeFrame);
    skinningManager.record(renderLoop, p.cb);
    scene->refreshGpuData(p.cb);

    // stochastic path tracers don't need shadow maps
    if (shadowRenderPack && (animated() || loop().frameCounter() == 0) && !ph::rt::World::RayTracingRenderPackCreateParameters::isStochastic(options.rpmode)) {
        for (auto & l : lights) {
            if (Light::Type::OFF == l->desc().type || !l->desc().allowShadow) continue;

            shadowParameters.commandBuffer = p.cb;
            shadowParameters.light         = l;
            shadowRenderPack->record(shadowParameters);

            // transfer shadow map layout from writing to reading
            va::setImageLayout(p.cb, l->shadowMap.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VkImageSubresourceRange {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
    }

    // setup recording parameters then start recording of command buffer.
    recordParameters.scene         = scene;
    recordParameters.commandBuffer = p.cb;
    recordParameters.targetView    = p.bb.view;
    recordParameters.depthView     = p.depthView;
    recordParameters.targetImage   = p.bb.image;
    recordParameters.projMatrix =
        fromEigen(cameras[selectedCameraIndex].calculateProj(((float) sw().initParameters().width), ((float) sw().initParameters().height)));
    recordParameters.viewMatrix    = NodeTransform(cameras[selectedCameraIndex].worldTransform().inverse());
    recordParameters.sceneExtents  = fromEigen(Eigen::Vector3f(_bounds.diagonal()));
    auto center                    = _bounds.center();
    recordParameters.sceneCenter.x = center.x();
    recordParameters.sceneCenter.y = center.y();
    recordParameters.sceneCenter.z = center.z();
    ptConfig.setupRp(recordParameters);
    pathRayTracingRenderPack->record(recordParameters);

    // update accumulation parameters
    using Accumulation = RayTracingRenderPack::Accumulation;
    if (accumDirty()) {
        recordParameters.accum = Accumulation::OFF;
        _accumProgress         = 0.0f;
        _accumDirty            = false;
    } else if (options.accum > 0) {
        // frame based limiter
        if (Accumulation::OFF == recordParameters.accum) {
            // start new accumulation process
            _accumProgress         = 0.0f;
            _accumulatedFrames     = 0;
            recordParameters.accum = Accumulation::ON;
        } else if (Accumulation::ON == recordParameters.accum) {
            _accumProgress = (float) _accumProgress / options.accum;
            ++_accumulatedFrames;
            if (_accumulatedFrames >= (size_t) options.accum) {
                PH_LOGI("Accumulation completed: %llu frames.", _accumulatedFrames);
                recordParameters.accum = Accumulation::RETAIN;
            }
        }
    } else {
        // time based limiter
        if (Accumulation::OFF == recordParameters.accum) {
            // start new accumulation process
            _accumProgress         = 0.0f;
            _accumulationStartTime = std::chrono::high_resolution_clock::now();
            recordParameters.accum = Accumulation::ON;
        } else if (Accumulation::ON == recordParameters.accum) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - _accumulationStartTime).count();
            // Allow accumulation to be controlled by max/min frame rate by calculating
            // accumulation progress from elapsed frame time.
            // Still, track actual accumulation time for reporting.
            float currProgressInSeconds = _accumProgress * -options.accum;
            currProgressInSeconds += (float) app().gameTime().sinceLastUpdate.count() / 1000000.0f;
            _accumProgress = currProgressInSeconds / -options.accum;
            if (_accumProgress >= 1.0f) {
                PH_LOGI("Accumulation completed: %lld seconds", duration);
                doAccumulationComplete(p.cb);
                recordParameters.accum = Accumulation::RETAIN;
            }
        }
    }
    _lastCameraPosition = firstPersonController.position();
    _lastCameraRotation = firstPersonController.angle();
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
    if (skybox && (ph::rt::World::RayTracingRenderPackCreateParameters::isNoiseFree(options.rpmode))) {
        // The camera's transformation matrix.
        auto &          camera = cameras[selectedCameraIndex];
        Eigen::Matrix4f proj   = camera.calculateProj(((float) sw().initParameters().width), ((float) sw().initParameters().height));
        // Draw skybox as part of the main render pass
        // FIXME: in theory, we should set sRGB to TRUE, since the swapchain buffer is in sRGB color space (See VK_COLOR_SPACE_SRGB_NONLINEAR_KHR flag
        // used in swapchain.cpp). But somehow that makes the sky box too bright.
        skybox->draw(p.cb, proj, camera.worldTransform().rotation(), recordParameters.saturation, recordParameters.gamma, recordParameters.sRGB,
                     recordParameters.skyboxRotation, skyboxLodBias, toEigen(recordParameters.ambientLight));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
//
Eigen::AlignedBox3f ModelViewer::addModelToScene(const LoadOptions & o) {
    Eigen::AlignedBox3f bbox;
    PH_REQUIRE(addModelNodeToScene(o, bbox));
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
        if (!scene) {
            PH_LOGE("Failed to load GLTF scene: %s", modelPath.string().c_str());
            return nullptr;
        }
        bbox = scene->getBounds();
        return scene->getNodes()[0]; // FIXME how do we get the parent node of a scene?

        // If this is any other type of model.
    } else {
        PH_LOGE("unsupported file: %s", modelPath.string().c_str());
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

ph::rt::Mesh * ModelViewer::createNonIndexedMesh(size_t vertexCount, const float * positions, const float * normals, const float * texcoords,
                                                 const float * tangents) {
    ph::rt::Scene::MeshCreateParameters mcp = {vertexCount};

    // mcp.vertices.position.buffer = _sbb.uploadData(positions, vertexCount * 3);
    ConstRange<float, size_t> pos(positions, (size_t) vertexCount * 3);
    mcp.vertices.position.buffer = sbb.allocatePermanentBuffer<float>(pos)->buffer;
    mcp.vertices.position.stride = sizeof(Eigen::Vector3f);
    mcp.vertices.position.format = VK_FORMAT_R32G32B32_SFLOAT;
    if (normals != nullptr) {
        // mcp.vertices.normal.buffer = _sbb.uploadData(normals, vertexCount * 3);
        ConstRange<float, size_t> norms(normals, (size_t) vertexCount * 3);
        mcp.vertices.normal.buffer = sbb.allocatePermanentBuffer<float>(norms)->buffer;
        mcp.vertices.normal.stride = sizeof(Eigen::Vector3f);
        mcp.vertices.normal.format = VK_FORMAT_R32G32B32_SFLOAT;
    }
    if (texcoords != nullptr) {
        // mcp.vertices.texcoord.buffer = _sbb.uploadData(texcoords, vertexCount * 2);
        ConstRange<float, size_t> texs(texcoords, (size_t) vertexCount * 2);
        mcp.vertices.texcoord.buffer = sbb.allocatePermanentBuffer<float>(texs)->buffer;
        mcp.vertices.texcoord.stride = sizeof(Eigen::Vector2f);
        mcp.vertices.texcoord.format = VK_FORMAT_R32G32_SFLOAT;
    }
    if (tangents != nullptr) {
        // mcp.vertices.tangent.buffer = _sbb.uploadData(tangents, vertexCount * 3);
        ConstRange<float, size_t> tans(tangents, (size_t) vertexCount * 3);
        mcp.vertices.tangent.buffer = sbb.allocatePermanentBuffer<float>(tans)->buffer;
        mcp.vertices.tangent.stride = sizeof(Eigen::Vector3f);
        mcp.vertices.tangent.format = VK_FORMAT_R32G32B32_SFLOAT;
    }
    return scene->createMesh(mcp);
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
    auto jmesh  = createNonIndexedMesh(mesh.position.size(), (const float *) mesh.position.data(), (const float *) mesh.normal.data(),
                                      (const float *) mesh.texcoord.data(), (const float *) mesh.tangent.data());
    jmesh->name = o.model;
    auto jnode  = addMeshNode(o.parent, ph::rt::NodeTransform::Identity(), jmesh, material);

    PH_LOGI(".OBJ mesh loaded with %zu vertices", mesh.position.size());

    return jnode;
}

// ---------------------------------------------------------------------------------------------------------------------
//
std::shared_ptr<const SceneAsset> ModelViewer::loadGltf(const LoadOptions & o) {
    // load GLTF scene
    GLTFSceneReader sceneReader(assetSys, textureCache.get(), world, scene, skinningManager.skinDataMap(), &morphTargetManager, &sbb, o.createGeomLights);
    std::shared_ptr<const SceneAsset> sceneAsset = sceneReader.read(o.model);

    // Add contents to the scene.
    loadSceneAsset(o, sceneAsset.get());

    // Setup SkinnedMeshManager
    skinningManager.initializeSkinning(this->app().dev().graphicsQ());

    // Setup MorphTargetManager
    morphTargetManager.initializeMorphTargets(&(this->app().dev().graphicsQ()));

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
    for (auto & keyPair : nameToAnimations) { PH_LOGI("Model contains animation '%s'", keyPair.first.c_str()); }

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

    // Set animation repeat count based on options.
    uint32_t repeatCount;
    if (options.animated < 0) {
        repeatCount = (uint32_t) -options.animated;
    } else {
        repeatCount = ::animations::Timeline::REPEAT_COUNT_INDEFINITE;
    }
    for (auto & timeline : animations) { timeline->setRepeatCount(repeatCount); }
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
    std::vector<Eigen::Vector3f> vertices, normals, tangents;
    std::vector<Model::Subset>   subsets;
    auto                         addWall = [&](const char *, uint32_t wall, Material * m) {
        Model::Subset subset;
        subset.material   = m;
        subset.indexBase  = vertices.size();
        subset.indexCount = 6;
        subsets.push_back(subset);

        const uint32_t indices[] = {wall * 4 + 0, wall * 4 + 1, wall * 4 + 2, wall * 4 + 0, wall * 4 + 2, wall * 4 + 3};
        for (auto i : indices) {
            vertices.push_back(wallPositions[i]);
            normals.push_back(wallNormals[i]);
        }
    };
    auto baseDesc = []() { return rt::Material::Desc {}; };
    auto white    = scene->create("white", baseDesc());
    auto red      = scene->create("red", baseDesc().setAlbedo(1.f, 0.f, 0.f));
    auto green    = scene->create("green", baseDesc().setAlbedo(0.f, 1.f, 0.f));
    addWall("back", 0, white);
    addWall("top", 1, white);
    addWall("bottom", 2, white);
    addWall("left", 3, red);
    addWall("rght", 4, green);

    tangents   = calculateTriangleTangents(normals, &white->desc().anisotropic);
    auto mesh  = createNonIndexedMesh(vertices.size(), (float *) vertices.data(), (float *) normals.data(), nullptr, (const float *) tangents.data());
    mesh->name = "cornell box";

    // Create the node that will manage the transforms.
    ph::rt::Node * node = scene->createNode({});
    node->name          = mesh->name;

    // Create the object that will display the mesh.
    auto model = scene->createModel({
        .mesh     = *mesh,
        .material = *white,
        .subsets  = subsets,
    });
    node->attachComponent(model);
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

    std::vector<uint32_t> iempty;
    std::vector<float>    empty;
    std::vector<float>    tangents;
    tangents = calculateSmoothTangents(iempty, empty, empty, normals, &lambertian->desc().anisotropic);

    auto mesh  = createNonIndexedMesh(vertices.size() / 3, vertices.data(), normals.data(), nullptr, tangents.data());
    mesh->name = "floor";
    addMeshNode(nullptr, ph::rt::NodeTransform::Identity(), mesh, lambertian);

    return {Eigen::Vector3f(v[0]), Eigen::Vector3f(v[2])};
}

// ---------------------------------------------------------------------------------------------------------------------
//
rt::Mesh * ModelViewer::createIcosahedron(float radius, uint32_t subdivide, const float * aniso) {
    // when we generate a sphere with many faces, make the normal smooth.
    bool smoothNormal = subdivide > 0;

    // generate sphere vertices
    auto vertices = buildIcosahedronUnitSphere(subdivide);

    // deal with handness
    if (options.leftHanded) {
        for (auto & v : vertices) { v.z() = -v.z(); }
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

    std::vector<Eigen::Vector3f> tangents = calculateTriangleTangents(normals, aniso);

    PH_ASSERT(tangents.size() == vertices.size());

    // now create the mesh
    return createNonIndexedMesh(vertices.size(), vertices[0].data(), normals[0].data(), nullptr, tangents[0].data());
}

rt::Node * ModelViewer::addIcosahedron(const char * name, float radius, uint32_t subdivide, rt::Material * material, rt::Node * parent,
                                       const rt::NodeTransform & transform) {
    auto mesh = createIcosahedron(radius, subdivide, &material->desc().anisotropic);
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

    for (size_t i = 0; i < 6; i++) normals.emplace_back(Eigen::Vector3f(0.f, 0.f, z));

    std::vector<Eigen::Vector3f> tangents = calculateTriangleTangents(normals);

    PH_ASSERT(vertices.size() == normals.size());
    PH_ASSERT(vertices.size() == tangents.size());

    auto mesh = createNonIndexedMesh(vertices.size(), (const float *) vertices.data(), (const float *) normals.data(), (const float *) tangents.data());
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

    std::vector<Eigen::Vector3f> tangents = calculateTriangleTangents(normals);

    PH_ASSERT(vertices.size() == normals.size());
    PH_ASSERT(vertices.size() == tangents.size());

    return createNonIndexedMesh(vertices.size(), (const float *) vertices.data(), (const float *) normals.data(), nullptr, (const float *) tangents.data());
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
    std::vector<Eigen::Vector3f> tangents;

    auto addWall = [&](int a, int b, int c, int d, float nx, float ny, float nz) {
        vertices.emplace_back(v[a]);
        vertices.emplace_back(v[b]);
        vertices.emplace_back(v[c]);
        vertices.emplace_back(v[a]);
        vertices.emplace_back(v[c]);
        vertices.emplace_back(v[d]);

        for (size_t i = 0; i < 6; ++i) { normals.emplace_back(nx, ny, nz); }
    };

    addWall(0, 1, 2, 3, 0.f, 0.f, 1.f * z);  // front
    addWall(5, 4, 7, 6, 0.f, 0.f, -1.f * z); // back
    addWall(3, 2, 6, 7, 0.f, 1.f, 0.f * z);  // top
    addWall(4, 5, 1, 0, 0.f, -1.f, 0.f * z); // bottom
    addWall(4, 0, 3, 7, -1.f, 0.f, 0.f * z); // left
    addWall(1, 5, 6, 2, 1.f, 0.f, 0.f * z);  // right

    PH_ASSERT(vertices.size() == normals.size());
    tangents = calculateTriangleTangents(normals);

    auto mesh =
        createNonIndexedMesh(vertices.size(), (const float *) vertices.data(), (const float *) normals.data(), nullptr, (const float *) tangents.data());
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

// ---------------------------------------------------------------------------------------------------------------------
//
void ModelViewer::drawUI() {
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    const auto & frameDuration = loop().frameDuration();

    ImGui::SetNextWindowBgAlpha(0.3f);
    if (ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("FPS : %.1f [%s]", 1000000000.0 / frameDuration.all.average, ns2str(frameDuration.all.average).c_str());
        if (ImGui::TreeNode("Frame Time Breakdown")) {
            auto drawPerfRow = [](int level, const char * name, uint64_t durationNs, const uint64_t totalNs) {
                ImGui::TableNextColumn();
                std::stringstream ss;
                for (int j = 0; j < level; ++j) ss << " ";
                ss << name;
                ImGui::Text("%s", ss.str().c_str());

                // TODO: align to right
                ImGui::TableNextColumn();
                ImGui::Text("%s", ns2str(durationNs).c_str());

                ImGui::TableNextColumn();
                ImGui::Text("[%4.1f%%]", (double) durationNs * 100.0 / totalNs);
            };

            if (ImGui::TreeNode("GPU Perf")) {
                ImGui::Text("GPU Frame Time : %s", ns2str(frameDuration.gpu.average).c_str());
                ImGui::BeginTable("CPU Frame Time", 3, ImGuiTableFlags_Borders);
                for (const auto & i : app().gpuTimes().reportAll()) { drawPerfRow(0, i.name, i.durationNs, frameDuration.gpu.average); }
                ImGui::EndTable();
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("CPU Perf")) {
                ImGui::Text("CPU Frame Time : %s", ns2str(frameDuration.cpu.average).c_str());
                ImGui::BeginTable("CPU Frame Time", 3, ImGuiTableFlags_Borders);
                for (const auto & i : app().cpuTimes().reportAll()) { drawPerfRow(i.level, i.name, i.durationNs, frameDuration.cpu.average); }
                ImGui::EndTable();
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
        describeImguiUI();
    }
    ImGui::End();
}

void ModelViewer::describeImguiUI() {
    if (options.showFrameTimes && ImGui::TreeNode("Ray Tracing GPU Perf")) {
        const auto & frameDuration = app().loop().frameDuration();
        auto         drawPerfRow   = [](int level, const char * name, uint64_t durationNs, const uint64_t totalNs) {
            ImGui::TableNextColumn();
            std::stringstream ss;
            for (int j = 0; j < level; ++j) ss << " ";
            ss << name;
            ImGui::Text("%s", ss.str().c_str());

            // TODO: align to right
            ImGui::TableNextColumn();
            ImGui::Text("%s", ns2str(durationNs).c_str());

            ImGui::TableNextColumn();
            ImGui::Text("[%4.1f%%]", (double) durationNs * 100.0 / totalNs);
        };
        auto scenePerf = scene->perfStats();
        ImGui::Text("Active Instance Count = %zu", scenePerf.instanceCount);
        ImGui::Text("Active triangle Count = %zu", scenePerf.triangleCount);
        ImGui::BeginTable("Ray Tracing GPU Perf", 3, ImGuiTableFlags_Borders);
        for (const auto & i : scenePerf.gpuTimestamps) { drawPerfRow(0, i.name, i.durationNs, frameDuration.gpu.average); }
        for (const auto & i : pathRayTracingRenderPack->perfStats().gpuTimestamps) { drawPerfRow(0, i.name, i.durationNs, frameDuration.gpu.average); }
        if (shadowRenderPack) {
            for (const auto & i : shadowRenderPack->perfStats().gpuTimestamps) { drawPerfRow(0, i.name, i.durationNs, frameDuration.gpu.average); }
        }
        ImGui::EndTable();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Render Pack")) {
        if (ImGui::BeginListBox("", ImVec2(0, 4 * ImGui::GetTextLineHeightWithSpacing()))) {
            using RenderPackMode = Options::RenderPackMode;
            if (ImGui::Selectable("Rasterize", options.rpmode == RenderPackMode::RASTERIZED)) { setRpMode(RenderPackMode::RASTERIZED); }
            if (ImGui::Selectable("Shadows Only Tracing", options.rpmode == RenderPackMode::SHADOW_TRACING)) { setRpMode(RenderPackMode::SHADOW_TRACING); }
            if (ImGui::Selectable("Noise-Free Path Tracing", options.rpmode == RenderPackMode::NOISE_FREE)) { setRpMode(RenderPackMode::NOISE_FREE); }
            if (ImGui::Selectable("Fast Path Tracing", options.rpmode == RenderPackMode::FAST_PT)) { setRpMode(RenderPackMode::FAST_PT); }
            if (ImGui::Selectable("Path Tracing", options.rpmode == RenderPackMode::PATH_TRACING)) { setRpMode(RenderPackMode::PATH_TRACING); }
            ImGui::EndListBox();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Scene")) {
        bool animated_ = animated();
        if (ImGui::Checkbox("animated", &animated_)) { setAnimated(animated_); }
        if (ImGui::TreeNode("Camera")) {
            ImGui::Text("Active Camera: %zu", selectedCameraIndex);
            const auto & p = firstPersonController.position();
            const auto & a = firstPersonController.angle();
            ImGui::Text("position: %f, %f, %f", p.x(), p.y(), p.z());
            ImGui::Text("angle   : %f, %f, %f", a.x(), a.y(), a.z());
            NodeTransform      fptfm = firstPersonController.getWorldTransform();
            Eigen::Quaternionf fprotation;
            fptfm.decompose(nullptr, &fprotation, nullptr);
            ImGui::Text("rotation: %f, %f, %f, %f", fprotation.x(), fprotation.y(), fprotation.z(), fprotation.w());
            bool isOrbiting = firstPersonController.orbiting();
            if (ImGui::Checkbox("Use orbital camera", &isOrbiting)) {
                if (isOrbiting) {
                    firstPersonController.setOrbitalCenter(&p).setOrbitalRadius(0.f);
                } else {
                    firstPersonController.setOrbitalCenter(nullptr);
                }
            }
            if (isOrbiting) {
                const auto & c = firstPersonController.orbitalCenter();
                ImGui::Text("orbital center: %f, %f, %f", c.x(), c.y(), c.z());
                ImGui::Text("orbital radius: %f", firstPersonController.orbitalRadius());
            }
            for (size_t i = 0; i < cameras.size(); ++i) {
                auto & c = cameras[i];
                if (ImGui::TreeNode(formatstr("Camera %zu", i))) {
                    Eigen::Vector3f    t;
                    Eigen::Quaternionf r;
                    ph::rt::NodeTransform(c.node->worldTransform()).decompose(&t, &r, nullptr);
                    ImGui::Text("position: %f, %f, %f", t.x(), t.y(), t.z());
                    ImGui::Text("rotation: %f, %f, %f, %f", r.x(), r.y(), r.z(), r.w());
                    ImGui::SliderFloat("znear", &c.zNear, 0.00001f, 0.1f);
                    ImGui::Text("znear: %f, zfar: %f, yfov: %f", c.zNear, c.zFar, c.yFieldOfView);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Light")) {
            ImGui::ColorEdit3("Ambient", &recordParameters.ambientLight.x);
            if (ImGui::BeginTable("Light Objects from Skybox", 2)) { // todo make this work for path tracer?
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("On", recordParameters.skyboxLighting == 1)) { recordParameters.skyboxLighting = 1; }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Off", recordParameters.skyboxLighting == 0)) { recordParameters.skyboxLighting = 0; }
                ImGui::EndTable();
            }
            ImGui::SliderFloat("Skybox Rotation", &recordParameters.skyboxRotation, 0.0f, 2.f * PI);
            ImGui::Text("Light Count: %u", (int) lights.size()); // show light amt
            for (size_t i = 0; i < lights.size(); ++i) {
                auto light = lights[i];
                if (light) {
                    if (ImGui::TreeNode(formatstr("Light %zu", i))) {
                        auto desc = light->desc();
                        // bool enabled = desc.type > 0;
                        // ImGui::Checkbox("Enabled", &enabled);
                        ImGui::SliderFloat("bias", &light->shadowMapBias, 0.f, 0.01f);
                        ImGui::SliderFloat("slope bias", &light->shadowMapSlopeBias, 0.f, 0.01f);
                        // changed to color for ease of use
                        ImGui::ColorEdit3("emission", &desc.emission.x,
                                          ImGuiColorEditFlags_Float |              // use 0f-1f instead of 0 to 255
                                              ImGuiColorEditFlags_HDR |            // have values greater than 1
                                              ImGuiColorEditFlags_PickerHueWheel); // changed to a color wheel for ease of use
                        if (debugManager) ImGui::Checkbox("Enable Debug Mesh", debugManager->getDebugEnable(light));
                        ImGui::SliderFloat("range", &desc.range, 0.01f, 1000.0f);
                        bool  npAreaLight = (desc.dimension[0] < 0.0f) || (desc.dimension[1] < 0.0f);
                        float dim0        = std::abs(desc.dimension[0]);
                        float dim1        = std::abs(desc.dimension[1]);
                        if (ImGui::Checkbox("Non-physical area lights", &npAreaLight)) {
                            if (npAreaLight) {
                                desc.dimension[0] = -dim0;
                                desc.dimension[1] = -dim1;
                            } else {
                                desc.dimension[0] = dim0;
                                desc.dimension[1] = dim1;
                            }
                        }
                        switch (desc.type) {
                        case rt::Light::POINT:
                            if (ImGui::SliderFloat("radius", &dim0, 0.0f, 100.0f)) desc.dimension[0] = npAreaLight ? -dim0 : dim0;
                            break;
                        case rt::Light::DIRECTIONAL:
                            ImGui::SliderFloat3("dir", &desc.directional.direction[0], -1.0f, 1.0f);
                            ImGui::SliderFloat3("bboxMin", &desc.directional.bboxMin[0], -1000.0f, 1000.0f);
                            ImGui::SliderFloat3("bboxMax", &desc.directional.bboxMax[0], -1000.0f, 1000.0f);
                            if (ImGui::SliderFloat("rect light width", &dim0, 0.0, 100.0f)) desc.dimension[0] = npAreaLight ? -dim0 : dim0;
                            if (ImGui::SliderFloat("rect light height", &dim1, 0.0, 100.0f)) desc.dimension[1] = npAreaLight ? -dim1 : dim1;
                            break;
                        case rt::Light::SPOT:
                            ImGui::SliderFloat3("dir", &desc.spot.direction[0], -1.0f, 1.0f);
                            ImGui::SliderFloat("inner cone angle", &desc.spot.inner, 0.0f, floor(HALF_PI * 100.0f) / 100.0f);
                            ImGui::SliderFloat("outer cone angle", &desc.spot.outer, 0.0f, floor(HALF_PI * 100.0f) / 100.0f);
                            if (ImGui::SliderFloat("disk light width", &dim0, 0.0, 100.0f)) desc.dimension[0] = npAreaLight ? -dim0 : dim0;
                            if (ImGui::SliderFloat("disk light height", &dim1, 0.0, 100.0f)) desc.dimension[1] = npAreaLight ? -dim1 : dim1;
                            break;
                        default:
                            // do nothing
                            break;
                        }
                        // shadow toggle
                        ImGui::Checkbox("Allow Shadows", &desc.allowShadow);
                        light->reset(desc);
                        if (debugManager) debugManager->updateDebugLight(light);
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    if (ImGui::Checkbox("Handle Refraction and Rough Reflection", &options.refractionAndRoughReflection)) _renderPackDirty = true;

    if (ImGui::Checkbox("Use Precompiled Shader Parameters", &options.usePrecompiledShaderParameters)) _renderPackDirty = true;
    if (!options.usePrecompiledShaderParameters && ImGui::TreeNode("Debug")) {
        if (ImGui::TreeNode("Quality")) {
            if (ImGui::TreeNode("Ray Bounces")) {
                if (ph::rt::World::RayTracingRenderPackCreateParameters::isStochastic(options.rpmode)) {
                    ImGui::SliderInt("Max Diffuse Bounces", (int *) &recordParameters.maxDiffuseBounces, 0, 5);
                }
                ImGui::SliderInt("Max Specular Bounces", (int *) &recordParameters.maxSpecularBounces, 0, 10);
                ImGui::TreePop();
            }
            float minRayLengthPower = (std::log(recordParameters.minRayLength) / std::log(10.0f));
            if (ImGui::SliderFloat("Minimum Ray Length (10e-N)", &minRayLengthPower, 1.f, -10.f)) {
                recordParameters.minRayLength = std::pow(10.0f, (float) minRayLengthPower);
            }
            int diffBounce = (int) recordParameters.maxDiffuseBounces;
            int specBounce = (int) recordParameters.maxSpecularBounces;
            if (ImGui::SliderInt("Number of Diffuse Bounces", &diffBounce, 0, 8)) recordParameters.maxDiffuseBounces = (uint32_t) diffBounce;
            if (ImGui::SliderInt("Number of Specular Bounces", &specBounce, 0, 8)) recordParameters.maxSpecularBounces = (uint32_t) specBounce;
            ImGui::SliderFloat("Roughness Cutoff", &recordParameters.reflectionRoughnessCutoff, 0.0f, 1.0f);
            ImGui::SliderFloat("Saturation", &recordParameters.saturation, 0.0f, 5.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Transparency")) {
            auto & transparencySettings = recordParameters.transparencySettings;

            if (options.rpmode == Options::RenderPackMode::NOISE_FREE) {
                ImGui::SliderFloat("Fresnel Cutoff", &transparencySettings.fresnelCutoff, 0.0f, 1.0f);
            }

            if (ImGui::TreeNode("Approximate Backscattering")) {
                if (ImGui::BeginTable("Approximate Backscattering", 3)) {
                    ImGui::TableNextColumn();
                    if (ImGui::RadioButton("Off", transparencySettings.backscatterMode == 0)) { transparencySettings.backscatterMode = 0; }
                    ImGui::TableNextColumn();
                    if (ImGui::RadioButton("Thin", (transparencySettings.backscatterMode == 1))) { transparencySettings.backscatterMode = 1; }
                    ImGui::TableNextColumn();
                    if (ImGui::RadioButton("Volumetric", (transparencySettings.backscatterMode == 2))) { transparencySettings.backscatterMode = 2; }
                    ImGui::EndTable();
                }

                ImGui::Checkbox("Approximate Spectral Absorption", &transparencySettings.calculateAbsorptionTransmittance);
                ImGui::TreePop();
            }

            ImGui::SliderFloat("Alpha Cutoff", &transparencySettings.alphaCutoff, 0.0f, 1.0f);
            ImGui::SliderInt("Max Alpha Hits", (int *) &transparencySettings.alphaMaxHit, 0, 5);

            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Shadow")) {
            using ShadowMode = ph::rt::RayTracingRenderPack::ShadowMode;
            if (ImGui::BeginTable("Shadow Mode", (int) ph::rt::RayTracingRenderPack::ShadowMode::NUM_SHADOW_MODES)) {
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Rasterized", recordParameters.shadowMode == ShadowMode::RASTERIZED)) {
                    recordParameters.shadowMode = ShadowMode::RASTERIZED;
                }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Hybrid", recordParameters.shadowMode == ShadowMode::REFINED)) { recordParameters.shadowMode = ShadowMode::REFINED; }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Ray Traced", recordParameters.shadowMode == ShadowMode::RAY_TRACED)) {
                    recordParameters.shadowMode = ShadowMode::RAY_TRACED;
                }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("Debug", recordParameters.shadowMode == ShadowMode::DEBUG)) { recordParameters.shadowMode = ShadowMode::DEBUG; }
                ImGui::EndTable();
            }
            if (recordParameters.shadowMode == ShadowMode::RAY_TRACED) {
                auto & shadowSettings = recordParameters.transparencySettings.shadowSettings;
                if (ImGui::TreeNode("Transparent Shadows")) {
                    ImGui::Checkbox("Enabled", &shadowSettings.tshadowAlpha);
                    if (shadowSettings.tshadowAlpha) {
                        if (ImGui::BeginTable("Transparent Shadow Features", 4)) {
                            ImGui::TableNextColumn();
                            ImGui::Checkbox("Colored", &shadowSettings.tshadowColor);
                            ImGui::TableNextColumn();
                            ImGui::Checkbox("Textured", &shadowSettings.tshadowTextured);
                            ImGui::TableNextColumn();
                            ImGui::Checkbox("Fresnel", &shadowSettings.tshadowFresnel);
                            if (recordParameters.transparencySettings.backscatterMode > 0) {
                                // transparent shadow absorption only applies if backscattering is enabled
                                ImGui::TableNextColumn();
                                ImGui::Checkbox("Absorption", &shadowSettings.tshadowAbsorption);
                            }
                            ImGui::EndTable();
                        }
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        bool softwareRayQuery = !app().cp().rayQuery;
        bool noiseFree        = ph::rt::World::RayTracingRenderPackCreateParameters::isNoiseFree(options.rpmode);
        if (softwareRayQuery || noiseFree) {
            // for now, this heat view only works when we use in-house BVH traversal.
            ImGui::Checkbox("Show heat view", &recordParameters.enableHeatMap);
            if (!noiseFree && recordParameters.enableHeatMap) {
                ImGui::SliderFloat("Max # traversal steps", &recordParameters.maxNumTraversalSteps, 0.0, 300.0, "%.1f");
            }
        }

        // skinningManager.describeImguiUI(options.skinMode);
        // morphTargetManager.describeImguiUI(options.morphMode);

        if (ph::rt::World::RayTracingRenderPackCreateParameters::isStochastic(options.rpmode)) ptConfig.describeImguiUI();
        ImGui::TreePop();
    }
}
