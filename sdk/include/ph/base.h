// This is the main header of PhysRay SDK Base module.
#pragma once

/// The main compile time flag indicating if debug feature is enabled or not.
#ifndef PH_BUILD_DEBUG
    #ifdef NDEBUG
        #define PH_BUILD_DEBUG 0
    #else
        #define PH_BUILD_DEBUG 1
    #endif
#endif

/// The main compile time flag to determine if PhysRay SDK is built as static build or not.
#ifndef PH_BUILD_STATIC
    #define PH_BUILD_STATIC 1
#endif

/// Main namespace of PhysRay SDK
namespace ph {}

#include "base/base.inl"
#include "base/color.inl"
#include "base/asset.inl"
