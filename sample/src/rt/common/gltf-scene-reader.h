/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
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
    GLTFSceneReader(ph::AssetSystem * assetSystem, TextureCache * textureCache, sg::Graph * graph, skinning::SkinMap * skinnedMeshes,
                    MorphTargetManager * morphTargetManager, SceneBuildBuffers * sbb, bool createGeomLights);

    virtual ~GLTFSceneReader() = default;

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
     * The main scene nodes are being added to.
     */
    sg::Graph * _mainGraph;

    skinning::SkinMap *  _skinnedMeshes;
    MorphTargetManager * _morphTargetManager;
    SceneBuildBuffers *  _sbb;
    bool                 _createGeomLights;
};
