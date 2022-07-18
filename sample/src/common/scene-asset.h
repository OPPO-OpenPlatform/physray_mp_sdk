/**
 *
 */
#pragma once

#include <ph/rt.h>
#include "animations/timeline.h"

#include <cstdint>
#include <istream>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Represents a scene and everything contained in it.
 */
class SceneAsset {
public:
    SceneAsset() = default;

    virtual ~SceneAsset() = default;

    /**
     * @return Bounding box of the scene in its initial state.
     */
    const Eigen::AlignedBox<float, 3> & getBounds() const { return _bounds; }

    /**
     * @return Bounding box of the scene in its initial state.
     */
    Eigen::AlignedBox<float, 3> & getBounds() { return _bounds; }

    /**
     * @return the list of scenes everything has been added to.
     */
    const std::vector<ph::rt::Scene *> & getScenes() const { return _scenes; }

    /**
     * @return the list of scenes everything has been added to.
     */
    std::vector<ph::rt::Scene *> & getScenes() { return _scenes; }

    /**
     * The scene everything has been added to.
     */
    ph::rt::Scene * getMainScene() const { return _mainScene; }

    /**
     * @param mainScene The main scene everything is added to.
     */
    void setMainScene(ph::rt::Scene * mainScene) { _mainScene = mainScene; }

    /**
     * @return Set of all cameras in the scene.
     */
    const std::vector<ph::rt::Camera *> & getCameras() const { return _cameras; }

    /**
     * @return Set of all cameras in the scene.
     */
    std::vector<ph::rt::Camera *> & getCameras() { return _cameras; }

    /**
     * @return Maps names to a set of cameras with that name.
     * Cameras without a name are mapped to "".
     */
    const std::unordered_map<std::string, std::unordered_set<ph::rt::Camera *>> & getNameToCameras() const { return _nameToCameras; }

    /**
     * @return Maps names to a set of cameras with that name.
     * Cameras without a name are mapped to "".
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Camera *>> & getNameToCameras() { return _nameToCameras; }

    /**
     * @return Set of all lights in the scene.
     */
    const std::vector<ph::rt::Light *> & getLights() const { return _lights; }

    /**
     * @return Set of all lights in the scene.
     */
    std::vector<ph::rt::Light *> & getLights() { return _lights; }

    /**
     * @return Maps names to a set of lights with that name.
     * Lights without a name are mapped to "".
     */
    const std::unordered_map<std::string, std::unordered_set<ph::rt::Light *>> & getNameToLights() const { return _nameToLights; }

    /**
     * @return Maps names to a set of lights with that name.
     * Lights without a name are mapped to "".
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Light *>> & getNameToLights() { return _nameToLights; }

    /**
     * @return List of all materials, mapped to their id.
     */
    const std::vector<ph::rt::Material *> & getMaterials() const { return _materials; }

    /**
     * @return List of all materials, mapped to their id.
     */
    std::vector<ph::rt::Material *> & getMaterials() { return _materials; }

    /**
     * @return Maps names to a set of materials with that name.
     * Animations without a name are mapped to "".
     */
    const std::unordered_map<std::string, std::unordered_set<ph::rt::Material *>> & getNameToMaterials() const { return _nameToMaterials; }

    /**
     * @return Maps names to a set of materials with that name.
     * Animations without a name are mapped to "".
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Material *>> & getNameToMaterials() { return _nameToMaterials; }

    /**
     * @return List of all nodes, mapped to their id.
     */
    const std::vector<ph::rt::Node *> & getNodes() const { return _nodes; }

    /**
     * @return List of all nodes, mapped to their id.
     */
    std::vector<ph::rt::Node *> & getNodes() { return _nodes; }

    /**
     * @return Maps names to a set of nodes with that name.
     * Nodes without a name are mapped to "".
     */
    const std::unordered_map<std::string, std::unordered_set<ph::rt::Node *>> & getNameToNodes() const { return _nameToNodes; }

    /**
     * @return Maps names to a set of nodes with that name.
     * Nodes without a name are mapped to "".
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Node *>> & getNameToNodes() { return _nameToNodes; }

    /**
     * @return List of all animations, mapped to their id.
     */
    const std::vector<std::shared_ptr<animations::Timeline>> & getAnimations() const { return _animations; }

    /**
     * @return List of all animations, mapped to their id.
     */
    std::vector<std::shared_ptr<animations::Timeline>> & getAnimations() { return _animations; }

    /**
     * @return Maps names to a set of animations with that name.
     * Animations without a name are mapped to "".
     */
    const std::unordered_map<std::string, std::unordered_set<std::shared_ptr<animations::Timeline>>> & getNameToAnimations() const { return _nameToAnimations; }

    /**
     * @return Maps names to a set of animations with that name.
     * Animations without a name are mapped to "".
     */
    std::unordered_map<std::string, std::unordered_set<std::shared_ptr<animations::Timeline>>> & getNameToAnimations() { return _nameToAnimations; }

protected:
    /**
     * Bounding box of the overall model.
     */
    Eigen::AlignedBox<float, 3> _bounds;

    /**
     * the list of scenes everything has been added to.
     */
    std::vector<ph::rt::Scene *> _scenes;

    /**
     * Main scene everything is added to.
     */
    ph::rt::Scene * _mainScene = nullptr;

    /**
     * List of all cameras.
     */
    std::vector<ph::rt::Camera *> _cameras;

    /**
     * Maps names to a set of cameras with that name.
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Camera *>> _nameToCameras;

    /**
     * List of all lights.
     */
    std::vector<ph::rt::Light *> _lights;

    /**
     * Maps names to a set of lights with that name.
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Light *>> _nameToLights;

    /**
     * List of all materials, mapped to their id.
     */
    std::vector<ph::rt::Material *> _materials;

    /**
     * Maps names to a set of materials with that name.
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Material *>> _nameToMaterials;

    /**
     * List of all nodes, mapped to their id.
     */
    std::vector<ph::rt::Node *> _nodes;

    /**
     * Maps names to a set of nodes with that name.
     */
    std::unordered_map<std::string, std::unordered_set<ph::rt::Node *>> _nameToNodes;

    /**
     * List of all animations, mapped to their id.
     */
    std::vector<std::shared_ptr<animations::Timeline>> _animations;

    /**
     * Maps names to a set of animations with that name.
     */
    std::unordered_map<std::string, std::unordered_set<std::shared_ptr<animations::Timeline>>> _nameToAnimations;
};
