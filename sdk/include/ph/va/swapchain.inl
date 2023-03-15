// This file is part of <ph/va.h>. Do NOT include it directly from your source code. Include <ph/va.h> instead.

namespace ph {
namespace va {

// ---------------------------------------------------------------------------------------------------------------------
//
struct SimpleSwapchain {
    struct InitParameters {
        /// the proxy to submit present to.
        VulkanSubmissionProxy & vsp;

        /// pointer to the window system. It is (GLFWwidow*) on desktop, (ANativeWindow*) on Android.
        /// ignored when creating offscreen swapchain.
        void * window = 0;

        /// handle of the main swapchain surface. Set to null to create an offscreen swapchain.
        VkSurfaceKHR surface = 0;

        /// back buffer format
        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;

        /// back buffer dimension
        uint32_t width = 0, height = 0;

        /// This value, along with VkSurfaceCapabilitiesKHR::minImageCount, determine the minimal
        /// number of images the swapchain must provide.
        uint32_t maxInFlightFrames = 2;

        /// is vsync enabled or not. Ignored for offscreen swapchain.
        bool vsync = true;

        /// This is in case graphics queue is different from the present queue.
        uint32_t gfxQueueFamilyIndex = 0;

        bool headless() const { return 0 == surface; }
    };

    struct BackBuffer {
        VkExtent2D    extent {};
        VkFormat      format = VK_FORMAT_UNDEFINED;
        VkImage       image  = 0;
        VkImageView   view   = 0;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    static SimpleSwapchain * create(const InitParameters &);

    virtual ~SimpleSwapchain() = default;

    virtual const InitParameters & initParameters() const = 0;

    virtual uint32_t activeBackBufferIndex() const = 0;

    virtual uint32_t backBufferCount() const = 0;

    virtual const BackBuffer & backBuffer(size_t index) const = 0;

    /// Acquire a back buffer for rendering. This method will update the return value of activeBackBufferIndex().
    /// \param semaphore Returns a semaphore that the rendering code needs to wait to ensure the back buffer
    ///                  is ready to use. Could be null, if there's no need to wait.
    /// \return true if the new back buffer image is successfully acquired. false otherwise.
    virtual bool acquireNextBackBuffer(VkSemaphore & semaphore) = 0;

    virtual VkResult present(VkSemaphore waitSemaphore) = 0;

protected:
    SimpleSwapchain() = default;
};

} // namespace va
} // namespace ph