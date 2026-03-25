// engine/Swapchain.hpp
#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Device.hpp"
#include "engine/window/Window.hpp"

namespace svk 
{

// A read-only snapshot of the exact resources needed for this specific frame.
// Handed down by the Swapchain to the RenderRoutine.
struct SwapchainFrame 
{
    uint32_t imageIndex;
    vk::Image image;
    vk::ImageView imageView;
    vk::Extent2D currentExtent;
    vk::Semaphore renderFinishedSemaphore; 
};

class Swapchain 
{
public:
    // "The portal's frame" - binds to the OS window and the Vulkan logical device.
    Swapchain(const svk::Device& device, const svk::Window& window);

    // Ironclad Constraints
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    ~Swapchain() = default;

    // =========================================================================
    //  Core Operations
    // =========================================================================

    // Destroys current views/swapchain and rebuilds based on current window size.
    void recreate();

    // Acquires the next image. Returns std::nullopt if the swapchain is out of date.
    // The caller MUST provide the semaphore that Vulkan will signal when the image is ready.
    [[nodiscard]] const std::optional<svk::SwapchainFrame> acquireNextImage(vk::Semaphore imageAvailableSemaphore) const;

    // Submits the finished image to the Present queue.
    // Returns true on success, false if the swapchain is out of date/suboptimal.
    [[nodiscard]] bool present(const svk::SwapchainFrame& frame) const;

    // =========================================================================
    //  Accessors
    // =========================================================================
    
    [[nodiscard]] inline vk::Format getFormat() const { return m_imageFormat; }
    [[nodiscard]] inline vk::Extent2D getExtent() const { return m_extent; }

private:
    const svk::Device& m_device;
    const svk::Queue& m_presentQueue;
    const svk::Window& m_window;

    // Fixed configuration established during construction
    vk::Format m_imageFormat = vk::Format::eUndefined;
    vk::ColorSpaceKHR m_colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    vk::PresentModeKHR m_presentMode = vk::PresentModeKHR::eFifo;
    vk::SharingMode m_sharingMode = vk::SharingMode::eExclusive;
    std::vector<uint32_t> m_sharingQueueFamilyIndices;

    // Dynamic state (changes on resize)
    vk::Extent2D m_extent;
    vk::raii::SwapchainKHR m_swapchain = nullptr;
    
    // Handles owned by the swapchain
    std::vector<vk::Image> m_images;
    
    // Wrappers owned by us
    std::vector<vk::raii::ImageView> m_imageViews;
    
    // Per-image sync primitives (owned by swapchain as requested)
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;

    // Internal Helpers
    void chooseSurfaceFormat();
    void choosePresentMode();
    void determineSharingMode();
    void buildSwapchain();
};

} // namespace svk
