/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-scene-reader.cpp
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
#include "pch.h"
#include "gltf-scene-reader.h"
#include "gltf/animations/gltf-animation-builder.h"
#include "gltf/gltf-scene-asset-builder.h"
#include "gltf/physray-type-converter.h"
#include <filesystem>

GLTFSceneReader::GLTFSceneReader(ph::AssetSystem * assetSystem, TextureCache * textureCache, ph::rt::World * world, ph::rt::Scene * mainScene,
                                 skinning::SkinMap * skinnedMeshes, MorphTargetManager * morphTargetManager, SceneBuildBuffers * sbb, bool createGeomLights)
    : _assetSystem(assetSystem), _textureCache(textureCache), _world(world), _mainScene(mainScene), _skinnedMeshes(skinnedMeshes),
      _morphTargetManager(morphTargetManager), _sbb(sbb), _createGeomLights(createGeomLights) {
    //
}

std::shared_ptr<const SceneAsset> GLTFSceneReader::read(const std::string & assetPath) {
    // If file does not exist.
    if (!_assetSystem->exist(assetPath.c_str())) { PH_THROW("GLTF file \"%s\" does not exist.", assetPath.c_str()); }

    // Load the full model to memory.
    auto asset = _assetSystem->load(assetPath.c_str()).get();

    // Fetch the resulting data.
    const auto & assetData = asset.content.v;

    // If file is too small, fail.
    if (assetData.size() <= 4) {
        PH_THROW("GLTF file \"%s\" is only %zu bytes and therefore too small "
                 "to be a valid file.",
                 assetPath.c_str(), assetData.size());
    }

    // Records whether file is a text or binary gltf file.
    // Check the contents for the magic number indicating a binary gltf file.
    // Since the text format is json, it is guaranteed not to have this magic number.
    bool isBinary = assetData[0] == 'g' && assetData[1] == 'l' && assetData[2] == 'T' && assetData[3] == 'F';

    // Create an instance of tiny gltf to parse the asset.
    tinygltf::TinyGLTF tinyGltf;

    // Create the callback tiny gltf will use to load images.
    auto imageLoader = [](tinygltf::Image * image,
                          const int,     // image_idx,
                          std::string *, // err,
                          std::string *, // warn,
                          int,           // req_width,
                          int,           // req_height,
                          const unsigned char * bytes, int size, void *) -> bool {
        // copy image data to image structure, as-is. no decoding.
        image->as_is = true;
        image->image.resize((size_t) size);
        memcpy(image->image.data(), bytes, (size_t) size);
        return true;
    };

    // Wrap the string asset path into a file path.
    std::filesystem::path filesystemAssetPath(assetPath);

    // Since we need to read the original file ourselves
    // rather than letting tiny gltf handle it,
    // determine the base directory of the GLTF asset we are loading.
    const std::string assetBaseDirectory = filesystemAssetPath.parent_path().string();

    // Set the tiny gltf parser to use the given asset system.
    tinyGltf.SetFsCallbacks(gltf::JediTypeConverter::toFsCallbacks(_assetSystem));
    tinyGltf.SetImageLoader(imageLoader, _assetSystem);

    // Holds the loaded gltf model.
    tinygltf::Model model;

    // These variables record any problems that occured while loading.
    std::string err;
    std::string warn;

    // Records whether model load was successful.
    bool success;

    // If this is a binary gltf file.
    PH_LOGI("[GLTF] Loading GLTF file %s....", assetPath.c_str());
    if (isBinary) {
        // Read the binary file
        success = tinyGltf.LoadBinaryFromMemory(&model, &err, &warn, assetData.data(), (uint32_t) assetData.size(), assetBaseDirectory);

        // If this is a text gltf file.
    } else {
        // Read the text based file
        success = tinyGltf.LoadASCIIFromString(&model, &err, &warn, (const char *) assetData.data(), (uint32_t) assetData.size(), assetBaseDirectory);
    }

    // If there was a warning, log it.
    if (warn.size() > 0) { PH_LOGW("[GLTF] %s", warn.c_str()); }

    // If there was an error, throw it.
    if (err.size() > 0) { PH_THROW(err.c_str()); }

    if (!success) { PH_THROW("failed to GLTF file %s", assetPath.c_str()); }

    // If operation was successful, convert it to the equivelant objects for the PhysRay SDK.
    // Create a builder to hold all the variables needed to generate the PhysRay objects.
    PH_LOGI("[GLTF] Constructing GLTF scene builder....");
    gltf::GLTFSceneAssetBuilder sceneBuilder(_assetSystem, _textureCache, _mainScene, &model, assetBaseDirectory, _skinnedMeshes, _morphTargetManager, _sbb,
                                             _createGeomLights);

    // Generate all of the available scenes and fetch the result.
    PH_LOGI("[GLTF] Building scene graph....");
    std::shared_ptr<SceneAsset> sceneAsset = sceneBuilder.build(_mainScene);

    // Create a builder to convert all the animations.
    gltf::animations::GLTFAnimationBuilder animationBuilder(&model, sceneAsset, _morphTargetManager);

    // Generate the animations.
    animationBuilder.build();

    return sceneAsset;
}
