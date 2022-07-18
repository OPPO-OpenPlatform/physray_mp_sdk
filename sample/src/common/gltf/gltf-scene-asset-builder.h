/**
 *
 */
#pragma once

#include <ph/rt.h>

#include "accessor-reader.h"
#include "../scene-asset.h"
#include "../texture-cache.h"
#include "../skinning.h"
#include "../morphtargets.h"

#include <filesystem>

namespace gltf {

/**
 * Converts tiny gltf objects to their PhysRay equivalents and assembles
 * them into a scene asset.
 */
class GLTFSceneAssetBuilder {
public:
    /**
     *
     * @param assetSys The main asset system to load files from.
     * @param textureCache The object used to load and cache textures.
     * @param world The world used to generate objects.
     * @param model The tinygltf model who's items are
     * being instantiated in world.
     * @param assetBaseDirectory The path holding the directory where
     * the gltf file is located. Is used to resolve relative paths.
     */
    GLTFSceneAssetBuilder(ph::AssetSystem * assetSys, TextureCache * textureCache, ph::rt::World * world, const tinygltf::Model * model,
                          const std::string & assetBaseDirectory, skinning::SkinMap * skinnedMeshes = nullptr,
                          MorphTargetManager * morphTargetManager = nullptr);

    /**
     * Destructor.
     */
    virtual ~GLTFSceneAssetBuilder() = default;

    /**
     * @return The world used to create new objects.
     */
    ph::rt::World * getWorld() { return _world; }

    /**
     * @return The tinygltf model who's items are being instantiated in world.
     */
    const tinygltf::Model * getModel() const { return _model; }

    /**
     * @return Path where glt file is being read from,
     * is used to build relative paths.
     */
    const std::filesystem::path & getAssetBaseDirectory() const { return _assetBaseDirectory; }

    /**
     * Generates all the nodes for the scenes, saves them
     * and the associated resource objects to a SceneAsset,
     * then returns the newly created scene asset.
     * @param scenes The list of scenes nodes will be added to.
     * GLTF files have a list of scenes.
     * This class will load each scene into
     * the scene at the matching index on this list
     * if available.
     *
     * If desired, it is in fact possible to add the
     * same scene multiple times.
     *
     * The main scene is loaded when we reach its index.
     * This class will load the matching GLTF scene
     * into the mainScene variable if set to anything other
     * than NULL, and use the one loaded at the matching
     * index in this list if not.
     * If the GLTF file does not specify a main scene,
     * then index zero will be treated as main scene.
     * @param mainScene The main scene nodes will be added to.
     */
    std::shared_ptr<SceneAsset> build(const std::vector<ph::rt::Scene *> & scenes, ph::rt::Scene * mainScene);

private:
    /**
     * Contains converted data about a given tiny gltf primitive.
     */
    struct PrimitiveData {
        /**
         * Jedi mesh generated from this primitive.
         */
        ph::rt::Mesh * mesh = nullptr;

        /**
         * The material for this primitive.
         */
        ph::rt::Material * material = nullptr;

        /**
         * bounding box of the mesh
         */
        Eigen::AlignedBox3f bbox;
    };

    /**
     * The main asset system to load files from.
     */
    ph::AssetSystem * _assetSys;

    /**
     * The object used to load and cache textures.
     */
    TextureCache * _textureCache;

    /**
     * The world everything is being instantiated in.
     */
    ph::rt::World * _world;

    /**
     * The tinygltf model who's items are being instantiated in world.
     */
    const tinygltf::Model * _model;

    /**
     * Base directory where the model file came from.
     */
    std::filesystem::path _assetBaseDirectory;

    /**
     * Used to read binary data from the model.
     */
    AccessorReader _accessorReader;

    skinning::SkinMap *  _skinnedMeshes;
    MorphTargetManager * _morphTargetManager;
    /**
     * Maps each tiny gltf material id to its PhysRay equivelant.
     */
    std::vector<ph::rt::Material *> _materials;

    /**
     * Maps names to a set of materials with that name.
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Material *>> _nameToMaterials;

    /**
     * Material to assign if a primitive doesn't have one.
     * Since many gltf files won't need it,
     * this value is lazy initialized, remaining NULL
     * unless it does in fact turn out to be needed.
     */
    ph::rt::Material * _defaultMaterial = nullptr;

    /**
     * Maps each tiny gltf mesh id to an array of information of each
     * primitive for that mesh. That second array matches each of the
     * mesh's primitives to a struct containing information about it,
     * allowing it to be instantiated as a node.
     */
    std::vector<std::vector<PrimitiveData>> _meshToPrimitives;

    /**
     * Converts gltf resource objects.
     */
    void convertResources();

    /**
     * Converts all tiny gltf images to their PhysRay equivelant and then
     * saves them to the given collection
     * @param images The collection the generated images will be saved to.
     */
    void convertImages(std::vector<ph::RawImage> & images);

    /**
     * Converts all tiny gltf materials to their PhysRay equivelant and then
     * saves them to the materials variable.
     * @param images Collection of gltf images loaded to ph,
     * indexed by image id.
     */
    void convertMaterials(const std::vector<ph::RawImage> & images);

    /**
     * Converts all tiny gltf meshes to a list of the equivelant PhysRay meshes.
     * This must be called AFTER convertMaterials() so that each mesh can
     * be associated with its desired material.
     */
    void convertMeshes();

    /**
     * @return The default material used if no material is provided,
     * lazy initializes it if necessary.
     */
    ph::rt::Material * getDefaultMaterial();

    /**
     * @param materialId The unique number identifying this material,
     * -1 if default material is desired
     * @return The material with the given id,
     * returns default material if materialId is -1.
     */
    ph::rt::Material * getMaterial(int materialId);

    /**
     * Iterates all nodes created for all scenes and adds items
     * connected to them, such as mesh views, cameras, lights, skins, etc.
     */
    void connectSceneGraphs(SceneAsset * sceneAsset);

    /**
     * Instantiates the mesh nodes associated with this node.
     * Does nothing if this node does not have a mesh.
     * @param sceneAsset
     * @param phNode The node being used as a parent for all the primitives.
     * @param node The tiny gltf node we are generating primitives for.
     */
    void addMeshPrimitives(SceneAsset * sceneAsset, ph::rt::Node * phNode, const tinygltf::Node & node);

    /**
     * Attaches camera to this node.
     * @param phNode The node camera is being attached to.
     * @param cameraId Id of the camera for this node. If -1,
     * this method does not do anything.
     */
    void addNodeCamera(SceneAsset * sceneAsset, ph::rt::Node * phNode, int cameraId);

    /**
     * Applies the effects of any extensions to the given node.
     * @param worldToNode Transform used to convert from world to node space.
     * @param groupNode The PhysRay node being used as a parent for all the primitives.
     * @param node The tiny gltf node we are processing the extensions for.
     */
    void processNodeExtensions(SceneAsset * sceneAsset, ph::rt::Node * phNode, const tinygltf::Node & node);

    /**
     * Attaches light to this node.
     * @param phNode The node camera is being attached to.
     * @param lightId Id of the light for this node. If -1,
     * this method does not do anything.
     */
    void addNodeLight(SceneAsset * sceneAsset, ph::rt::Node * phNode, int lightId);
};

} // namespace gltf
