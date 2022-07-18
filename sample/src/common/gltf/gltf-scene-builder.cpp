/**
 *
 */
#include "pch.h"
#include "gltf-mesh-builder.h"
#include "gltf-scene-builder.h"
#include "physray-type-converter.h"

#include <limits>
#include <stack>

namespace gltf {

GLTFSceneBuilder::GLTFSceneBuilder(const tinygltf::Model * model): _model(model) {
    //
}

void GLTFSceneBuilder::build(SceneAsset * sceneAsset, const tinygltf::Scene & tinygltfScene, ph::rt::Scene * phScene) {
    // Iterate all the root nodes.
    for (std::size_t index = 0; index < tinygltfScene.nodes.size(); ++index) {
        // Get the id of the current root node to be generated.
        int nodeId = tinygltfScene.nodes[index];

        // Create PhysRay node equivelants to the given root.
        // Since this is a root node, it has neither a
        // parent transformation, nor a parent.
        buildSceneGraphNode(sceneAsset, phScene, NULL, nodeId);
    }
}

void GLTFSceneBuilder::build(SceneAsset * sceneAsset, const std::vector<ph::rt::Scene *> & scenes, ph::rt::Scene * mainScene) {
    // The index the main scene is saved to in gltf.
    std::size_t mainSceneIndex;

    // If a valid default scene is selected.
    if (_model->defaultScene >= 0 && _model->defaultScene < (int) _model->scenes.size()) {
        // Use the GLTF file's default scene.
        mainSceneIndex = _model->defaultScene;

        // If no default scene is selected, default to zero.
    } else {
        mainSceneIndex = 0;
    }

    // Build the main scene.
    // If a main scene is selected.
    if (mainScene != NULL) {
        // Then build the main scene using it
        build(sceneAsset, _model->scenes[_model->defaultScene], mainScene);

        // If no main scene is selected.
    } else {
        // Try to fetch it from the list of scenes.
        if (mainSceneIndex < scenes.size()) {
            build(sceneAsset, _model->scenes[mainSceneIndex], mainScene);

            // If no scene exists.
        } else {
            // Then tell user that they must pass a scene to the GLTF loader.
            PH_THROW("GLTFSceneReader must be passed an instance of "
                     "ph::rt::Scene so that it can load objects into it.");
        }
    }

    // Determine the number of PhysRay scenes we can process.
    // A scene can only be processed if we have a matching PhysRay and tinygltf scene
    std::size_t phSceneCount = std::min(scenes.size(), _model->scenes.size());

    // Iterate each PhysRay scene.
    for (std::size_t sceneIndex = 0; sceneIndex < phSceneCount; ++sceneIndex) {
        // If we are on the main scene index.
        if (sceneIndex == mainSceneIndex) {
            // Main scene has already been loaded, so skip it.
            continue;
        }

        // Fetch current tiny gltf scene to be generated.
        const tinygltf::Scene & tinygltfScene = _model->scenes[sceneIndex];

        // Fetch current PhysRay scene to be generated.
        ph::rt::Scene * phScene = scenes[sceneIndex];

        // Parse tinygltf into the PhysRay scene.
        build(sceneAsset, tinygltfScene, phScene);
    }
}

void GLTFSceneBuilder::buildSceneGraphNode(SceneAsset * sceneAsset, ph::rt::Scene * phScene, ph::rt::Node * parent, int nodeId) {
    // Contains information about a tinygtlf node
    // to be converted to a PhysRay object.
    struct NodeConvertInfo {
        // Jedi node parent of the node to be created.
        ph::rt::Node * parent;

        // Tinygltf id of the node to be created.
        int nodeId;
    };

    // Use depth first search to build all descendants.
    // Stack used for depth first search.
    std::stack<NodeConvertInfo> search;

    // Fetch the tinygltf node referred to by nodeId.
    // Add it to the stack first.
    search.push({.parent = parent, .nodeId = nodeId});

    // While there are still nodes to iterate.
    while (search.size() > 0) {
        // Fetch the next node to process.
        NodeConvertInfo info = search.top();

        // Clear it from the stack.
        search.pop();

        // Fetch the tinygltf node referred to by nodeId.
        const tinygltf::Node & node = _model->nodes[info.nodeId];

        // Fetch the local transform of this node.
        ph::rt::NodeTransform parentToNode;
        JediTypeConverter::toNodeTransform(&node, parentToNode);

        // Create the PhysRay node.
        ph::rt::Node * phNode = phScene->addNode({.parent = info.parent, .transform = parentToNode});

        // Save this node to the list of all nodes, giving it the same index as its id.
        sceneAsset->getNodes()[info.nodeId] = phNode;

        // Save it to the set of nodes for its name.
        sceneAsset->getNameToNodes()[node.name].insert(phNode);
        phNode->name = node.name;

        // If this node has its own children.
        for (std::size_t index = 0; index < node.children.size(); ++index) {
            // Fetch the next node to be created.
            int childNodeId = node.children[index];

            // Add the child so that it can be converted on a future iteration.
            search.push({.parent = phNode, .nodeId = childNodeId});
        }
    }
}

} // namespace gltf
