#include "../common/modelviewer.h"

using namespace ph;
using namespace ph::va;
using namespace ph::rt;

struct ShadowScene : ModelViewer {

    struct Options : ModelViewer::Options {
        bool directional = false; // set to true to use directional light
        Options() {
            rpmode     = World::RayTracingRenderPackCreateParameters::SHADOW_TRACING;
            shadowMode = RayTracingRenderPack::ShadowMode::REFINED;
        }
    };

    ShadowScene(SimpleApp & app, const Options & o): ModelViewer(app, o) {
        // remove reflection textures
        recordParameters.irradianceMap = {};
        recordParameters.reflectionMap = {};

        // load model
        auto model  = "model/fence.obj";
        auto bbox   = addModelToScene({model});
        scene->name = model;
        PH_LOGI(s("model loaded. bounding box is\nmin:\n") << bbox.min() << "\nmax:\n" << bbox.max());

        // add floor
        _floorCenter     = bbox.center();
        _floorCenter.y() = bbox.min().y() + bbox.sizes().y() * 0.03f;
        _floorSize       = bbox.diagonal().norm() * 2.0f;
        addFloorPlaneToScene(_floorCenter, _floorSize);
        _sceneSize = bbox;
        _sceneSize.extend(Eigen::Vector3f(_floorCenter.x() - _floorSize / 2.0f, _floorCenter.y(), _floorCenter.z() - _floorSize / 2.0f));
        _sceneSize.extend(
            Eigen::Vector3f(_floorCenter.x() + _floorSize / 2.0f, bbox.max().y() + bbox.max().y() - _floorCenter.y(), _floorCenter.z() + _floorSize / 2.0f));

        // setup camera using bounding box w/o floor. So the camera can be focused on the fence.
        setupDefaultCamera(bbox);
        firstPersonController.setAngle({-PI / 6.0f, 0.f, 0.f});

        // setup the render packs
        setupShadowRenderPack();

        // setup initial light properties
        initialLight();
        if (o.directional)
            switchToDirectionalLight();
        else
            switchToPointLight();
    }

    const FrameTiming & update() override {
        using namespace std::chrono_literals;
        auto & t = ModelViewer::update();

        // animate light
        if (animated()) {
            constexpr auto cycle = std::chrono::duration_cast<std::chrono::microseconds>(30s).count();

            // Calculate what the current value of x should be.
            static const float initialX = _lightPosition.x();
            auto               angle    = PI * 2.0 * (double) (t.sinceBeginning.count() % cycle) / (double) cycle;
            float              xpos     = (float) (std::sin(angle) * _animationRadius);

            // Modify light position along the x axis.
            _lightPosition.x()                   = initialX + xpos;
            ph::rt::NodeTransform lightTransform = ph::rt::NodeTransform::Identity();
            lightTransform.translate(_lightPosition);
            _lightNode->setTransform(lightTransform);

            // Update light direction
            if (!_pointLight) {
                auto            desc      = _light->desc();
                Eigen::Vector3f direction = (_floorCenter - _lightPosition).normalized();
                desc.directional.setDir(direction);
                _light->reset(desc);
            }
        }

        return t;
    }

private:
    /// Starting position of the light.
    Eigen::Vector3f _lightPosition;

    Eigen::Vector3f _floorCenter;
    float           _floorSize;

    Eigen::AlignedBox3f _sceneSize;

    /// Node containing the light of the scene.
    ph::rt::Node * _lightNode;

    /// The light component.
    ph::rt::Light * _light;

    float _animationRadius = 50.0f;

    bool _pointLight = false;

    ph::rt::Material::TextureHandle _shadowMapCube, _shadowMap2D;

    void switchToPointLight() {
        _pointLight       = true;
        auto desc         = _light->desc();
        desc.type         = ph::rt::Light::Type::POINT;
        desc.dimension[0] = 0.f;
        desc.dimension[1] = 0.f;
        desc.point.range  = _floorSize;
        desc.setEmission(10.f, 10.f, 10.f);
        _light->reset(desc);
        _light->shadowMap = _shadowMapCube;
    }

    void switchToDirectionalLight() {
        _pointLight = false;
        auto desc   = _light->desc();
        desc.type   = ph::rt::Light::Type::DIRECTIONAL;
        desc.directional.setBBox(_sceneSize);
        desc.dimension[0] = 0.f;
        desc.dimension[1] = 0.f;
        desc.setEmission(10.f, 10.f, 10.f);
        _light->reset(desc);
        _light->shadowMap = _shadowMap2D;
    }

    // /// Creates default light.
    // void setupPointLight() {
    //     //Create the light's transform.
    //     ph::rt::NodeTransform lightTransform = ph::rt::NodeTransform::Identity();
    //     lightTransform.translate(_lightPosition);

    //     //Create the node that will contain the light.
    //     _lightNode = this->scene->addNode({
    //         .transform = lightTransform
    //     });

    //     float brightness = 10.0f;

    //     //Create a light.
    //     _light = this->scene->addLight({
    //         .node = _lightNode,
    //         .desc {
    //             .type = ph::rt::Light::Type::POINT,
    //             .emission = {brightness, brightness, brightness},
    //             .point {
    //                 .radius = 0.0f,
    //                 .range = _floorSize,
    //             }
    //         },
    //     });
    //     _light->shadowMap = textureCache->createShadowMapCube("point");

    //     //Give light to model viewer.
    //     lights.push_back(_light);

    //     _pointLight = true;
    // }

    void initialLight() {
        // Create the light's transform.
        ph::rt::NodeTransform lightTransform = ph::rt::NodeTransform::Identity();
        _lightPosition.x()                   = _floorCenter.x();
        _lightPosition.y()                   = 120.f;
        _lightPosition.z()                   = -50.0f;
        lightTransform.translate(_lightPosition);

        // Create the node that will contain the light.
        _lightNode = this->scene->addNode({.transform = lightTransform});

        // Create a light.
        _light = this->scene->addLight({
            .node = _lightNode,
        });

        // Create shadow map
        _shadowMapCube = textureCache->createShadowMapCube("point");
        _shadowMap2D   = textureCache->createShadowMap2D("directional");

        // Give light to model viewer.
        lights.push_back(_light);
    }
};
