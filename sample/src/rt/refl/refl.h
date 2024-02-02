/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#include "../common/modelviewer.h"

#include <ph/rt-utils.h>

using namespace ph;
using namespace ph::va;

struct ReflScene : ModelViewer {
    struct Options : ModelViewer::Options {
        int sceneIndex = 0;
        Options() {
            clearColorOnMainPass = true;
            clearDepthOnMainPass = true;
        }
    };

    ReflScene(SimpleApp & app, const Options & o): ModelViewer(app, o), _rec(app.loop()) {
        initSprites();
        selectScene(o.sceneIndex);
    }

    void selectScene(int sceneIndex) {
        ModelViewer::resetScene(); // this will clear old scene data.
        switch (sceneIndex) {
        case 0:
        default:
            scene0();
            break;
        case 1:
            scene1();
            break;
        }
    }

protected:
    void recreateMainRenderPack() override {
        // create render target images. set initial layout to shader resource.
        auto ci = ImageObject::CreateInfo {}
                      .set2D(1280, 720)
                      .setFormat(VK_FORMAT_R8G8B8A8_UNORM)
                      .setUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        _colorImage = _fac->createImage(ph::rps::Image::CreateParameters1 {ci}, "color0");
        rps::SynchronousCommandRecorder(app().dev().graphicsQ()).syncExec([&](auto & rec) { _colorImage->cmdSetAccess(rec, rps::Image::SR()); });

        // create render pack
        auto cp = rt::render::ReflectionRenderPack::CreateParameters {world}.setTarget(VK_FORMAT_R8G8B8A8_UNORM, ci.extent.width, ci.extent.height,
                                                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        _rPack.reset(rt::render::ReflectionRenderPack::create(cp));
    }

    void recordOffscreenPass(const PassParameters & p) override {
        ModelViewer::recordOffscreenPass(p);

        // update command buffer of the recorder.
        _rec.setCommands(p.cb);

        // render the scene with reflection render pack
        auto & camera    = cameras[selectedCameraIndex];
        auto   rp        = rt::render::ReflectionRenderPack::RecordParameters {};
        rp.scene         = scene;
        rp.projMatrix    = camera.calculateProj(1280.f, 720.f);
        rp.viewMatrix    = camera.worldTransform().inverse().matrix();
        rp.commandBuffer = p.cb;
        rp.targetImage   = _colorImage->desc().image;
        rp.targetView    = _colorImage->getVkImageView(va::firstSubImageRange(0));
        rp.ambientLight << .1f, .1f, .1f;
        rp.background << rp.ambientLight, 0.f;
        _rPack->record(rp);

        // Notify the color image that the render pack class has transferred the image into render target layout.
        _colorImage->syncAccess(rps::Image::RT());

        // Set the image back to shader read layout.
        _colorImage->cmdSetAccess(_rec, rps::Image::SR());

        // Generate the sprites that renders the color image to screen. We have to do this out of the main color
        // pass, since changing buffer access via barrier is not allowed within a graphics render pass in Vulkan.
        _sprites = _spr->begin({&_rec, {_colorImage}});
        _sprites->append({});
        _sprites->end();
    }

    void recordMainColorPass(const PassParameters &) override { _spr->record(_sprites); }

private:
    rps::RenderLoopCommandRecorder                _rec;
    rps::Ref<ph::rps::Factory>                    _fac;
    std::unique_ptr<rps::SpriteRenderer>          _spr;
    std::shared_ptr<rps::SpriteRenderer::Batch>   _sprites;
    std::unique_ptr<render::ReflectionRenderPack> _rPack;
    rps::Ref<ph::rps::Image>                      _colorImage;

private:
    void initSprites() {
        _fac = ph::rps::Factory::createFactory({&app().dev().graphicsQ()});

        const char * spriteColor = R"(
            vec4 spriteColor(vec4 color, vec2 texCoord) {
                vec4  t      = texture(tex, texCoord);
                vec3  refl   = t.rgb;
                float shadow = t.a;
                if (any(greaterThan(refl, vec3(0.0)))) {
                    color.rgb *= refl;
                } else {
                    color.rgb *= shadow;
                }
                return color;
            }
        )";

        _spr =
            std::make_unique<rps::SpriteRenderer>(rps::SpriteRenderer::ConstructParameters {_fac.get(), mainColorPass()}.setSpriteColorFunction(spriteColor));
    }

    void scene0() {
        // create materials
        auto baseDesc = []() { return rt::Material::Desc {}; };
        // auto red      = world->create("red",      baseDesc().setAlbedo(1.f, 0.f, 0.f));
        // auto green    = world->create("green",    baseDesc().setAlbedo(0.f, 1.f, 0.f));
        auto yellow  = world->create("yellow", baseDesc().setAlbedo(1.f, 1.f, 0.f));
        auto magenta = world->create("magenta", baseDesc().setAlbedo(1.f, 0.f, 1.f));
        // auto mirror  = world->create("mirror", baseDesc().setAlbedo(0.f, 1.0f, 1.f).setMetalness(1.f).setRoughness(0.f));
        // auto glass   = world->create("glass", baseDesc().setMetalness(0.f).setRoughness(0.f).setOpaqueness(0.f).setAlbedo(1.0f, .3f, 1.f));

        // Add a floor with white color.
        addBox("floor", 10.f, 1.f, 10.f, lambertian);

        // Add a yellow sphere on the center of the floor.
        addIcosahedron("sphere", 1.f, 2, yellow, nullptr, sg::Transform::make({0.f, 2.f, 0.f}));

        // Add a magenta mirror behind the sphere.
        auto mirrorNode = addBox("mirror", 5.f, 5.f, .1f, magenta, nullptr, sg::Transform::make({0.f, 2.f, -3.f}));

        // mark the mirror as a reflection surface.
        auto mirrorModel = mirrorNode->model();
        mirrorModel->flags |= rt::Model::REFLECTIVE;

        // Add a light source.
        addPointLight({-20.f, 20.f, 20.f}, 100.f, {1.f, 1.f, 1.f});

        // Setup camera
        Eigen::AlignedBox3f bbox;
        bbox.min() << -10.f, -10.f, -10.f;
        bbox.max() << 10.f, 10.f, 10.f;
        setupDefaultCamera(bbox);
        firstPersonController.setOrbitalCenter({0.144f, 2.76f, -1.81f});
        firstPersonController.setOrbitalRadius(4.84f);
        firstPersonController.setAngle({-0.417f, 0.641f, 0.f});
    }

    void scene1() {
        // another test scene
    }
};
