/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include "../../animations/key-value.h"
#include "../../animations/timeline.h"
#include "../accessor-reader.h"
#include "../gltf.h"
#include "../../scene-asset.h"
#include "../../animations/weight-channel.h"
#include "../../animations/simple-linear-interpolator.h"

#include <memory>

namespace gltf {
namespace animations {
class GLTFWeightChannelBuilder {
public:
    GLTFWeightChannelBuilder(tinygltf::Model * model, ::animations::WeightChannel * weightChannel, tinygltf::AnimationChannel * animationChannel,
                             tinygltf::AnimationSampler * animationSampler);

    virtual ~GLTFWeightChannelBuilder() = default;
    std::shared_ptr<::animations::Channel> build();

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

protected:
    // TODO same as transform channel builder. combine?

    /**
     * The tinygltf model who's animations are being built.
     */
    tinygltf::Model * _model;

    /**
     * The transform channel this animation channel will be modifying.
     */
    ::animations::WeightChannel * _weightChannel;

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

    class VectorLinearInterpolator : public ::animations::Interpolator<std::vector<float>> {
    public:
        VectorLinearInterpolator()          = default;
        virtual ~VectorLinearInterpolator() = default;
        void interpolate(const std::vector<float> & startValue, const std::vector<float> & endValue, float fraction,
                         std::vector<float> & interpolated) override {
            interpolated.resize(startValue.size());
            for (size_t i = 0; i < startValue.size(); i++) {
                float distance  = endValue[i] - startValue[i];
                interpolated[i] = startValue[i] + (float) (fraction * distance);
            }
        }
    };

    // Not the same as transform channel- no need to template
    class SimpleKeyValueBuilder : public KeyValueBuilder<std::vector<float>> {
    public:
        /**
         * @param valueParser Function that parses the vector array
         * into the end value of the key value.
         * @param interpolator The interpolator to pass to each keyvalue.
         */
        SimpleKeyValueBuilder(size_t stride, std::shared_ptr<::animations::Interpolator<std::vector<float>>> interpolator)
            : _stride(stride), _interpolator(interpolator) {
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
        ::animations::KeyValue<std::vector<float>> * build() override {
            // Parse value from the sample output.
            std::vector<float> value;
            for (int i = 0; i < int(_stride); i++) { value.push_back(_sampleOutputStart[i]); }

            // Build the keyvalue.
            return new ::animations::KeyValue<std::vector<float>>(value, _interpolator);
        }

    protected:
        size_t _stride;
        /**
         * Used to interpolate between each key value.
         */
        std::shared_ptr<::animations::Interpolator<std::vector<float>>> _interpolator;
    };
    void
    buildVectorKeyValues(std::map<std::chrono::duration<uint64_t, std::nano>, std::shared_ptr<::animations::KeyValue<std::vector<float>>>> & timeToKeyValue);

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
                        "%zu is negative: %f.",
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