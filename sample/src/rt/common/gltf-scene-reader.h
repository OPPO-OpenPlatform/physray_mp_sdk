/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-scene-reader.h
 *
 * Version: 2.0
 *
 * Date : Feb 2023
 *
 * Author: Computing & Graphics Research Institute
 *
 * ------------------ Revision History: ---------------------
 *  <version>  <date>  <author>  <desc>
 *
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>
#include "scene-asset.h"
#include "texture-cache.h"
#include "skinning.h"
#include "morphtargets.h"

#include <cstdint>
#include <istream>
#include <memory>
#include <vector>

// forward declarations
class SceneBuildBuffers;

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
                    skinning::SkinMap * skinnedMeshes, MorphTargetManager * morphTargetManager, SceneBuildBuffers * sbb, bool createGeomLights);

    virtual ~GLTFSceneReader() = default;

    ph::AssetSystem * getAssetSystem() { return _assetSystem; }

    TextureCache * getTextureCache() { return _textureCache; }

    ph::rt::World * getWorld() { return _world; }

    ph::rt::Scene * getMainScene() { return _mainScene; }

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

    skinning::SkinMap *  _skinnedMeshes;
    MorphTargetManager * _morphTargetManager;
    SceneBuildBuffers *  _sbb;
    bool                 _createGeomLights;
};
