/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "pch.h"

#include "ui.h"

#include "3rdparty/imgui/imgui_impl_vulkan.h"
#if PH_ANDROID
    #include "3rdparty/imgui/imgui_impl_android.h"
    #include <android/native_window.h>
#else
    #include "3rdparty/imgui/imgui_impl_glfw.h"
#endif

using namespace ph;
using namespace ph::va;

// ---------------------------------------------------------------------------------------------------------------------
/// The implementation class of SimpleUI
class SimpleUI::Impl {
public:
    /// constructor
    Impl(const ConstructParameters & cp): _cp(cp) {}

    /// destructor
    ~Impl() { cleanup(); }

    // -----------------------------------------------------------------------------------------------------------------
    /// construct the UI class. this is called once and only once during class construction.
    void construct() {
        auto & vgi = _cp.vsp.vgi();

        // TODO: We probably don't need to be this aggressive in terms of descriptor pool allocations
        VkDescriptorPoolSize poolSizes[] {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                          {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

        auto poolci          = VkDescriptorPoolCreateInfo {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolci.maxSets       = 1000;
        poolci.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolci.pPoolSizes    = poolSizes;

        PH_VA_REQUIRE(vkCreateDescriptorPool(vgi.device, &poolci, vgi.allocator, _imguiDescriptorPool.prepare(vgi)));

        ImGui::CreateContext();

#if PH_ANDROID
        ImGui_ImplAndroid_Init((ANativeWindow *) _cp.window);
#else
        ImGui_ImplGlfw_InitForVulkan((GLFWwindow *) _cp.window, false);
#endif

        auto & io      = ImGui::GetIO();
        io.IniFilename = nullptr; // disable imgui.ini writing.

        ImGui_ImplVulkan_LoadFunctions(
            [](const char * function_name, void * user_data) { return vkGetInstanceProcAddr((VkInstance) user_data, function_name); }, vgi.instance);
    }

    // -----------------------------------------------------------------------------------------------------------------
    /// record the render commands to command buffer. called once per frame.
    void record(const RecordParameters & rp) {
        // recreate all device objects if needed to match the incoming render pass.
        updateRenderPass(rp.pass);

        // Call user's UI routine to describe UI.
        ImGui_ImplVulkan_NewFrame();
#if PH_ANDROID
        ImGui_ImplAndroid_NewFrame();
        auto & io        = ImGui::GetIO();
        io.DisplaySize.x = (float) _cp.width;
        io.DisplaySize.y = (float) _cp.height;
        // scale UI 2x on Android
        io.DisplayFramebufferScale.x = 1.5f;
        io.DisplayFramebufferScale.y = 1.5f;
#else
        if (_cp.window) {
            ImGui_ImplGlfw_NewFrame(_cp.width, _cp.height, _cp.width, _cp.height);
        } else {
            // manually set display size and scaling factor, when there's no window
            auto & io                    = ImGui::GetIO();
            io.DisplaySize.x             = (float) _cp.width;
            io.DisplaySize.y             = (float) _cp.height;
            io.DisplayFramebufferScale.x = 1.0f;
            io.DisplayFramebufferScale.y = 1.0f;
        }
#endif

        // Check for invalid display size. This is to avoid triggering the assert
        // failure in ImGui::NewFrame()
        auto & size = ImGui::GetIO().DisplaySize;
        if (size.x <= 0.f || size.y <= 0.f) { return; }

        ImGui::NewFrame();
        if (rp.routine) { rp.routine(rp.user); }
        ImGui::Render();

        // Then push render commands to the command buffer
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), rp.cb);
    }

#if PH_ANDROID
    // -----------------------------------------------------------------------------------------------------------------
    //
    void handleAndroidSimpleTouchEvent(bool down, float x, float y) {
        int32_t w  = ANativeWindow_getWidth((ANativeWindow *) _cp.window);
        int32_t h  = ANativeWindow_getHeight((ANativeWindow *) _cp.window);
        auto &  io = ImGui::GetIO();
        x *= (float) _cp.width / w / io.DisplayFramebufferScale.x;
        y *= (float) _cp.height / h / io.DisplayFramebufferScale.y;
        return ImGui_ImplAndroid_HandleSimpleTouchEvent(down, x, y);
    }

#endif

private:
    void cleanup() {
        cleanupVulkanObjects();
#if PH_ANDROID
        ImGui_ImplAndroid_Shutdown();
#else
        ImGui_ImplGlfw_Shutdown();
#endif
        ImGui::DestroyContext();
    }

    void cleanupVulkanObjects() {
        auto device = _cp.vsp.vgi().device;

        if (device) threadSafeDeviceWaitIdle(device);

        // reset descriptor pool
        if (_imguiDescriptorPool) { vkResetDescriptorPool(device, _imguiDescriptorPool, 0); }

        ImGui_ImplVulkan_Shutdown();
    }

    void updateRenderPass(VkRenderPass pass) {
        if (pass == _currentRenderPass) return;

        cleanupVulkanObjects();

        // Initializes ImGui for Vulkan
        const auto &              vgi = _cp.vsp.vgi();
        ImGui_ImplVulkan_InitInfo ci  = {};
        ci.Instance                   = vgi.instance;
        ci.PhysicalDevice             = vgi.phydev;
        ci.Device                     = vgi.device;
        ci.DescriptorPool             = _imguiDescriptorPool.get();
        ci.MinImageCount              = _cp.maxInFlightFrames;
        ci.ImageCount                 = _cp.maxInFlightFrames;
        ci.MSAASamples                = _cp.samples;

        ImGui_ImplVulkan_Init(&ci, pass);

        // Upload ImGui font textures
        ph::va::SingleUseCommandPool pool(_cp.vsp);
        auto                         cb = pool.create();
        ImGui_ImplVulkan_CreateFontsTexture(cb);
        pool.finish(cb);

        // Clear font textures from CPU data
        ImGui_ImplVulkan_DestroyFontUploadObjects();

        // store the new render pass
        _currentRenderPass = pass;
    }

    ConstructParameters          _cp;
    VkRenderPass                 _currentRenderPass = 0;
    AutoHandle<VkDescriptorPool> _imguiDescriptorPool;
};

#if PH_ANDROID
// ---------------------------------------------------------------------------------------------------------------------
//
void SimpleUI::handleAndroidSimpleTouchEvent(bool down, float x, float y) { _impl->handleAndroidSimpleTouchEvent(down, x, y); }

#endif

// ---------------------------------------------------------------------------------------------------------------------
//
SimpleUI::SimpleUI(const ConstructParameters & cp): _impl(new Impl(cp)) { _impl->construct(); }
SimpleUI::~SimpleUI() { delete _impl; }
void SimpleUI::record(const RecordParameters & rp) {
    if (_impl) _impl->record(rp);
}
