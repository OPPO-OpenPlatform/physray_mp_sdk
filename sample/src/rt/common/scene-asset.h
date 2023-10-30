/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>
#include "animations/timeline.h"
#include "camera.h"

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
    const std::vector<Camera> & getCameras() const { return _cameras; }

    /**
     * @return Set of all cameras in the scene.
     */
    std::vector<Camera> & getCameras() { return _cameras; }

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

    /**
     * scene model list
     */
    std::vector<ph::rt::Model *> models;

protected:
    /**
     * Bounding box of the overall model.
     */
    Eigen::AlignedBox<float, 3> _bounds;

    /**
     * Main scene everything is added to.
     */
    ph::rt::Scene * _mainScene = nullptr;

    /**
     * List of all cameras.
     */
    std::vector<Camera> _cameras;

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
