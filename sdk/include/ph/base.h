/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : base.h
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

/// Main namespace of PhysRay SDK
namespace ph {}

#include "base/base.inl"
#include "base/color.inl"
#include "base/asset.inl"

// Include a modified copy of palacaze/sigslot library. all classes is moved into ph namespace
// to avoid naming conflicting.
#include "base/sigslot/signal.hpp"
