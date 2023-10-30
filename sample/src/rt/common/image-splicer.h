/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/rt-utils.h>

#include <array>

/// Allows different channels from different images to be added together
/// into one RGBA8 image.
class ImageSplicer {
public:
    /// The object filling in one of the channels of the result image.
    class Channel {
    public:
        /// Default constructor. imageProxy will be null and defaultValue will be zero.
        Channel();

        /// @param imageProxy Source image values will be retrieved from. This can be null if
        /// you do not want to splice an image, in which case defaultValue will be used instead.
        /// @param imageChannel Channel of each pixel in imageProxy we will retrieve the value of.
        /// @param defaultValue The value to return if imageProxy is null.
        Channel(const ph::ImageProxy * imageProxy, uint8_t imageChannel, uint8_t defaultValue);

        /// @param imageProxy Source image values will be retrieved from. This can be null if
        /// you do not want to splice an image, in which case defaultValue will be used instead.
        /// @param imageChannel Channel of each pixel in imageProxy we will retrieve the value of.
        Channel(const ph::ImageProxy * imageProxy, uint8_t imageChannel);

        /// Leaves imageProxy as null and makes channel simply pass the default value.
        /// @param defaultValue The value to return if imageProxy is null.
        Channel(uint8_t defaultValue);

        ~Channel() = default;

        /// @param x
        /// @param y
        /// @param z
        /// @return
        uint8_t getValue(size_t x, size_t y, size_t z) const;

        /// @return Source image values will be retrieved from. This can be null if
        /// you do not want to splice an image, in which case defaultValue will be used instead.
        const ph::ImageProxy * imageProxy() const { return _imageProxy; }

        /// @param imageProxy Source image values will be retrieved from. This can be null if
        /// you do not want to splice an image, in which case defaultValue will be used instead.
        void setImageProxy(const ph::ImageProxy * imageProxy) { _imageProxy = imageProxy; }

        /// @return Channel of each pixel in imageProxy we will retrieve the value of.
        uint8_t imageChannel() const { return _imageChannel; }

        /// @param imageChannel Channel of each pixel in imageProxy we will retrieve the value of.
        void setImageChannel(uint8_t imageChannel) { _imageChannel = imageChannel; }

        /// @return The value to return if imageProxy is null.
        uint8_t defaultValue() const { return _defaultValue; }

        /// @param defaultValue The value to return if imageProxy is null.
        void setDefaultValue(uint8_t defaultValue) { _defaultValue = defaultValue; }

        // Make sure ImageSplicer has full access to this class.
        friend class ImageSplicer;

    private:
        ///
        const ph::ImageProxy * _imageProxy;

        ///
        uint8_t _imageChannel;

        /// Value to use if no image.
        uint8_t _defaultValue;

        /// Convenience function to get size of the image backing this channel.
        /// @return Size of the image, {1, 1, 1} if image is null.
        std::array<size_t, 3> getSize() const;

        /// @return A function that will return the value of this channel at each coordinate.
        std::function<uint8_t(size_t x, size_t y, size_t z)> getValueFunction() const;
    };

    ///
    ImageSplicer();
    ~ImageSplicer() = default;

    /// @param width Width of the final image.
    /// @param height Height of the final image.
    /// @param depth Depth of the final image.
    /// @return An image created from the combination of each channel.
    /// The image will have the size of the parameters.
    ph::RawImage build(size_t width, size_t height, size_t depth) const;

    /// @param width Width of the final image.
    /// @param height Height of the final image.
    /// @return An image created from the combination of each channel.
    /// The image will have the width and height of the parameters and a depth of 1.
    ph::RawImage build(size_t width, size_t height) const;

    /// @return An image created from the combination of each channel.
    /// The resulting image will have the largest size of each dimension for all channels.
    ph::RawImage build() const;

    /// @return List of channels the image will be spliced together from.
    /// Each Channel's index matches the channel it will be providing.
    /// For example, as the result is an RGBA8 image, the red channel is at index 0,
    /// the green channel is at index 1, the blue channel is at index 2,
    /// and the alpha channel is at index 3.
    std::array<ImageSplicer::Channel, 4> & channels() { return _channels; }

private:
    /// List of channels the image will be spliced together from.
    std::array<ImageSplicer::Channel, 4> _channels;

    // @return The largest value of each dimension for all channels.
    std::array<size_t, 3> getCombinedImageSize() const;

    /// Called when source image is empty.
    /// Transfers default value to all pixels on the given channel.
    /// @param destinationImage The image being modified.
    /// @param destinationChannelIndex The channel in each pixel to modify.
    /// @param defaultValue Value to assign to channelIndex of each pixel.
    void spliceDefaultValue(ph::RawImage & destinationImage, size_t destinationChannelIndex, uint8_t defaultValue) const;

    /// Called when destination and source images have the same size.
    /// Copies source channel of source image to destination channel of destination image.
    /// @param destinationImage The image being modified.
    /// @param channelIndex The channel in each pixel to modify.
    /// @param sourceImageProxy Image being copied to destination image.
    /// @param sourceChannelIndex The channel in each pixel to copy.
    void spliceSameSize(ph::RawImage & destinationImage, size_t destinationChannelIndex, const ph::ImageProxy * sourceImageProxy,
                        uint8_t sourceChannelIndex) const;

    /// Called when destination and source images have different sizes.
    /// Copies source channel of source image to destination channel of destination image.
    /// @param destinationImage The image being modified.
    /// @param channelIndex The channel in each pixel to modify.
    /// @param sourceImageProxy Image being copied to destination image.
    /// @param sourceChannelIndex The channel in each pixel to copy.
    void spliceDifferentSize(ph::RawImage & destinationImage, size_t destinationChannelIndex, const ph::ImageProxy * sourceImageProxy,
                             uint8_t sourceChannelIndex) const;
};
