/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

/**
 *
 */
#pragma once

#include <ph/rt-utils.h>

/**
 *
 * Ambient occlusion data is extracted from the R channel of the occlusion image.
 * Metalness data from the B channel and roughness from the G channel of the MR map.
 * The format supplied to this function must be RGBA8_UNORM. This function will succeed
 * if either one of the maps is valid.
 * @param pOcclusionImg
 * @param pMetalRoughnessImg
 * @deprecated Replaced by ImageSplicer.
 */
static inline std::unique_ptr<ph::RawImage>
combineOcclusionMetalRoughness(const ph::ImageProxy * pOcclusionImg,     // [in] Image 1: occlusion image, src of R channel
                               const ph::ImageProxy * pMetalRoughnessImg // [in] Image 2: metal-rough image, src of GB channels
) {

    static const ph::ImageProxy EMPTY = {};

    PH_REQUIRE(pOcclusionImg != nullptr || pMetalRoughnessImg != nullptr);

    // PH_LOGI("[ORM] Num Elements: %d", num_elements);
    // PH_LOGI("[ORM] Bits per pixel: %d", occlusionImg.format(0,0).bitsPerPixel());

    uint8_t *     pOccSrc  = nullptr;
    size_t        numBytes = 0;
    uint8_t *     pMrSrc   = nullptr;
    ph::ImageDesc copyImageDesc;

    if (pOcclusionImg) {
        // This util expects the image to be RGBA8_UNORM only.
        PH_REQUIRE(pOcclusionImg->format() == ph::ColorFormat::RGBA_8_8_8_8_UNORM());

        pOccSrc       = pOcclusionImg->data;
        copyImageDesc = pOcclusionImg->desc;
        numBytes      = pOcclusionImg->size();
    }

    if (pMetalRoughnessImg) {
        // This util expects the image to be RGBA8_UNORM only.
        PH_REQUIRE(pMetalRoughnessImg->format() == ph::ColorFormat::RGBA_8_8_8_8_UNORM());

        pMrSrc        = pMetalRoughnessImg->data;
        copyImageDesc = pMetalRoughnessImg->desc;

        if (numBytes > 0) {
            // The sizes of the two images that need to be combined must be the same.
            PH_REQUIRE(numBytes == pMetalRoughnessImg->size());
        }

        numBytes = pMetalRoughnessImg->size();
    }

    std::vector<uint8_t> rawData(numBytes, 0);
    uint8_t *            pDst = rawData.data();

    // Alloc failure.
    PH_ASSERT(pDst != nullptr);

    // Assuming the format is a RGBA8_UNORM
    PH_REQUIRE(numBytes % 4 == 0);
    PH_LOGI("[UTIL] Combining AO and MR maps.");
    for (unsigned i = 0; i < numBytes; i += 4) {
        // Copy every 2nd (G) and 3rd (B) byte from the metal roughness image
        if (pOccSrc) {
            pDst[i] = pOccSrc[i];
        } // R: occlusion
        else {
            pDst[i] = 255;
        } // Set AO = 1 (not occluded) if there is no occlusion data.
        if (pMrSrc) {
            pDst[i + 1] = pMrSrc[i + 1]; // G: roughness
            pDst[i + 2] = pMrSrc[i + 2];
        }                  // B: metalness
        pDst[i + 3] = 255; // Alpha (unused)
    }

    // Construct a RawImage on the heap.
    return std::unique_ptr<ph::RawImage>(new ph::RawImage(copyImageDesc, pDst, numBytes));
}
