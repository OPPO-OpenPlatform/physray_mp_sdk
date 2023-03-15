/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : gltf.cpp
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

// In addition to the defines inside of tiny-gltf.h,
// add "TINYGLTF_IMPLEMENTATION" to compile the defines within this cpp.
// This needs to happen exactly once, to provide the defines and avoid redefines
// due to gltf being a single header library.
#define TINYGLTF_IMPLEMENTATION

#include "gltf.h"
