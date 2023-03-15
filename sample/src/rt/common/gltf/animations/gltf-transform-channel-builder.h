/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf-transform-channel-builder.h
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
#pragma once

#include <ph/rt-utils.h>

#include "../../animations/key-value.h"
#include "../../animations/simple-cubic-spline-interpolator.h"
#include "../../animations/timeline.h"
#include "../accessor-reader.h"
#include "../gltf.h"
#include "../../scene-asset.h"
#include "../../animations/quaternionf-cubic-spline-interpolator.h"
#include "../../animations/transform-channel.h"

#include <memory>

namespace gltf {
namespace animations {

/**
 * Assembles individual transform animation channels from tiny gltf objects.
 * The resulting channel will then modify a GLTFTransformChannel, which will in turn
 * update the transform of the targeted node.
 * Note that this only supports building channels for translation, rotation, and scale.
 */
class GLTFTransformChannelBuilder {
public:
    /**
     *
     * @param model The tinygltf model who's items are being instantiated as animations.
     * @param transformChannel The transformChannel the channel this creates will be manipulating.
     * @param animation The animation a channel is being built from.
     * @param channel The tiny gltf channel a PhysRay channel is being built from.
     */
    GLTFTransformChannelBuilder(tinygltf::Model * model, ::animations::TransformChannel * transformChannel, tinygltf::AnimationChannel * animationChannel,
                                tinygltf::AnimationSampler * animationSampler);

    /**
     * Destructor.
     */
    virtual ~GLTFTransformChannelBuilder() = default;

    /**
     * Builds a channel using the tinygltf animation object.
     * Will check the channel's type and return the result of the appropriate build method.
     * @return The channel object generated from the tinygltf animation.
     * Will return null if channel type is not supported.
     */
    std::shared_ptr<::animations::Channel> build();

    /**
     * Builds a translation channel using the tinygltf animation object.
     * @return The channel object generated from the tinygltf animation.
     * Undefined behaviour if channel type does not matched.
     */
    std::shared_ptr<::animations::Channel> buildTranslateChannel();

    /**
     * Builds a rotation channel using the tinygltf animation object.
     * @return The channel object generated from the tinygltf animation.
     * Undefined behaviour if channel type does not matched.
     */
    std::shared_ptr<::animations::Channel> buildRotateChannel();

    /**
     * Builds a scaling channel using the tinygltf animation object.
     * @return The channel object generated from the tinygltf animation.
     * Undefined behaviour if channel type does not matched.
     */
    std::shared_ptr<::animations::Channel> buildScaleChannel();

private:
    /**
     * Builds keyvalues.
     * @param <T> Type being targeted by the keyvalues.
     */
    template<typename T>
    class KeyValueBuilder {
    public:
        /**
         *
         */
        KeyValueBuilder() = default;

        /**
         *
         */
        virtual ~KeyValueBuilder() = default;

        /**
         * @return Iterator to start of next part of sample output to parse.
         */
        std::vector<float>::iterator getSampleOutputStart() { return _sampleOutputStart; }

        /**
         * @param sampleOutputStart Iterator to start of next part of sample output to parse.
         */
        void setSampleOutputStart(std::vector<float>::iterator sampleOutputStart) { _sampleOutputStart = sampleOutputStart; }

        /**
         * Builds the next key value from the current parameters.
         * @return A key value according to the current parameters.
         */
        virtual ::animations::KeyValue<T> * build() = 0;

    protected:
        /**
         * Iterator to start of next part of sample output to parse.
         */
        std::vector<float>::iterator _sampleOutputStart;
    };

    /**
     * Builds keyvalues with a few simple parameters.
     * @param <T> Type being targeted by the keyvalues.
     */
    template<typename T>
    class SimpleKeyValueBuilder : public KeyValueBuilder<T> {
    public:
        /**
         * @param valueParser Function that parses the vector array
         * into the end value of the key value.
         * @param interpolator The interpolator to pass to each keyvalue.
         */
        SimpleKeyValueBuilder(void (*valueParser)(std::vector<float>::iterator sampleOutputStart, T & parsedValue),
                              std::shared_ptr<::animations::Interpolator<T>> interpolator)
            : _valueParser(valueParser), _interpolator(interpolator) {
            //
        }

        /**
         *
         */
        virtual ~SimpleKeyValueBuilder() = default;

        /**
         * Builds the next key value from the current parameters.
         * @return A key value according to the current parameters.
         */
        ::animations::KeyValue<T> * build() override {
            // Parse value from the sample output.
            T value;
            _valueParser(KeyValueBuilder<T>::_sampleOutputStart, value);

            // Build the keyvalue.
            return new ::animations::KeyValue<T>(value, _interpolator);
        }

    protected:
        /**
         * Parses the next value from the vector.
         */
        void (*_valueParser)(std::vector<float>::iterator sampleOutputStart, T & parsedValue);

        /**
         * Used to interpolate between each key value.
         */
        std::shared_ptr<::animations::Interpolator<T>> _interpolator;
    };

    /**
     * Builds keyvalues with a cubic spline interpolator.
     * @param <T> Type being targeted by the keyvalues.
     */
    template<typename T>
    class CubicSplineKeyValueBuilder : public KeyValueBuilder<T> {
    public:
        /**
         * @param valueParser Function that parses the vector array
         * into the end value of the key value.
         * @param componentCount Number of components to read for each value.
         */
        CubicSplineKeyValueBuilder(void (*valueParser)(std::vector<float>::iterator sampleOutputStart, T & parsedValue), std::size_t componentCount,
                                   std::shared_ptr<::animations::Interpolator<T>> (*interpolatorBuilder)(const T & startTangent, const T & endTangent))
            : _valueParser(valueParser), _componentCount(componentCount), _interpolatorBuilder(interpolatorBuilder) {
            //
        }

        /**
         *
         */
        virtual ~CubicSplineKeyValueBuilder() = default;

        /**
         * Builds the next key value from the current parameters.
         * @return A key value according to the current parameters.
         */
        ::animations::KeyValue<T> * build() override {
            // Parse values from the sample output.
            T startTangent;
            _valueParser(KeyValueBuilder<T>::_sampleOutputStart, startTangent);
            T value;
            _valueParser(KeyValueBuilder<T>::_sampleOutputStart + _componentCount * 1, value);
            T endTangent;
            _valueParser(KeyValueBuilder<T>::_sampleOutputStart + _componentCount * 2, endTangent);

            // Build the keyvalue.
            return new ::animations::KeyValue(value, _interpolatorBuilder(startTangent, endTangent));
        }

    protected:
        /**
         * Parses the next value from the vector.
         */
        void (*_valueParser)(std::vector<float>::iterator sampleOutputStart, T & parsedValue);

        /**
         * Number of components to read for each value.
         */
        std::size_t _componentCount;

        /**
         * Generates an appropriate cubic spline interpolator for the given values.
         */
        std::shared_ptr<::animations::Interpolator<T>> (*_interpolatorBuilder)(const T & startTangent, const T & endTangent);
    };

    /**
     * Builds keyvalues with a cubic spline interpolator.
     * @param <T> Type being targeted by the keyvalues.
     */
    template<typename T>
    class SimpleCubicSplineKeyValueBuilder : public CubicSplineKeyValueBuilder<T> {
    public:
        /**
         * @param valueParser Function that parses the vector array
         * into the end value of the key value.
         * @param componentCount Number of components to read for each value.
         */
        SimpleCubicSplineKeyValueBuilder(void (*valueParser)(std::vector<float>::iterator sampleOutputStart, T & parsedValue, std::size_t stride),
                                         std::size_t componentCount)
            : CubicSplineKeyValueBuilder<T>(valueParser, componentCount) {
            //
        }

        /**
         *
         */
        virtual ~SimpleCubicSplineKeyValueBuilder() = default;

        /**
         * @return a cubic spline interpolator.
         */
        std::shared_ptr<::animations::Interpolator<T>> createCubicInterpolator(const T & startTangent, const T & endTangent) override {
            return std::shared_ptr<::animations::Interpolator<T>>(new ::animations::SimpleCubicSplineInterpolator<T>(startTangent, endTangent));
        }

    protected:
    };

    /**
     * The tinygltf model who's animations are being built.
     */
    tinygltf::Model * _model;

    /**
     * The transform channel this animation channel will be modifying.
     */
    ::animations::TransformChannel * _transformChannel;

    /**
     * The animation being converted to a PhysRay animation channel.
     */
    tinygltf::AnimationChannel * _animationChannel;

    /**
     * Used to read binary data from the model.
     */
    AccessorReader _accessorReader;

    /**
     * The sampler providing the data of this animation.
     */
    tinygltf::AnimationSampler * _animationSampler;

    /**
     * Builds key values for a vector3f based channel.
     * @param timeToKeyValue Collection key values will be stored to.
     */
    void
    buildVector3fKeyValues(std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<::animations::KeyValue<Eigen::Vector3f>>> & timeToKeyValue);

    /**
     * Builds key values for a quaternionf based channel.
     * @param timeToKeyValue Collection key values will be stored to.
     */
    void buildQuaternionfKeyValues(
        std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<::animations::KeyValue<Eigen::Quaternionf>>> & timeToKeyValue);

    /**
     * Builds keyvalues from the sampler.
     * @param timeToKeyValue Collection key values will be stored to.
     * @param keyValueBuilder Used to assemble each key value.
     * @param stride Number of floats between each sample output value.
     */
    template<typename T>
    void buildKeyValues(std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<::animations::KeyValue<T>>> & timeToKeyValue,
                        KeyValueBuilder<T> & keyValueBuilder, std::size_t stride) {
        // Parse out the time of each keyframe, which is saved as seconds.
        std::vector<float> keyValueTimes;
        _accessorReader.readAccessor(_model->accessors[_animationSampler->input], keyValueTimes);

        // Read the sample output data, which holds stuff like key values and data for the interpolator.
        std::vector<float> samplerOutput;
        _accessorReader.readAccessor(_model->accessors[_animationSampler->output], samplerOutput);

        // Iterate each keyframe, which should be equal to the number of times.
        for (std::size_t frameIndex = 0; frameIndex < keyValueTimes.size(); ++frameIndex) {
            // Get the time of the keyframe.
            float keyValueTimeSeconds = keyValueTimes[frameIndex];

            // According to the GLTF specification,
            // negative keyvalue time is an error:
            // https://github.com/KhronosGroup/glTF-Validator/blob/master/ISSUES.md
            // ACCESSOR_ANIMATION_INPUT_NEGATIVE
            // However, some exporters, such as Blender, can potentially output
            // this, resulting in undefined behavior.
            if (keyValueTimeSeconds < 0) {
                PH_LOGW("Animation input accessor element at index "
                        "%llu is negative: %f.",
                        frameIndex, keyValueTimeSeconds);

                // Skip this invalid key time.
                continue;
            }

            // Cast key value time to a chrono duration object.
            std::chrono::nanoseconds keyValueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<float>(keyValueTimeSeconds));

            // Build the keyvalue.
            // Give it the start of the next sample.
            keyValueBuilder.setSampleOutputStart(samplerOutput.begin() + frameIndex * stride);

            // Add the key value.
            timeToKeyValue[keyValueTime].reset(keyValueBuilder.build());
        }
    }
};

} // namespace animations
} // namespace gltf
