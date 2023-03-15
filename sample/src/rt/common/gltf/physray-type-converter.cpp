/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : physray-type-converter.cpp
 *
 * Version: 2.0
 *
 * Date : Feb 2023
 *
 * Author: Computing & Graphics Research Institute
 *
 * ------------------ Revision History: ---------------------
 *  <version>  <date>  <author>  <desc>
 *
 *******************************************************************************/

/**
 *
 */
#include "pch.h"
#include "physray-type-converter.h"

namespace gltf {

bool JediTypeConverter::toVector3f(const std::vector<double> & list, std::size_t offset, Eigen::Vector3f & vector) {
    // If the given list contains a vector.
    if (list.size() >= offset + 3) {
        vector.x() = (float) list[offset + 0];
        vector.y() = (float) list[offset + 1];
        vector.z() = (float) list[offset + 2];

        return true;
    }

    return false;
}

bool JediTypeConverter::toVector3f(const std::vector<double> & list, Eigen::Vector3f & vector) { return toVector3f(list, 0, vector); }

bool JediTypeConverter::toQuaternionf(const std::vector<double> & list, std::size_t offset, Eigen::Quaternionf & quaternion) {
    // If the given list contains a quaternion.
    if (list.size() >= offset + 4) {
        quaternion.x() = (float) list[offset + 0];
        quaternion.y() = (float) list[offset + 1];
        quaternion.z() = (float) list[offset + 2];
        quaternion.w() = (float) list[offset + 3];

        return true;
    }

    return false;
}

bool JediTypeConverter::toQuaternionf(const std::vector<double> & list, Eigen::Quaternionf & quaternion) { return toQuaternionf(list, 0, quaternion); }

bool JediTypeConverter::toMatrix(const std::vector<float> & list, std::size_t offset, Eigen::Matrix4f & matrix) {
    // If the given list contains a matrix.
    if (list.size() >= offset + 16) {
        // Get array holding matrix to be loaded.
        const float * array = list.data() + offset;

        // Convert float array to matrix.
        matrix = Eigen::Map<const Eigen::Matrix4f, Eigen::Unaligned, Eigen::Stride<4, 1>>(array);

        return true;
    }

    return false;
}

bool JediTypeConverter::toMatrix(const std::vector<float> & list, Eigen::Matrix4f & matrix) { return toMatrix(list, 0, matrix); }

bool JediTypeConverter::toMatrix(const std::vector<double> & list, std::size_t offset, Eigen::Matrix4f & matrix) {
    // If the given list contains a matrix.
    if (list.size() >= offset + 16) {
        // Stores the matrix data as a float array.
        float array[16];

        // Beginning of the collection where we want to start casting.
        auto begin = list.begin() + offset;

        // Convert double array to float array.
        std::transform(begin, begin + 16, array, [](double value) { return (float) value; });

        // Convert float array to matrix.
        matrix = Eigen::Map<Eigen::Matrix4f, Eigen::Unaligned, Eigen::Stride<4, 1>>(array);

        return true;
    }

    return false;
}

bool JediTypeConverter::toMatrix(const std::vector<double> & list, Eigen::Matrix4f & matrix) { return toMatrix(list, 0, matrix); }

void JediTypeConverter::toNodeTransform(const tinygltf::Node * node, ph::rt::NodeTransform & nodeTransform) {

    // If the matrix is defined.
    if (node->matrix.size() >= 16) {
        // Read matrix directly.
        Eigen::Matrix4f matrix;
        toMatrix(node->matrix, matrix);

        // Assign to the result.
        nodeTransform = matrix;

        // Otherwise, attempt to read the transform via the
        // combination of the translation, rotation, and scale.
    } else {
        // Make sure the transform is initialized to identity.
        nodeTransform = ph::rt::NodeTransform::Identity();

        // Combine everything into the transform by order of translate, rotate, scale.
        // Grab the translation.
        Eigen::Vector3f translation;

        // If we were able to grab the translation.
        if (toVector3f(node->translation, translation)) {
            // Apply the translation to the transform.
            nodeTransform.translate(translation);
        }

        // Grab the rotation.
        Eigen::Quaternionf rotation;

        // If we were able to grab the rotation.
        if (toQuaternionf(node->rotation, rotation)) {
            // Apply the translation to the transform.
            nodeTransform.rotate(rotation);
        }

        // Grab the scale.
        Eigen::Vector3f scale;

        // If we were able to grab the scale.
        if (toVector3f(node->scale, scale)) {
            // Apply the scale to the transform.
            nodeTransform.scale(scale);
        }
    }
}

/**
 * Implementation of tiny gltf's FilExistsFunction.
 * Allows tiny gltf to check if a file exists in an asset system.
 */
static bool fileExistsAssetSystem(const std::string & abs_filename, void * user_data) {
    // Cast out the asset system.
    ph::AssetSystem * assetSystem = (ph::AssetSystem *) user_data;

    // Return whether the given file name exists.
    return assetSystem->exist(abs_filename.c_str());
}

/**
 * Implementation of tiny gltf's ExpandFilePathFunction.
 * Allows tiny gltf to expand a file path on an asset system,
 * converting path variables to their true values.
 */
static std::string expandFilePathFunctionAssetSystem(const std::string & path, void *) {
    // AssetManager does not currently support anything along the lines
    // of getAbsolutePath, so just return the original path.
    return path;
}

/**
 * Implementation of tiny gltf's ReadWholeFileFunction.
 * Allows tiny gltf to read an entire file from an asset system.
 */
static bool readWholeFileFunctionAssetSystem(std::vector<unsigned char> * buffer, std::string * fileReadError, const std::string & filePath, void * user_data) {
    // Cast out the asset system.
    ph::AssetSystem * assetSystem = (ph::AssetSystem *) user_data;

    // Attempt to read the file in question.
    ph::Asset asset = assetSystem->load(filePath.c_str()).get();

    // If asset loading failed, return false.
    if (!asset) {
        *fileReadError = ph::formatstr("failed to read asset: %s", filePath.c_str());
        return false;
    }

    // Fetch the resulting data.
    const auto & data = asset.content.v;

    // Load the data into the buffer.
    *buffer = std::move(data);

    // Since we succeeded in reading the file, return true.
    return true;
}

tinygltf::FsCallbacks JediTypeConverter::toFsCallbacks(ph::AssetSystem * assetSystem) {
    // Create the tiny gltf struct wrapping the asset system.
    tinygltf::FsCallbacks result;

    // Give the struct the appropriate function pointers to utilize asset system.
    result.FileExists     = fileExistsAssetSystem;
    result.ExpandFilePath = expandFilePathFunctionAssetSystem;
    result.ReadWholeFile  = readWholeFileFunctionAssetSystem;

    // Asset system does not currently support writing.
    result.WriteWholeFile = NULL;

    // Pass the asset system to userdata so that its functions can reuse it.
    result.user_data = assetSystem;

    return result;
}

} // namespace gltf
