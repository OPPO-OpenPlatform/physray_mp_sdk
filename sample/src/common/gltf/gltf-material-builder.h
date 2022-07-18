/**
 *
 */
#pragma once

#include <ph/rt.h>

#include "gltf.h"
#include "../scene-asset.h"
#include "../texture-cache.h"

#include <filesystem>

namespace gltf {

/**
 * Constructs a material from a tiny gltf object.
 */
class GLTFMaterialBuilder {
public:
    /**
     * This constructor will load all of the images inside
     * the model file.
     * @param textureCache The object used to load and cache textures.
     * @param world Used to create new objects.
     * @param model The tinygltf model who's items are being instantiated in world.
     */
    GLTFMaterialBuilder(TextureCache * textureCache, ph::rt::World * world, const tinygltf::Model * model, const std::vector<ph::RawImage> * images);

    /**
     *
     */
    virtual ~GLTFMaterialBuilder() = default;

    /**
     * The world this builder is operating on.
     */
    ph::rt::World * getWorld() { return _world; }

    /**
     * The tinygltf model who's items are being instantiated in world.
     */
    const tinygltf::Model * getModel() const { return _model; }

    /**
     * Creates a PhysRay object equivelant to the tinygltf object
     * passed in.
     * @param material The tinygltf object to be converted.
     * @return The newly created PhysRay material.
     */
    ph::rt::Material * build(const tinygltf::Material & material);

private:
    /**
     * A functor allowing _ormToTextureHandle to use tuples as keys.
     */
    struct OrmHasher {
        size_t operator()(const std::tuple<int, int> & key) const;
    };

    /**
     * The object used to load and cache textures.
     */
    TextureCache * _textureCache;

    /**
     * World being used to create new lights.
     */
    ph::rt::World * _world;

    /**
     * The tinygltf model who's items are being instantiated in world.
     */
    const tinygltf::Model * _model;

    /**
     * Maps each tiny gltf image to its PhysRay equivelant.
     */
    const std::vector<ph::RawImage> * _images;

    /**
     * Records the combined Occlusion-Metalness-roughness images.
     * The builder combines the AO and MR (metal-rougness) images into a single ORM map that
     * it owns.
     *
     * Maps different occlusion metalness-roughness image id combinations
     * to the texture generated for them so that they can be reused.
     */
    std::unordered_map<std::tuple<int, int>, ph::rt::Material::TextureHandle, OrmHasher> _ormToTextureHandle;

    /**
     * @param imageId Id of the image we want the texture handle for.
     * Does NOT check for negative ids.
     * @return The texture handle wrapping the given image.
     */
    ph::rt::Material::TextureHandle getTextureHandleForImageId(int imageId, std::string uri);

    /**
     * Retrieves the uri of the given texture info object.
     * @param <TextureInfo> class of the texture info object.
     * All of the tinygltf classes have the same layout and members, but
     * do not share any base classes or interfaces.
     * @param info Info struct the image is being extracted from.
     */
    template<typename TextureInfo>
    const ph::ImageProxy & getTextureImage(const TextureInfo & info) {
        // Empty image used if no texture is selected.
        static ph::ImageProxy EMPTY = {};

        // If texture does not exist.
        if (info.index < 0) { return EMPTY; }

        // Jedi currently only supports TEXCOORD_0.
        // Check if this selects an additional texture coordinate.
        if (info.texCoord != 0) {
            // Warn user that this texture is using custom texture coords.
            PH_LOGW("Material is using texture coordinates \"TEXCOORD_%d\", "
                    "which is currently unsupported by ph.",
                    info.texCoord);
        }

        // Fetch the texture in question.
        const tinygltf::Texture & texture = _model->textures[info.index];

        // Return the texture's matching image.
        return ((*_images)[texture.source]).proxy();
    }

    /**
     * Retrieves the uri of the given texture info object.
     * @param <TextureInfo> class of the texture info object.
     * All of the tinygltf classes have the same layout and members, but
     * do not share any base classes or interfaces.
     * @param info Info struct the image is being extracted from.
     */
    template<typename TextureInfo>
    const ph::rt::Material::TextureHandle getTextureHandle(const TextureInfo & info) {
        // If texture does not exist.
        if (info.index < 0) {
            // Just return an empty texture handle.
            return ph::rt::Material::TextureHandle::EMPTY;
        }

        // Jedi currently only supports TEXCOORD_0.
        // Check if this selects an additional texture coordinate.
        if (info.texCoord != 0) {
            // Warn user that this texture is using custom texture coords.
            PH_LOGW("Material is using texture coordinates \"TEXCOORD_%d\", "
                    "which is currently unsupported by ph.",
                    info.texCoord);
        }

        // Fetch the texture in question.
        const tinygltf::Texture & texture = _model->textures[info.index];
        std::string               uri     = _model->images[texture.source].uri; // may need to convert to absolute path?

        // Return the texture handle for this texture's image.
        return getTextureHandleForImageId(texture.source, uri);
    }

    /**
     * Returns the texture handle for the combined Occlusion-Metalness-roughness image for the given material.
     * @param material The material we are retrieving the Orm texture for.
     * @return A texture containing the combined Occlusion-Metalness-roughness
     * image for the given material.
     */
    ph::rt::Material::TextureHandle getOrmTextureHandle(const tinygltf::Material & material);
};

} // namespace gltf
