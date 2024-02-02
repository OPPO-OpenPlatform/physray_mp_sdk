/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

#include "../scene-asset.h"

#include "gltf.h"

namespace gltf {

/**
 * Converts tiny gltf objects to their PhysRay equivelants.
 */
class JediTypeConverter {
public:
    /**
     * Wraps the PhysRay asset system into an equivelant struct compatible with tinygltf.
     * @param assetSystem The PhysRay asset system we want tinygltf to be able to read.
     * @return A tiny gltf struct used for reading assets.
     */
    static tinygltf::FsCallbacks toFsCallbacks(ph::AssetSystem * assetSystem);

    /**
     * Converts items in list to vector. Does nothing if there are less than 3 items available.
     * @param list The list we are reading the object from.
     * @param offset Offset into list that we start reading from.
     * @param vector The value to which the converted result is saved.
     * @return true if successfully converted, false otherwise.
     */
    static bool toVector3f(const std::vector<double> & list, std::size_t offset, Eigen::Vector3f & vector);

    /**
     * Converts items in list to vector. Does nothing if there are less than 3 items available.
     * Uses an offset of 0.
     * @param list The list we are reading the object from.
     * @param vector The value to which the converted result is saved.
     * @return true if successfully converted, false otherwise.
     */
    static bool toVector3f(const std::vector<double> & list, Eigen::Vector3f & vector);

    /**
     * Converts items in list to quaternion. Does nothing if there are less than 4 items available.
     * @param list The list we are reading the object from.
     * @param offset Offset into list that we start reading from.
     * @return true if successfully converted, false otherwise.
     */
    static bool toQuaternionf(const std::vector<double> & list, std::size_t offset, Eigen::Quaternionf & quaternion);

    /**
     * Converts items in list to quaternion. Does nothing if there are less than 4 items available.
     * Uses an offset of 0.
     * @param list The list we are reading the object from.
     * @return true if successfully converted, false otherwise.
     */
    static bool toQuaternionf(const std::vector<double> & list, Eigen::Quaternionf & quaternion);

    /**
     * Converts items in list to matrix. Does nothing if there are less than 16 items available.
     * @param list The list we are reading the object from.
     * @param offset Offset into list that we start reading from.
     * @return true if successfully converted, false otherwise.
     */
    static bool toMatrix(const std::vector<float> & list, std::size_t offset, Eigen::Matrix4f & matrix);

    /**
     * Converts items in list to matrix. Does nothing if there are less than 16 items available.
     * Uses an offset of 0.
     * @param list The list we are reading the object from.
     * @return true if successfully converted, false otherwise.
     */
    static bool toMatrix(const std::vector<float> & list, Eigen::Matrix4f & matrix);

    /**
     * Converts items in list to matrix. Does nothing if there are less than 16 items available.
     * @param list The list we are reading the object from.
     * @param offset Offset into list that we start reading from.
     * @return true if successfully converted, false otherwise.
     */
    static bool toMatrix(const std::vector<double> & list, std::size_t offset, Eigen::Matrix4f & matrix);

    /**
     * Converts items in list to matrix. Does nothing if there are less than 16 items available.
     * Uses an offset of 0.
     * @param list The list we are reading the object from.
     * @return true if successfully converted, false otherwise.
     */
    static bool toMatrix(const std::vector<double> & list, Eigen::Matrix4f & matrix);

    /**
     * Reads tinygltf node's transform and converts it to a PhysRay transform.
     * @param node The tinygltf node being read from.
     * @param nodeTransform The PhysRay transform being written to.
     */
    static void toNodeTransform(const tinygltf::Node * node, sg::Transform & nodeTransform);

private:
    /**
     * Class is not meant to be instantiated.
     */
    JediTypeConverter() = delete;
};

} // namespace gltf
