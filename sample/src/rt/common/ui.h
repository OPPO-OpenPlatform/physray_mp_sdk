/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : ui.h
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

#pragma once

#include <ph/va.h>
#include "3rdparty/imgui/imgui.h"
#include <functional>

/// A simple in-game UI based on Imgui
class SimpleUI {
public:
    struct ConstructParameters {
        ph::va::VulkanSubmissionProxy & vsp;

        /// Handle to window system.
        /// On desktop, it is a pointer to GLFW window.
        /// On Android, it is a pointer to ANativeWindow.
        void * window = nullptr;

        /// screen space size of the UI area.
        uint32_t width, height;

        /// Defines max number of frames in flight. Minimal allowed value is 2.
        uint32_t maxInFlightFrames = 2;

        /// MSAA sample count.
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    /// constructor
    SimpleUI(const ConstructParameters &);

    /// destructor
    ~SimpleUI();

    /// the callback function that draw the UI using IMGUI.
    typedef std::function<void(void * user)> UIRoutine;

    struct RecordParameters {
        VkRenderPass    pass;
        VkCommandBuffer cb;
        UIRoutine       routine = nullptr;
        void *          user    = nullptr;
    };

    // Record the UI.
    void record(const RecordParameters &);

#if PH_ANDROID
    void handleAndroidSimpleTouchEvent(bool down, float x, float y);
#endif

private:
    class Impl;
    Impl * _impl = nullptr;
};