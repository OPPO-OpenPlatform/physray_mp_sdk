/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "gltf.h"

#include <filesystem>

namespace gltf {

/**
 * Converts tinygltf images to matching PhysRay images.
 */
class GLTFImageBuilder {
public:
    /**
     * This constructor will load all of the images inside
     * the model file.
     * @param assetSys The main asset system.
     * @param assetBaseDirectory Base directory where the model file came from.
     * Is used to determine how to load relative urls.
     */
    GLTFImageBuilder(ph::AssetSystem * assetSys, const std::filesystem::path & assetBaseDirectory);

    /**
     *
     */
    virtual ~GLTFImageBuilder() = default;

    /**
     * @return Path where glt file is being read from,
     * is used to build relative paths.
     */
    const std::filesystem::path & getAssetBaseDirectory() const { return _assetBaseDirectory; }

    /**
     * Creates a PhysRay object equivelant to the tinygltf object
     * passed in.
     * @param image The tinygltf object to be converted.
     * @param phImage Place the image will be loaded to.
     * @return true if image conversion was succesful, false otherwise.
     */
    bool build(const tinygltf::Image & image, ph::RawImage & phImage);

private:
    /**
     * @return true if this is a relative uri, false otherwise.
     */
    static bool isRelativeURI(const std::string & uri);

    /**
     * Decodes special characters encoded into the given uri.
     * For example, "%20" will become " ".
     * @param uri The uri to be decoded.
     * @return The uri with all special characters decoded.
     */
    static std::string decodeURI(const std::string & uri);

    /**
     * The asset system used to asset refereced by the main gltf file
     */
    ph::AssetSystem * _assetSys;

    /**
     * Base directory where the model file came from.
     * Is used to determine how to load relative urls.
     */
    std::filesystem::path _assetBaseDirectory;
};

} // namespace gltf
