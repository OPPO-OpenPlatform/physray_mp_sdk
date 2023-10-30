/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"
#include "image-splicer.h"

#include <algorithm>

ImageSplicer::Channel::Channel(): _imageProxy(nullptr), _imageChannel(0), _defaultValue(0) {
    //
}

ImageSplicer::Channel::Channel(const ph::ImageProxy * imageProxy, uint8_t imageChannel, uint8_t defaultValue)
    : _imageProxy(imageProxy), _imageChannel(imageChannel), _defaultValue(defaultValue) {
    //
}

ImageSplicer::Channel::Channel(const ph::ImageProxy * imageProxy, uint8_t imageChannel)
    : _imageProxy(imageProxy), _imageChannel(imageChannel), _defaultValue(0) {
    //
}

ImageSplicer::Channel::Channel(uint8_t defaultValue): _imageProxy(nullptr), _imageChannel(0), _defaultValue(defaultValue) {
    //
}

uint8_t ImageSplicer::Channel::getValue(size_t x, size_t y, size_t z) const {
    // If we have an image proxy to query.
    if (_imageProxy != nullptr) {
        // Get the pixel at the desired coordinates.
        const uint8_t * pixel = _imageProxy->pixel(0, 0, x, y, z);

        // Return the value of the desired channel.
        return _imageProxy->format().getPixelChannelByte(pixel, _imageChannel);

    } else {
        return _defaultValue;
    }
}

std::array<size_t, 3> ImageSplicer::Channel::getSize() const {
    // If we have an actual image.
    if (_imageProxy != nullptr) {
        // Return the size of the image.
        return {_imageProxy->width(), _imageProxy->height(), _imageProxy->depth()};

        // If we do not have an image.
    } else {
        // Just give it a size of 1 so it doesn't cause any divide by zero errors.
        // The calculation will not be used since there is no image to get pixels from.
        return {1, 1, 1};
    }
}

std::function<uint8_t(size_t x, size_t y, size_t z)> ImageSplicer::Channel::getValueFunction() const {
    // If we have an image proxy to query.
    if (_imageProxy != nullptr) {
        // Create local scope copy of image proxy so we can pass it to lambda.
        const ph::ImageProxy * imageProxy = _imageProxy;

        // Create local scope copy of image channel so we can pass it to lambda.
        uint8_t imageChannel = _imageChannel;

        // If image proxy uses RGBA8 format, as per the result.
        if (_imageProxy->format() == ph::ColorFormat::RGBA_8_8_8_8_UNORM()) {
            return [imageProxy, imageChannel](size_t x, size_t y, size_t z) {
                // Get the pixel at the desired coordinates.
                const uint8_t * pixel = imageProxy->pixel(0, 0, x, y, z);

                // Return the value of the desired channel.
                return pixel[imageChannel];
            };

            // If image uses an alternate format.
        } else {
            return [imageProxy, imageChannel](size_t x, size_t y, size_t z) {
                // Get the pixel at the desired coordinates.
                const uint8_t * pixel = imageProxy->pixel(0, 0, x, y, z);

                // Return the value of the desired channel.
                // Use format to convert it to a normalized unsigned byte.
                return imageProxy->format().getPixelChannelByte(pixel, imageChannel);
            };
        }

        // If we don't have an image.
    } else {
        // Create local scope copy of default value so we can pass it to lambda.
        uint8_t defaultValue = _defaultValue;

        // Then return a function that will always return the same value.
        return [defaultValue](size_t, size_t, size_t) { return defaultValue; };
    }
}

ImageSplicer::ImageSplicer() {
    //
}

ph::RawImage ImageSplicer::build(size_t width, size_t height, size_t depth) const {
    // Assemble the plane descriptor.
    ph::ImagePlaneDesc imagePlaneDesc = ph::ImagePlaneDesc::make(ph::ColorFormat::RGBA_8_8_8_8_UNORM(), width, height, depth);

    // Allocate the combined image.
    ph::RawImage splicedImage(imagePlaneDesc);

    // Iterate each channel and copy their contents.
    for (size_t channelIndex = 0; channelIndex < _channels.size(); ++channelIndex) {
        // Get current channel to be spliced into the result.
        const Channel & sourceChannel = _channels[channelIndex];

        // Get the backing image.
        const ph::ImageProxy * sourceImageProxy = sourceChannel.imageProxy();

        // If channel has an image to transfer.
        if (sourceImageProxy != nullptr) {
            // If source and destination sizes match.
            if (((size_t) sourceImageProxy->width()) == width && ((size_t) sourceImageProxy->height()) == height &&
                ((size_t) sourceImageProxy->depth()) == depth) {
                // Splice using the option that doesn't waste time on relative position calculations.
                spliceSameSize(splicedImage, channelIndex, sourceImageProxy, sourceChannel.imageChannel());

                // If source and destination sizes are different.
            } else {
                // Splice using the option which can convert between relative sizes.
                spliceDifferentSize(splicedImage, channelIndex, sourceImageProxy, sourceChannel.imageChannel());
            }

            // If channel doesn't have an image to be transfered.
        } else {
            // Transfer the channel's default value.
            spliceDefaultValue(splicedImage, channelIndex, sourceChannel.defaultValue());
        }
    }

    return splicedImage;
}

ph::RawImage ImageSplicer::build(size_t width, size_t height) const { return build(width, height, 1); }

ph::RawImage ImageSplicer::build() const {
    // Get the largest size of each dimension.
    std::array<size_t, 3> size = getCombinedImageSize();
    return build(size[0], size[1], size[2]);
}

std::array<size_t, 3> ImageSplicer::getCombinedImageSize() const {
    // Records the biggest of each dimension for each image.
    std::array<size_t, 3> size;

    // Default size of 1 for everything.
    size.fill(1);

    // Iterate all channels.
    for (const Channel & channel : _channels) {
        // Get size of this channel.
        std::array<size_t, 3> channelSize = channel.getSize();

        // Make sure size is big enough to fit everything.
        size[0] = std::max(size[0], channelSize[0]);
        size[1] = std::max(size[1], channelSize[1]);
        size[2] = std::max(size[2], channelSize[2]);
    }

    return size;
}

void ImageSplicer::spliceDefaultValue(ph::RawImage & destinationImage, size_t destinationChannelIndex, uint8_t defaultValue) const {
    // Get the image array we are filling.
    uint8_t * destinationData = destinationImage.data();

    // Total number of bytes in the destination array.
    size_t destinationSize = destinationImage.size();

    // Iterate all RGBA8 pixels and assign default value to the desired channel.
    // Channel index points to the absolute channel being modified.
    // Is incremented by the number of channels in each pixel each loop.
    // Will terminate when we have modified all pixels.
    for (size_t absoluteDestinationChannelIndex = destinationChannelIndex; absoluteDestinationChannelIndex < destinationSize;
         absoluteDestinationChannelIndex += 4) {
        destinationData[absoluteDestinationChannelIndex] = defaultValue;
    }
}

void ImageSplicer::spliceSameSize(ph::RawImage & destinationImage, size_t destinationChannelIndex, const ph::ImageProxy * sourceImageProxy,
                                  uint8_t sourceChannelIndex) const {
    // Get the image array we are filling.
    uint8_t * destinationData = destinationImage.data();

    // Check whether source image is using RGBA8U format.
    // Allows for an optimization if it's true since we don't have to convert formats and coordinates.
    // This is the fastest splice method.
    if (sourceImageProxy->format() == ph::ColorFormat::RGBA_8_8_8_8_UNORM()) {
        // Total number of bytes in the destination array.
        size_t destinationSize = destinationImage.size();

        // Get the image array we are copying from.
        const uint8_t * sourceData = sourceImageProxy->data;

        // Iterate all pixels.
        // pixelStartIndex refers to the index of the first channel of the current pixel being iterated.
        for (size_t pixelStartIndex = 0; pixelStartIndex < destinationSize; pixelStartIndex += 4) {
            // Pixel being copied to.
            uint8_t * destinationPixel = destinationData + pixelStartIndex;

            // Pixel being copied from.
            const uint8_t * sourcePixel = sourceData + pixelStartIndex;

            // Transfer the pixel channel byte.
            destinationPixel[destinationChannelIndex] = sourcePixel[sourceChannelIndex];
        }

        // If formats are different.
    } else {
        // Get size of each dimension.
        size_t width  = (size_t) destinationImage.width();
        size_t height = (size_t) destinationImage.height();
        size_t depth  = (size_t) destinationImage.depth();

        // Description of the image being copied into.
        const ph::ImageDesc & destinationDesc = destinationImage.desc();

        // Format of the image being copied from.
        ph::ColorFormat sourceColorFormat = sourceImageProxy->format();

        // Iterate all pixels across all dimensions.
        for (size_t x = 0; x < width; ++x) {
            for (size_t y = 0; y < height; ++y) {
                for (size_t z = 0; z < depth; ++z) {
                    // Pixel being copied to.
                    uint8_t * destinationPixel = destinationData + destinationDesc.pixel(0, 0, x, y, z);

                    // Pixel being copied from.
                    const uint8_t * sourcePixel = sourceImageProxy->pixel(0, 0, x, y, z);

                    // Get value of desired channel and convert it to byte format.
                    uint8_t sourceChannelValue = sourceColorFormat.getPixelChannelByte(sourcePixel, sourceChannelIndex);

                    // Transfer the pixel channel byte.
                    destinationPixel[destinationChannelIndex] = sourceChannelValue;
                }
            }
        }
    }
}

void ImageSplicer::spliceDifferentSize(ph::RawImage & destinationImage, size_t destinationChannelIndex, const ph::ImageProxy * sourceImageProxy,
                                       uint8_t sourceChannelIndex) const {
    // Get the image array we are filling.
    uint8_t * destinationData = destinationImage.data();

    // Get size of each dimension in destination.
    size_t destinationWidth  = (size_t) destinationImage.width();
    size_t destinationHeight = (size_t) destinationImage.height();
    size_t destinationDepth  = (size_t) destinationImage.depth();

    // Description of the image being copied into.
    const ph::ImageDesc & destinationDesc = destinationImage.desc();

    // Get size of each dimension in source.
    size_t sourceWidth  = (size_t) sourceImageProxy->width();
    size_t sourceHeight = (size_t) sourceImageProxy->height();
    size_t sourceDepth  = (size_t) sourceImageProxy->depth();

    // Format of the image being copied from.
    ph::ColorFormat sourceColorFormat = sourceImageProxy->format();

    // Whether source image is using RGBA8U format.
    const bool sameFormat = sourceColorFormat == ph::ColorFormat::RGBA_8_8_8_8_UNORM();

    // Iterate all pixels across all dimensions.
    // Iterate across x dimension.
    for (size_t destinationX = 0; destinationX < destinationWidth; ++destinationX) {
        // Source x coordinate in the same relative position as the destination x coordinate.
        size_t sourceX = destinationX * sourceWidth / destinationWidth;

        // Iterate across y dimension.
        for (size_t destinationY = 0; destinationY < destinationHeight; ++destinationY) {
            // Source y coordinate in the same relative position as the destination y coordinate.
            size_t sourceY = destinationY * sourceHeight / destinationHeight;

            // Iterate across z dimension.
            for (size_t destinationZ = 0; destinationZ < destinationDepth; ++destinationZ) {
                // Source z coordinate in the same relative position as the destination z coordinate.
                size_t sourceZ = destinationZ * sourceDepth / destinationDepth;

                // Pixel being copied to.
                uint8_t * destinationPixel = destinationData + destinationDesc.pixel(0, 0, destinationX, destinationY, destinationZ);

                // Pixel being copied from.
                const uint8_t * sourcePixel = sourceImageProxy->pixel(0, 0, sourceX, sourceY, sourceZ);

                // If pixels are using the same format.
                if (sameFormat) {
                    // We can just get the byte value directly.
                    destinationPixel[destinationChannelIndex] = sourcePixel[sourceChannelIndex];

                    // If pixels are using different formats.
                } else {
                    // Get value of desired channel and convert it to byte format.
                    uint8_t sourceChannelValue = sourceColorFormat.getPixelChannelByte(sourcePixel, sourceChannelIndex);

                    // Transfer the pixel channel byte.
                    destinationPixel[destinationChannelIndex] = sourceChannelValue;
                }
            }
        }
    }
}
