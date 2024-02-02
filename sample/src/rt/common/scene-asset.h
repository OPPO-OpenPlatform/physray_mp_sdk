/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
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
     * The scene graph everything has been added to.
     */
    sg::Graph & getMainGraph() const { return *_mainGraph; }

    void setMainGraph(sg::Graph * mainGraph) { _mainGraph = mainGraph; }

    /**
     * The scene everything has been added to.
     */
    ph::rt::Scene & getMainScene() const { return _mainGraph->scene(); }

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
    const std::vector<sg::Node *> & getLights() const { return _lights; }

    /**
     * @return Set of all lights in the scene.
     */
    std::vector<sg::Node *> & getLights() { return _lights; }

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
    const std::vector<sg::Node *> & getNodes() const { return _nodes; }

    /**
     * @return List of all nodes, mapped to their id.
     */
    std::vector<sg::Node *> & getNodes() { return _nodes; }

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
     * Main scene graph everything is added to.
     */
    sg::Graph * _mainGraph = nullptr;

    /**
     * List of all cameras.
     */
    std::vector<Camera> _cameras;

    /**
     * List of all lights.
     */
    std::vector<sg::Node *> _lights;

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
    std::vector<sg::Node *> _nodes;

    /**
     * Maps names to a set of nodes with that name.
     */
    std::unordered_map<std::string, std::unordered_set<sg::Node *>> _nameToNodes;

    /**
     * List of all animations, mapped to their id.
     */
    std::vector<std::shared_ptr<animations::Timeline>> _animations;

    /**
     * Maps names to a set of animations with that name.
     */
    std::unordered_map<std::string, std::unordered_set<std::shared_ptr<animations::Timeline>>> _nameToAnimations;
};
