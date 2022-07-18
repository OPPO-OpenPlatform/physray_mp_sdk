/**
 *
 */
#pragma once

/**
 * Handles our configuration of TinyGLTF
 * and standardizes how it is included.
 */
// We are not using the STB image library.
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE

// We are currently handling external images completely ourselves.
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include "../3rdparty/tinygltf-2.5.0/tiny_gltf.h"
