/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"

#include "gltf-image-builder.h"

#include <sstream>

namespace gltf {

bool GLTFImageBuilder::isRelativeURI(const std::string & uri) {
    // Return true if url does not have a protocol, meaning it is a relative url.
    return uri.find("://") == std::string::npos;
}

std::string GLTFImageBuilder::decodeURI(const std::string & uri) {
    // Pointer to the c string.
    const char * cString = uri.c_str();

    // Starting position of the region of the string that we are checking.
    std::size_t startPosition = 0;

    // The position of the current special character being processed.
    std::size_t currentPosition;

    // Stores the built string
    std::stringstream stream;

    // Keep searching for special characters until we are out of them.
    while ((currentPosition = uri.find('%', startPosition)) != std::string::npos) {
        // Copy all characters between here and the previous special character found.
        stream.write(cString + startPosition, currentPosition - startPosition);

        // Encoded characters take the form '%xy', where xy and y are each a single digit number.
        // Skip '%' and parse the next 2 characters into a byte.
        char numberString[]   = {cString[currentPosition + 1], cString[currentPosition + 2], 0};
        char encodedCharacter = (char) strtol(numberString, NULL, 16);

        // Save it to the string.
        stream.write(&encodedCharacter, 1);

        // Update the start position to just past the encoded character.
        startPosition = currentPosition + 3;
    }

    // Add the last section of the string.
    stream.write(cString + startPosition, uri.size() - startPosition);

    // Pass decoded string back to user.
    return stream.str();
}

GLTFImageBuilder::GLTFImageBuilder(ph::AssetSystem * assetSys, const std::filesystem::path & assetBaseDirectory)
    : _assetSys(assetSys), _assetBaseDirectory(assetBaseDirectory) {}

bool GLTFImageBuilder::build(const tinygltf::Image & image, ph::RawImage & phImage) {
    if (image.as_is) {
        // If the image data is already loaded to gltf in its original compression format.
        // Load the image object from memory.
        phImage = ph::RawImage::load(image.image);
        return true;
    } else if (isRelativeURI(image.uri)) {
        // This is a relative URI.
        // Unescape any special characters, like spaces.
        std::string decodedURI = decodeURI(image.uri);

        // Convert to an absolute URI by adding it to the directory the gltf file was loaded from.
        auto name = (_assetBaseDirectory / decodedURI).string();

        // Load the image bytes from the asset system.
        std::future<ph::Asset> futureAsset = _assetSys->load(name.c_str());
        ph::Asset              asset       = futureAsset.get();
        if (asset.content.i.empty()) return false;
        phImage = std::move(asset.content.i);
        return true;
    } else {
        // If the image is saved to an absolute uri,
        // warn user that absolute uris are not supported.
        PH_LOGW("[GLTF] Texture absolute uri \"%s\" is not supported.", image.uri.c_str());
        return false;
    }
}

} // namespace gltf
