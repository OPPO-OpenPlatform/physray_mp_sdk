/**
 *
 */
#pragma once

#include <ph/rt.h>

#include "accessor-reader.h"
#include "../scene-asset.h"

#include <filesystem>

namespace gltf {

/**
 * Converts tiny gltf objects to their PhysRay equivalents.
 */
class GLTFSceneBuilder {
public:
    /**
     * @param model The tinygltf model who's items are being instantiated.
     */
    GLTFSceneBuilder(const tinygltf::Model * model);

    /**
     * Destructor.
     */
    virtual ~GLTFSceneBuilder() = default;

    /**
     * The tinygltf model who's items are being instantiated.
     */
    const tinygltf::Model * getModel() const { return _model; }

    /**
     * Instantiates the nodes of the scenegraph from the tinygltf scene
     * inside the given PhysRay scene,
     * gives them their transforms, as well as their child relationships.
     * @param sceneAsset Object created nodes will be saved to.
     * @param tinygltfScene The tiny gltf scene to parse.
     * @param phScene The PhysRay scene to create nodes with.
     */
    void build(SceneAsset * sceneAsset, const tinygltf::Scene & tinygltfScene, ph::rt::Scene * phScene);

    /**
     * Instantiates the nodes of the scenegraphs from the tinygltf scenes
     * inside the matching PhysRay scenes,
     * gives them their transforms, as well as their child relationships.
     * @param sceneAsset Object created nodes will be saved to.
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
    void build(SceneAsset * sceneAsset, const std::vector<ph::rt::Scene *> & scenes, ph::rt::Scene * mainScene);

private:
    /**
     * The tinygltf model who's items are being instantiated in world.
     */
    const tinygltf::Model * _model;

    /**
     * Converts the given tiny gltf node to an equivelant PhysRay node.
     * @param sceneAsset Object created nodes will be saved to.
     * @param parent The PhysRay parent of the node being generated.
     * NULL if generating a root node.
     * @param nodeId Number identifying the tiny gltf node we are instantiating in ph.
     */
    void buildSceneGraphNode(SceneAsset * sceneAsset, ph::rt::Scene * phScene, ph::rt::Node * parent, int nodeId);
};

} // namespace gltf
