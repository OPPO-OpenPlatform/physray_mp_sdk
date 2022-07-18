/**
 *
 */
#pragma once

#include <ph/rt.h>
#include "scene-asset.h"
#include "texture-cache.h"
#include "skinning.h"
#include "morphtargets.h"

#include <cstdint>
#include <istream>
#include <memory>
#include <vector>

/**
 * Converts GLTF files into scenes and adds them to ph.
 */
class GLTFSceneReader {
public:
    /**
     *
     * @param assetSystem A pointer to the asset system this will
     * be reading GLTF scene files from from.
     * @param textureCache The object used to load and cache textures.
     * @param world The world used to generate objects.
     * @param mainScene The main scene nodes will be added to.
     */
    GLTFSceneReader(ph::AssetSystem * assetSystem, TextureCache * textureCache, ph::rt::World * world, ph::rt::Scene * mainScene,
                    skinning::SkinMap * skinnedMeshes = nullptr, MorphTargetManager * morphTargetManager = nullptr);

// While we may yet have a need for this in the future,
// supporting multiple scenes has been disabled for
// simplicity's sake for the time being.
// If we turn out to need multiple scenes, just delete
// the below #if in the header and the matching one in the cpp.
// You will also need to implement some of the TODO code.
#if false
    /**
     * 
     * @param assetSystem A pointer to the asset system this will
     * be reading GLTF scene files from from.
     * @param world The world used to generate objects.
     * @param scenes GLTF files have a list of scenes.
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
     */
    GLTFSceneReader(
        ph::AssetSystem* assetSystem,
        ph::rt::World* world,
        std::vector<ph::rt::Scene*>& scenes
        );
    
    /**
     * 
     * @param A pointer to the asset system this will
     * be reading GLTF scene files from from.
     * @param world The world used to generate objects.
     * @param mainScene The main scene nodes will be added to.
     * @param scenes GLTF files have a list of scenes.
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
     */
    GLTFSceneReader(
        ph::AssetSystem* assetSystem,
        ph::rt::World* world,
        std::vector<ph::rt::Scene*>& scenes,
        ph::rt::Scene* mainScene
        );
#endif

    /**
     *
     */
    virtual ~GLTFSceneReader() = default;

    /**
     * The asset system being read from.
     */
    ph::AssetSystem * getAssetSystem() { return _assetSystem; }

    /**
     * @return The object used to load and cache textures.
     */
    TextureCache * getTextureCache() { return _textureCache; }

    /**
     * @return The world being used to generate objects.
     */
    ph::rt::World * getWorld() { return _world; }

    /**
     * @return The main scene nodes are being added to.
     */
    ph::rt::Scene * getMainScene() { return _mainScene; }

    /**
     * @return GLTF files have a list of scenes.
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
     */
    std::vector<ph::rt::Scene *> & getScenes() { return _scenes; }

    /**
     * Reads the given path from the given assetSystem
     * as a GLTF file.
     * @param assetPath The asset path to the model file to be loaded.
     * @return An object containing information about what
     * was loaded into world.
     */
    std::shared_ptr<const SceneAsset> read(const std::string & assetPath);

private:
    /**
     * Asset system this is reading gltf files from.
     */
    ph::AssetSystem * _assetSystem;

    /**
     * The object used to load and cache textures.
     */
    TextureCache * _textureCache;

    /**
     * The world being used to generate objects.
     */
    ph::rt::World * _world;

    /**
     * The main scene nodes are being added to.
     */
    ph::rt::Scene * _mainScene;

    /**
     * List of all scenes to load gltf objects into
     * if you have more scenes than the main scene.
     * The main gltf scene is loaded only once,
     * even if both its index in this list and
     * mainScene are both set.
     */
    std::vector<ph::rt::Scene *> _scenes;

    skinning::SkinMap *  _skinnedMeshes;
    MorphTargetManager * _morphTargetManager;
};
