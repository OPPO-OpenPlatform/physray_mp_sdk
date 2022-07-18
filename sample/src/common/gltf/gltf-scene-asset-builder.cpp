/**
 *
 */
#include "pch.h"
#include "gltf-camera-builder.h"
#include "gltf-image-builder.h"
#include "gltf-light-builder.h"
#include "gltf-material-builder.h"
#include "gltf-mesh-builder.h"
#include "gltf-scene-asset-builder.h"
#include "gltf-scene-builder.h"
#include "physray-type-converter.h"
#include "mesh-utils.h"

#include <limits>
#include <string>
#include <vector>

namespace gltf {

GLTFSceneAssetBuilder::GLTFSceneAssetBuilder(ph::AssetSystem * assetSys, TextureCache * textureCache, ph::rt::World * world, const tinygltf::Model * model,
                                             const std::string & assetBaseDirectory, skinning::SkinMap * skinnedMeshes, MorphTargetManager * morphTargetManager)
    : _assetSys(assetSys), _textureCache(textureCache), _world(world), _model(model), _assetBaseDirectory(assetBaseDirectory), _accessorReader(model),
      _skinnedMeshes(skinnedMeshes), _morphTargetManager(morphTargetManager) {
    // convert all of the resource objects first.
    convertResources();
}

std::shared_ptr<SceneAsset> GLTFSceneAssetBuilder::build(const std::vector<ph::rt::Scene *> & scenes, ph::rt::Scene * mainScene) {
    // The result object this will return.
    std::shared_ptr<SceneAsset> sceneAsset {new SceneAsset()};

    // Pass the parameters to the results.
    sceneAsset->getScenes() = scenes;
    sceneAsset->setMainScene(mainScene);

    // Copy the resources used by sceneasset.
    sceneAsset->getMaterials()       = _materials;
    sceneAsset->getNameToMaterials() = _nameToMaterials;

    // Ensure results are big enough to fit everything we will be adding to them.
    sceneAsset->getCameras().resize(_model->cameras.size());
    sceneAsset->getLights().resize(_model->lights.size());
    sceneAsset->getNodes().resize(_model->nodes.size());

    // Create the builder for constructing the scenegraph.
    GLTFSceneBuilder sceneBuilder(_model);

    // Instantiate all the nodes of the scenes.
    sceneBuilder.build(sceneAsset.get(), scenes, mainScene);

    // Fetch the bounding box of the result.
    Eigen::AlignedBox<float, 3> & bounds = sceneAsset->getBounds();

    // Initialize the bounding box to an empty box.
    bounds.setEmpty();
    PH_ASSERT(bounds.isEmpty());

    // Give the nodes any attachments they may have.
    connectSceneGraphs(sceneAsset.get());

    // If the initial bounds did not change, then that probably
    // means that this gltf file apparently did not have any mesh nodes.
    // In such an event, set bounds to zero.
    if (bounds.isEmpty()) {
        bounds.min() = {0.f, 0.f, 0.f};
        bounds.max() = {0.f, 0.f, 0.f};
    }

    return sceneAsset;
}

void GLTFSceneAssetBuilder::convertResources() {
    // Prepare the images for use by ph.
    // Stores the images in the gltf file.
    // We only need this long enough to load the materials.
    PH_LOGI("[GLTF] converting images....");
    std::vector<ph::RawImage> images;
    convertImages(images);

    // Create the materials used to color the mesh views.
    PH_LOGI("[GLTF] converting materials....");
    convertMaterials(images);

    // Parse the PhysRay meshes.
    PH_LOGI("[GLTF] converting meshes....");
    convertMeshes();
}

void GLTFSceneAssetBuilder::convertImages(std::vector<ph::RawImage> & images) {
    // Load all the backing images.
    GLTFImageBuilder imageBuilder(_assetSys, _assetBaseDirectory);

    // Instantiate all the PhysRay image objects that will be loaded into.
    images.resize(_model->images.size());

    // Iterate all the gltf images.
    for (std::size_t index = 0; index < _model->images.size(); ++index) {
        // Fetch the image to be loaded.
        const tinygltf::Image & image = _model->images[index];

        // Load the image into the matching PhysRay object.
        imageBuilder.build(image, images[index]);
    }
}

void GLTFSceneAssetBuilder::convertMaterials(const std::vector<ph::RawImage> & images) {
    // Make sure collection of materials is big enough to hold everything.
    _materials.reserve(_model->materials.size());

    // Create a builder to create each material.
    GLTFMaterialBuilder builder(_textureCache, _world, _model, &images);

    // Iterate materials.
    for (std::size_t materialId = 0; materialId < _model->materials.size(); ++materialId) {
        // Fetch the material to be converted.
        const tinygltf::Material & material = _model->materials[materialId];

        // Convert the material.
        ph::rt::Material * phMaterial = builder.build(material);

        // Save the result to the list of all materials, giving it the same index as its id.
        _materials.push_back(phMaterial);

        // Save the material to its name.
        _nameToMaterials[material.name].insert(phMaterial);
    }
}

void GLTFSceneAssetBuilder::convertMeshes() {
    // Ensure there is a slot for each mesh.
    _meshToPrimitives.resize(_model->meshes.size());

    // Create the object that will build each mesh.
    GLTFMeshBuilder builder(_world, _model, _skinnedMeshes, _morphTargetManager->getMorphTargets());

    // Iterate all meshes.
    for (std::size_t meshId = 0; meshId < _model->meshes.size(); ++meshId) {
        // Fetch the tinygltf mesh to be converted.
        const tinygltf::Mesh & mesh = _model->meshes[meshId];

        // Get this mesh's list of converted PhysRay meshes.
        std::vector<PrimitiveData> & primitives = _meshToPrimitives[meshId];

        // Ensure there is enough space for all primitives.
        primitives.reserve(mesh.primitives.size());

        // Iterate the mesh's list of primitives.
        for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex) {
            // Fetch the primitive to be converted.
            const tinygltf::Primitive & primitive = mesh.primitives[primitiveIndex];

            std::string primitiveName = ph::formatstr("%s #%zu", mesh.name.c_str(), primitiveIndex);

            PrimitiveData primitiveData;

            // If conversion succeeded, record it.
            if (builder.build(primitiveName.c_str(), primitive, primitiveData.mesh, primitiveData.bbox)) {
                // Store material pointer
                primitiveData.material = getMaterial(primitive.material);

                // Save it to the set of PhysRay meshes for this tiny gltf mesh.
                primitives.push_back(primitiveData);

                // Add morph weights
                if (_morphTargetManager) {
                    std::vector<float> floatWeights;
                    size_t             numWeights = mesh.weights.size();
                    floatWeights.resize(numWeights);
                    for (size_t i = 0; i < numWeights; i++) {
                        floatWeights[i] = float(mesh.weights[i]);
                    }
                    _morphTargetManager->setWeights(primitiveData.mesh, floatWeights);
                }

                // If conversion failed, fire a warning and skip.
            } else {
                PH_LOGW("Primitive number %d of mesh %d not supported.", primitiveIndex, meshId);
            }
        }
    }
}

ph::rt::Material * GLTFSceneAssetBuilder::getDefaultMaterial() {
    // If not initialized, create it.
    if (_defaultMaterial == NULL) {
        // Create the default PhysRay material as defined by the GLTF specification:
        // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0/schema
        ph::rt::Material::Desc phMaterialDesc;

        // The default value of
        // pbrMetallicRoughness.metallicFactor is 1.0.
        phMaterialDesc.metalness = 1.0;

        // The default value of
        // pbrMetallicRoughness.roughnessFactor is 1.0.
        phMaterialDesc.roughness = 1.0f;

        // The default value of
        // pbrMetallicRoughness.baseColorFactor is (1, 1, 1).
        phMaterialDesc.setAlbedo(1.0f, 1.0f, 1.0f);

        // material.emissiveFactor defaults to (0, 0, 0),
        // so leave phMaterial.emission at zero.

        // Create the material.
        _defaultMaterial = _world->createMaterial(phMaterialDesc);
    }

    return _defaultMaterial;
}

ph::rt::Material * GLTFSceneAssetBuilder::getMaterial(int materialId) {
    // Fetch the material for this primitive.
    // If it has a material reference.
    if (materialId != -1) {
        // Return the material with the given id.
        return _materials[materialId];

        // If this does not have a selected material.
    } else {
        // Return the default material.
        return getDefaultMaterial();
    }
}

void GLTFSceneAssetBuilder::connectSceneGraphs(SceneAsset * sceneAsset) {
    // Get the list of all PhysRay nodes.
    std::vector<ph::rt::Node *> & phNodes = sceneAsset->getNodes();

    // Iterate all nodes.
    for (std::size_t nodeId = 0; nodeId < phNodes.size(); ++nodeId) {
        // Get the PhysRay node to be processed.
        ph::rt::Node * phNode = phNodes[nodeId];

        // If this node is empty for whatever reason, skip it.
        // This will most likely happen if there were multiple scenes
        // and not all of them were loaded.
        if (phNode == nullptr) { continue; }

        // Get the matching tinygltf object for this node.
        const tinygltf::Node & node = _model->nodes[nodeId];

        // Attach everything that should be added to the node.
        // Give the node its camera if it has one.
        addNodeCamera(sceneAsset, phNode, node.camera);

        // If this node has its own primitives, add them.
        addMeshPrimitives(sceneAsset, phNode, node);

        // Apply any of the extensions this node is using.
        processNodeExtensions(sceneAsset, phNode, node);
    }
}

void GLTFSceneAssetBuilder::addNodeCamera(SceneAsset * sceneAsset, ph::rt::Node * phNode, int cameraId) {
    // If this node doesn't have a camera, there is nothing to do.
    if (cameraId == -1) { return; }

    // Fetch the camera.
    const tinygltf::Camera & camera = _model->cameras[cameraId];

    // Build the camera.
    GLTFCameraBuilder builder(&phNode->scene());
    ph::rt::Camera *  phCamera = builder.build(camera, phNode);

    // Save the camera to the list of cameras.
    sceneAsset->getCameras()[cameraId] = phCamera;

    // Save the camera to its name.
    sceneAsset->getNameToCameras()[camera.name].insert(phCamera);
}

static Eigen::AlignedBox3f calculateWorldSpaceBoundingBox(const ph::rt::NodeTransform & transform, const Eigen::AlignedBox3f bbox) {
    // Total number of corners in a box.
    constexpr size_t cornerCount = 8;

    // Grab the coordinates of the 8 corners of the bounding box.
    Eigen::Vector3f corners[cornerCount] = {
        bbox.corner(Eigen::AlignedBox3f::CornerType::BottomLeftFloor), bbox.corner(Eigen::AlignedBox3f::CornerType::BottomRightFloor),
        bbox.corner(Eigen::AlignedBox3f::CornerType::TopLeftFloor),    bbox.corner(Eigen::AlignedBox3f::CornerType::TopRightFloor),
        bbox.corner(Eigen::AlignedBox3f::CornerType::BottomLeftCeil),  bbox.corner(Eigen::AlignedBox3f::CornerType::BottomRightCeil),
        bbox.corner(Eigen::AlignedBox3f::CornerType::TopLeftCeil),     bbox.corner(Eigen::AlignedBox3f::CornerType::TopRightCeil)};

    // Transform the corners individually.
    for (std::size_t index = 0; index < cornerCount; ++index) {
        corners[index] = transform * corners[index];
    }

    // Use the transformed corners to calculate the transformed box.
    // Initialize it with the first corner.
    Eigen::AlignedBox3f result;
    result.min() = corners[0];
    result.max() = corners[0];

    // Extend the box by the combination of all corners
    // beyond the first (which the box has already been initialized to).
    for (std::size_t index = 1; index < cornerCount; ++index) {
        result.extend(corners[index]);
    }

    // done
    return result;
}

void GLTFSceneAssetBuilder::addMeshPrimitives(SceneAsset * sceneAsset, ph::rt::Node * phNode, const tinygltf::Node & node) {
    // If this node isn't a mesh, then there is nothing for us to do.
    if (node.mesh == -1) { return; }

    // Determine if this node has a skin.
    bool hasSkin = node.skin != -1;

    // The nodes making up this mesh view's skeleton (if it has one).
    std::vector<ph::rt::Node *> joints;

    // The inverse bind matrices corresponding to each joint.
    std::vector<Eigen::Matrix4f> inverseBindMatrices;

    // If this node has a skin.
    if (hasSkin) {
        // Fetch the skin being parsed.
        const tinygltf::Skin & skin = _model->skins[node.skin];

        // Fetch all the nodes making up this skin's skeleton.
        for (auto jointNodeId : skin.joints) {
            // Fetch the actual PhysRay node implementing this joint.
            ph::rt::Node * n = sceneAsset->getNodes()[jointNodeId];

            // Not expecting this to be null.
            PH_REQUIRE(n != nullptr);

            // Add to the skeleton
            joints.push_back(n);
        }

        // If this defines inverse bind matrices.
        if (skin.inverseBindMatrices != -1) {
            // Read the matrices.
            _accessorReader.readAccessor(skin.inverseBindMatrices, inverseBindMatrices);
        }

        // If the skin defines a skeletal root.
        if (skin.skeleton != -1) {
            // Base the skinned mesh view's transform on the skeletal root.
            phNode = sceneAsset->getNodes()[skin.skeleton];
        } else {
            // Set world transform to zero so that skinning transforms are applied correctly
            phNode->setWorldTransform(ph::rt::NodeTransform::make(Eigen::Vector3f::Zero(), Eigen::Quaternionf::Identity(), Eigen::Vector3f::Ones()));
        }
    }

    // Get the bounding box so we can update
    // it according to the current primitives.
    Eigen::AlignedBox3f & bounds = sceneAsset->getBounds();

    // Fetch the scene object that will be used as a factory for creating the mesh views.
    ph::rt::Scene & scene = phNode->scene();

    // Get this mesh's list of converted PhysRay meshes.
    std::vector<PrimitiveData> & primitiveDatas = _meshToPrimitives[node.mesh];

    // Iterate all primitives in this mesh.
    for (std::size_t primitiveIndex = 0; primitiveIndex < primitiveDatas.size(); ++primitiveIndex) {
        // Get the data about the mesh to be instantiated.
        // Fetch the primitive for this mesh.
        const PrimitiveData & primitiveData = primitiveDatas[primitiveIndex];

        // Create a mesh view for this primitive.
        scene.addMeshView({
            .node     = phNode,
            .mesh     = primitiveData.mesh,
            .material = primitiveData.material,
            //.groupId = node.mesh,
        });

        if (_skinnedMeshes != nullptr) {
            auto & skinData              = _skinnedMeshes->find(primitiveData.mesh)->second;
            skinData.jointMatrices       = joints;
            skinData.inverseBindMatrices = inverseBindMatrices;
        }

        // Calculate this primitive's bounds after the transform is applied
        // to the mesh's original bounds.
        auto primitiveBounds = calculateWorldSpaceBoundingBox(phNode->worldTransform(), primitiveData.bbox);

        // Add this primitive's bounds to the total.
        if (bounds.isEmpty())
            bounds = primitiveBounds;
        else
            bounds.extend(primitiveBounds);
    }
}

void GLTFSceneAssetBuilder::processNodeExtensions(SceneAsset * sceneAsset, ph::rt::Node * phNode, const tinygltf::Node & node) {
    // Iterate all of this node's extensions.
    for (auto & nameToValue : node.extensions) {
        // If this is the light extension.
        if (nameToValue.first == "KHR_lights_punctual") {
            // Fetch the light id.
            const tinygltf::Value & value   = nameToValue.second;
            const tinygltf::Value & lightId = value.Get("light");

            // If this has a valid light index.
            if (lightId.IsInt()) { addNodeLight(sceneAsset, phNode, lightId.GetNumberAsInt()); }

            // If this is an unrecognized extension.
        } else {
            PH_LOGW("Node has unsupported extension '%s'", nameToValue.first.c_str());
        }
    }
}

void GLTFSceneAssetBuilder::addNodeLight(SceneAsset * sceneAsset, ph::rt::Node * phNode, int lightId) {
    // If this node doesn't have a light, there is nothing to do.
    if (lightId == -1) { return; }

    // Fetch the light.
    const tinygltf::Light & light = _model->lights[lightId];

    // Build the light.
    GLTFLightBuilder builder(_textureCache);
    ph::rt::Light *  phLight = builder.build(light, phNode);

    // Save the light to the list of lights.
    sceneAsset->getLights()[lightId] = phLight;

    // Save the light to its name.
    sceneAsset->getNameToLights()[light.name].insert(phLight);
}

} // namespace gltf
