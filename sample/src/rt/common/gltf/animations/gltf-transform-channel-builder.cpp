/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-transform-channel-builder.cpp
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
#include "gltf-transform-channel-builder.h"
#include "../physray-type-converter.h"
#include "../../animations/quaternionf-linear-interpolator.h"
#include "../../animations/quaternionf-cubic-spline-interpolator.h"
#include "../../animations/step-interpolator.h"
#include "../../animations/target-channel.h"

#include <chrono>

namespace gltf {
namespace animations {

/**
 * Parses a vector3f from an iterator to a vector.
 * Assumes no gaps between the elements.
 * @param sampleOutputStart Start of the value being parsed in the collection.
 * @param parsedValue Holds the result object.
 */
static void parseVector3f(std::vector<float>::iterator sampleOutputStart, Eigen::Vector3f & parsedValue) {
    parsedValue.x() = sampleOutputStart[0];
    parsedValue.y() = sampleOutputStart[1];
    parsedValue.z() = sampleOutputStart[2];
}

/**
 * Parses a quaternionf from an iterator to a vector.
 * Assumes no gaps between the elements.
 * @param sampleOutputStart Start of the value being parsed in the collection.
 * @param parsedValue Holds the result object.
 */
static void parseQuaternionf(std::vector<float>::iterator sampleOutputStart, Eigen::Quaternionf & parsedValue) {
    parsedValue.x() = sampleOutputStart[0];
    parsedValue.y() = sampleOutputStart[1];
    parsedValue.z() = sampleOutputStart[2];
    parsedValue.w() = sampleOutputStart[3];

    // GLTF animated quaternions are NOT guaranteed to already be normalized.
    parsedValue.normalize();
}

/**
 * Factory method.
 * @return a new ly created simple cubic interpolator.
 */
template<typename T>
static std::shared_ptr<::animations::Interpolator<T>> buildSimpleCubicInterpolator(const T & startTangent, const T & endTangent) {
    return std::shared_ptr<::animations::Interpolator<T>>(new ::animations::SimpleCubicSplineInterpolator<T>(startTangent, endTangent));
}

/**
 * Factory method.
 * @return a newly created quaternion cubic interpolator.
 */
static std::shared_ptr<::animations::Interpolator<Eigen::Quaternionf>> buildQuaternionfCubicInterpolator(const Eigen::Quaternionf & startTangent,
                                                                                                         const Eigen::Quaternionf & endTangent) {
    return std::shared_ptr<::animations::Interpolator<Eigen::Quaternionf>>(new ::animations::QuaternionfCubicSplineInterpolator(startTangent, endTangent));
}

GLTFTransformChannelBuilder::GLTFTransformChannelBuilder(tinygltf::Model * model, ::animations::TransformChannel * transformChannel,
                                                         tinygltf::AnimationChannel * animationChannel, tinygltf::AnimationSampler * animationSampler)
    : _model(model), _transformChannel(transformChannel), _animationChannel(animationChannel), _accessorReader(model), _animationSampler(animationSampler) {
    //
}

std::shared_ptr<::animations::Channel> GLTFTransformChannelBuilder::build() {
    // If this is animating the node's translation.
    if (_animationChannel->target_path == "translation") {
        return buildTranslateChannel();

        // If this is animating the node's rotation.
    } else if (_animationChannel->target_path == "rotation") {
        return buildRotateChannel();

        // If this is animating the node's scale.
    } else if (_animationChannel->target_path == "scale") {
        return buildScaleChannel();

        // If this is a type of channel this builder does not support.
    } else {
        // Return an empty shared pointer.
        return std::shared_ptr<::animations::Channel>();
    }
}

/**
 * Builds a translation channel using the tinygltf animation object.
 * @return The channel object generated from the tinygltf animation.
 * Undefined behaviour if channel type does not matched.
 */
std::shared_ptr<::animations::Channel> GLTFTransformChannelBuilder::buildTranslateChannel() {
    // Create a local reference to transform channel so that it can be captured by the lambda.
    ::animations::TransformChannel * transformChannel = _transformChannel;

    // Create a channel for manipulating the node's translation.
    std::shared_ptr<::animations::TargetChannel<Eigen::Vector3f>> channel(
        new ::animations::TargetChannel<Eigen::Vector3f>([transformChannel](Eigen::Vector3f & value) { transformChannel->setTranslation(value); }));

    // Parse its keyvalues.
    buildVector3fKeyValues(channel->getTimeToKeyValue());

    return channel;
}

std::shared_ptr<::animations::Channel> GLTFTransformChannelBuilder::buildRotateChannel() {
    // Create a local reference to transform channel so that it can be captured by the lambda.
    ::animations::TransformChannel * transformChannel = _transformChannel;

    // Create a channel for manipulating the node's translation.
    std::shared_ptr<::animations::TargetChannel<Eigen::Quaternionf>> channel(
        new ::animations::TargetChannel<Eigen::Quaternionf>([transformChannel](Eigen::Quaternionf & value) { transformChannel->setRotation(value); }));

    // Parse its keyvalues.
    buildQuaternionfKeyValues(channel->getTimeToKeyValue());

    return channel;
}

std::shared_ptr<::animations::Channel> GLTFTransformChannelBuilder::buildScaleChannel() {
    // Create a local reference to transform channel so that it can be captured by the lambda.
    ::animations::TransformChannel * transformChannel = _transformChannel;

    // Create a channel for manipulating the node's translation.
    std::shared_ptr<::animations::TargetChannel<Eigen::Vector3f>> channel(
        new ::animations::TargetChannel<Eigen::Vector3f>([transformChannel](Eigen::Vector3f & value) { transformChannel->setScale(value); }));

    // Parse its keyvalues.
    buildVector3fKeyValues(channel->getTimeToKeyValue());

    return channel;
}

void GLTFTransformChannelBuilder::buildVector3fKeyValues(
    std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<::animations::KeyValue<Eigen::Vector3f>>> & timeToKeyValue) {
    // Parse according to the type of interpolation.
    // If this is a linear interpolation.
    if (_animationSampler->interpolation == "LINEAR") {
        // Create an object to handle how each individual keyframe is built.
        SimpleKeyValueBuilder<Eigen::Vector3f> keyValueBuilder(
            parseVector3f, std::shared_ptr<::animations::Interpolator<Eigen::Vector3f>>(new ::animations::SimpleLinearInterpolator<Eigen::Vector3f>()));

        // Assemble the key values.
        // Gap is component count.
        buildKeyValues<Eigen::Vector3f>(timeToKeyValue, keyValueBuilder, 3);

        // If this is a step interpolation.
    } else if (_animationSampler->interpolation == "STEP") {
        // Create an object to handle how each individual keyframe is built.
        SimpleKeyValueBuilder<Eigen::Vector3f> keyValueBuilder(
            parseVector3f, std::shared_ptr<::animations::Interpolator<Eigen::Vector3f>>(new ::animations::StepInterpolator<Eigen::Vector3f>()));

        // Assemble the key values.
        // Gap is component count.
        buildKeyValues<Eigen::Vector3f>(timeToKeyValue, keyValueBuilder, 3);

        // If this is a cubic spline interpolation.
    } else if (_animationSampler->interpolation == "CUBICSPLINE") {
        // Create an object to handle how each individual keyframe is built.
        CubicSplineKeyValueBuilder<Eigen::Vector3f> keyValueBuilder(parseVector3f, 3, buildSimpleCubicInterpolator);

        // Assemble the key values.
        // Gap is component count * 3.
        buildKeyValues<Eigen::Vector3f>(timeToKeyValue, keyValueBuilder, 3 * 3);

        // If this is anything else.
    } else {
        // Warn user that interpolation type is not supported.
        PH_LOGW("Interpolation type '%s' not supported.");
    }
}

void GLTFTransformChannelBuilder::buildQuaternionfKeyValues(
    std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<::animations::KeyValue<Eigen::Quaternionf>>> & timeToKeyValue) {
    // Parse according to the type of interpolation.
    // If this is a linear interpolation.
    if (_animationSampler->interpolation == "LINEAR") {
        // Create an object to handle how each individual keyframe is built.
        SimpleKeyValueBuilder<Eigen::Quaternionf> keyValueBuilder(
            parseQuaternionf, std::shared_ptr<::animations::Interpolator<Eigen::Quaternionf>>(new ::animations::QuaternionfLinearInterpolator()));

        // Assemble the key values.
        // Gap is component count.
        buildKeyValues<Eigen::Quaternionf>(timeToKeyValue, keyValueBuilder, 4);

        // If this is a step interpolation.
    } else if (_animationSampler->interpolation == "STEP") {
        // Create an object to handle how each individual keyframe is built.
        SimpleKeyValueBuilder<Eigen::Quaternionf> keyValueBuilder(
            parseQuaternionf, std::shared_ptr<::animations::Interpolator<Eigen::Quaternionf>>(new ::animations::StepInterpolator<Eigen::Quaternionf>()));

        // Assemble the key values.
        // Gap is component count.
        buildKeyValues<Eigen::Quaternionf>(timeToKeyValue, keyValueBuilder, 4);

        // If this is a cubic spline interpolation.
    } else if (_animationSampler->interpolation == "CUBICSPLINE") {
        // Create an object to handle how each individual keyframe is built.
        CubicSplineKeyValueBuilder<Eigen::Quaternionf> keyValueBuilder(parseQuaternionf, 4, buildQuaternionfCubicInterpolator);

        // Assemble the key values.
        // Gap is component count * 3.
        buildKeyValues<Eigen::Quaternionf>(timeToKeyValue, keyValueBuilder, 4 * 3);

        // If this is anything else.
    } else {
        // Warn user that interpolation type is not supported.
        PH_LOGW("Interpolation type '%s' not supported.");
    }
}

} // namespace animations
} // namespace gltf
