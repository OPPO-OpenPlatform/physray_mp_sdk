/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "gltf-material-builder.h"

#include "../image-splicer.h"

namespace gltf {

GLTFMaterialBuilder::GLTFMaterialBuilder(TextureCache * textureCache, ph::rt::Scene * scene, const tinygltf::Model * model,
                                         const std::vector<ph::RawImage> * images)
    : _textureCache(textureCache), _scene(scene), _model(model), _images(images) {
    PH_REQUIRE(textureCache);
    PH_REQUIRE(scene);
    PH_REQUIRE(model);
    PH_REQUIRE(images);
}

ph::rt::Material * GLTFMaterialBuilder::build(const tinygltf::Material & material) {
    // Create the PhysRay object this will be converted into.
    ph::rt::Material::Desc phMaterialDesc = ph::rt::Material::Desc {};

    phMaterialDesc.sss         = 0.0f;
    phMaterialDesc.ao          = 1.0f;
    phMaterialDesc.anisotropic = 0.f;

    // Copy properties to their equivelants.
    // Convert metal.
    const tinygltf::PbrMetallicRoughness & metallicRoughness = material.pbrMetallicRoughness;

    phMaterialDesc.metalness = (float) metallicRoughness.metallicFactor;
    phMaterialDesc.roughness = (float) metallicRoughness.roughnessFactor;

    // Copy basic rgb color.
    for (std::size_t index = 0; index < 3; ++index) { phMaterialDesc.albedo[index] = (float) metallicRoughness.baseColorFactor[index]; }

    // Copy alpha defined by KHR_materials_transmission
    auto transmission = material.extensions.find("KHR_materials_transmission");
    if (transmission != material.extensions.end()) {
        const auto & value    = transmission->second;
        float        t        = (float) value.Get("transmissionFactor").GetNumberAsDouble();
        phMaterialDesc.opaque = 1.0f - t;
    } else {
        phMaterialDesc.opaque = 1.0f;
    }

    // Copy alpha defined by KHR_materials_transmission
    auto ior = material.extensions.find("KHR_materials_ior");
    if (ior != material.extensions.end()) {
        const auto & value = ior->second;
        phMaterialDesc.ior = (float) value.Get("ior").GetNumberAsDouble();
    } else {
        phMaterialDesc.ior = 1.5f;
    }

    // Copy clearcoat factors
    auto clearcoat = material.extensions.find("KHR_materials_clearcoat");
    if (clearcoat != material.extensions.end()) {
        const auto & value                = clearcoat->second;
        phMaterialDesc.clearcoat          = (float) value.Get("clearcoatFactor").GetNumberAsDouble();
        phMaterialDesc.clearcoatRoughness = (float) value.Get("clearcoatRoughnessFactor").GetNumberAsDouble();
    } else {
        phMaterialDesc.clearcoat          = 0.f;
        phMaterialDesc.clearcoatRoughness = 0.f;
    }

    // Transfer the basic color's texture if any.
    phMaterialDesc.maps[(std::size_t) ph::rt::Material::TextureType::ALBEDO] = getTextureHandle(metallicRoughness.baseColorTexture);

    // Copy emission color & texture
    for (std::size_t index = 0; index < 3; ++index) { phMaterialDesc.emission[index] = (float) material.emissiveFactor[index]; }
    phMaterialDesc.maps[(std::size_t) ph::rt::Material::TextureType::EMISSION] = getTextureHandle(material.emissiveTexture);

    // Copy other textures (if any).
    phMaterialDesc.maps[(std::size_t) ph::rt::Material::TextureType::NORMAL] = getTextureHandle(material.normalTexture);

    // If this material has occlusion or metallic roughness.
    phMaterialDesc.maps[(std::size_t) ph::rt::Material::TextureType::ORM] = getOrmTextureHandle(material);

    // Create the material and return it.
    auto phMaterial = _scene->create(material.name, phMaterialDesc);
    return phMaterial;
}

ph::rt::Material::TextureHandle GLTFMaterialBuilder::getTextureHandleForImageId(int imageId, std::string uri) {
    // If the texture handle does not exist, create it.
    // Get the image we want a texture handle for.
    const ph::ImageProxy & imageProxy = (*_images)[imageId].proxy();

    // Records the texture handle we are creating for this image.
    ph::rt::Material::TextureHandle textureHandle;

    // Create a texture handle for this image.
    textureHandle = _textureCache->createFromImageProxy(imageProxy, uri);

    return textureHandle;
}

ph::rt::Material::TextureHandle GLTFMaterialBuilder::getOrmTextureHandle(const tinygltf::Material & material) {
    // Fetch metallic properties.
    const tinygltf::PbrMetallicRoughness & metallicRoughness = material.pbrMetallicRoughness;

    // If there is no occlusion or metallic roughness texture.
    if (material.occlusionTexture.index < 0 && metallicRoughness.metallicRoughnessTexture.index < 0) {
        // Then just return an empty handle.
        return ph::rt::Material::TextureHandle::EMPTY_2D();
    }

    // Create a key representing the desired ORM.
    std::tuple<int, int> ormKey = std::make_tuple(material.occlusionTexture.index, metallicRoughness.metallicRoughnessTexture.index);
    // Search for the preexisting texture handle for this orm combination (if any).
    auto iterator = _ormToTextureHandle.find(ormKey);

    // If texture handle for this ORM already exists.
    if (iterator != _ormToTextureHandle.end()) {
        // Then just return it.
        return iterator->second;

        // If texture handle does not exist.
    } else {
        // Create a texture handle for an image containing the combination of the
        // occlusion and metallic roughness images.
        // Get the images we are combining.
        // The image containing the occlusion.
        const ph::ImageProxy * pOcclusionImg;

        // If this material has an occlusion texture.
        if (material.occlusionTexture.index > 0) {
            // Fetch the backing PhysRay image.
            pOcclusionImg = &getTextureImage(material.occlusionTexture);

            // If this material does NOT have an occlusion texture.
        } else {
            // Leave the pointer empty.
            pOcclusionImg = nullptr;
        }

        // The image containing the metallic roughness.
        const ph::ImageProxy * pMetalRoughImg;

        std::string uri = "";
        // If this material has a metallic roughness texture.
        if (metallicRoughness.metallicRoughnessTexture.index > 0) {
            // Fetch the backing PhysRay image.
            pMetalRoughImg = &getTextureImage(metallicRoughness.metallicRoughnessTexture);

            uri = _model->images[_model->textures[metallicRoughness.metallicRoughnessTexture.index].source].uri;
            // If this material does NOT have a metallic roughness texture.
        } else {
            // Leave the pointer empty.
            pMetalRoughImg = nullptr;
        }

        // Combine the two images together.
        ImageSplicer imageSplicer;

        // Setup the channels we want to combine.
        std::array<ImageSplicer::Channel, 4> & channels = imageSplicer.channels();

        // Setup the occlusion channel. Defaults to 255 (not occluded) if no texture.
        channels[0] = ImageSplicer::Channel(pOcclusionImg, 0, 255);

        // Setup the roughness channel. Leave it as zero if no roughness.
        // Channel 1 (green) is used for roughness.
        channels[1] = ImageSplicer::Channel(pMetalRoughImg, 1, 0);

        // Setup the metallic channel. Leave it as zero if no metal.
        // Channel 2 (blue) is used for metalness.
        channels[2] = ImageSplicer::Channel(pMetalRoughImg, 2, 0);

        // Alpha is not used, just set it all to opaque.
        channels[3] = ImageSplicer::Channel(pMetalRoughImg, 3, 127);

        // Combine the images together.
        ph::RawImage ormMap = imageSplicer.build();

        // Create a texture handle out of them.
        ph::rt::Material::TextureHandle textureHandle = _textureCache->createFromImageProxy(ormMap.proxy(), uri);

        // Cache this combination just in case another material needs it.
        _ormToTextureHandle[ormKey] = textureHandle;

        // Return the texture handle we created.
        return textureHandle;
    }
}

size_t GLTFMaterialBuilder::OrmHasher::operator()(const std::tuple<int, int> & key) const {
    // Records the calculated hash of the texture handle.
    size_t hash = 7;

    // Hash the parameters.
    hash = 79 * hash + std::get<0>(key);
    hash = 79 * hash + std::get<1>(key);

    return hash;
}

} // namespace gltf
