// engine/Swapchain.cpp
#include "engine/Swapchain.hpp"

#include <limits>
#include <stdexcept>

namespace svk
{
Swapchain::Swapchain(const svk::Device& device, const svk::Window& window)
    : m_device(device),
      m_presentQueue(device.presentQueue()),
      m_window(window)
{
    chooseSurfaceFormat();
    choosePresentMode();
    determineSharingMode();
    buildSwapchain();
}

void Swapchain::chooseSurfaceFormat()
{
    const auto formats = m_device.physicalDevice().getSurfaceFormatsKHR(*m_window.getSurface());
    
    for (const auto& format : formats)
    {
        // check if good format is available
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            m_imageFormat = format.format;
            m_colorSpace = format.colorSpace;
            return;
        }
    }
    throw std::runtime_error("Required surface format (eB8G8R8A8Srgb + eSrgbNonlinear) not supported");
}

void Swapchain::choosePresentMode()
{
    const auto presentModes = m_device.physicalDevice().getSurfacePresentModesKHR(*m_window.getSurface());
    
    for (const auto& mode : presentModes)
    {
        if (mode == vk::PresentModeKHR::eMailbox)
            { m_presentMode = mode; return; }
    }
    // Mailbox not available, fall back to FIFO (guaranteed)
    m_presentMode = vk::PresentModeKHR::eFifo;
}

void Swapchain::determineSharingMode()
{
    const uint32_t graphicsFamily = m_device.graphicsQueue().getFamilyIndex();
    const uint32_t presentFamily = m_presentQueue.getFamilyIndex();

    m_sharingQueueFamilyIndices.emplace_back(graphicsFamily);

    if (graphicsFamily != presentFamily)
    {
        m_sharingMode = vk::SharingMode::eConcurrent;
        m_sharingQueueFamilyIndices.emplace_back(presentFamily);
    }
}

void Swapchain::recreate()
{
    m_device.waitIdle();
    
    // Clear old RAII handles explicitly to destroy them
    m_imageViews.clear();
    m_renderFinishedSemaphores.clear();
    
    buildSwapchain();
}

// =========================================================================
//  buildSwapchain()
// =========================================================================

void Swapchain::buildSwapchain()
{
    const auto caps = m_device.physicalDevice().getSurfaceCapabilitiesKHR(*m_window.getSurface());
    
    // Calculate extent
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        { m_extent = caps.currentExtent; }
    else
    {
        const vk::Extent2D windowExtent = m_window.getExtent();
        m_extent.width = std::clamp(windowExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        m_extent.height = std::clamp(windowExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    
    // Determine image count: Aim for triple buffering (3) for Mailbox mode
    const uint32_t maxImages = (caps.maxImageCount == 0) 
                             ? std::numeric_limits<uint32_t>::max() 
                             : caps.maxImageCount;
    uint32_t imageCount = std::clamp(3u, caps.minImageCount, maxImages);
    
    // Create info - reuse old swapchain if it exists
    const vk::SwapchainCreateInfoKHR createInfo{
        .surface = *m_window.getSurface(),
        .minImageCount = imageCount,
        .imageFormat = m_imageFormat,
        .imageColorSpace = m_colorSpace,
        .imageExtent = m_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = m_sharingMode,
        .queueFamilyIndexCount = static_cast<uint32_t>(m_sharingQueueFamilyIndices.size()),
        .pQueueFamilyIndices = m_sharingQueueFamilyIndices.data(),
        .preTransform = caps.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = m_presentMode,
        .clipped = vk::True,
        .oldSwapchain = *m_swapchain
    };
    
    // Create new swapchain
    m_swapchain = vk::raii::SwapchainKHR(m_device.device(), createInfo);
    
    // Get images
    m_images = m_swapchain.getImages();
    
    // Create image views
    for (const auto& image : m_images)
    {
        const vk::ImageViewCreateInfo viewCreateInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = m_imageFormat,
            .components = {
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity
            },
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        
        m_imageViews.emplace_back(m_device.device(), viewCreateInfo);
    }
    
    // Create render finished semaphores
    for (size_t i = 0; i < m_images.size(); ++i)
        { m_renderFinishedSemaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo{}); }
}

const std::optional<svk::SwapchainFrame> Swapchain::acquireNextImage(vk::Semaphore imageAvailableSemaphore) const
{
    try
    {
        const auto result = m_swapchain.acquireNextImage(UINT64_MAX, imageAvailableSemaphore, nullptr);
        const uint32_t imageIndex = result.second;
        
        return svk::SwapchainFrame{
            .imageIndex = imageIndex,
            .image = m_images[imageIndex],
            .imageView = *m_imageViews[imageIndex],
            .currentExtent = m_extent,
            .renderFinishedSemaphore = *m_renderFinishedSemaphores[imageIndex]
        };
    }
    catch (const vk::OutOfDateKHRError&)
        { return std::nullopt; }
}

bool Swapchain::present(const svk::SwapchainFrame& frame) const
{
    const vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &*m_swapchain,
        .pImageIndices = &frame.imageIndex
    };
    
    const vk::Result result = m_presentQueue.present(presentInfo);
    
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
        { return false; }
    return true;
}

} // namespace svk
