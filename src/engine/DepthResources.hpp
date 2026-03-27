// src/engine/DepthResources.hpp
#pragma once

#include <array>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Device.hpp"
#include "engine/Image.hpp"

namespace svk 
{

[[nodiscard]] inline vk::Format pickDepthFormat(const vk::raii::PhysicalDevice& physicalDevice)
{
    static constexpr std::array<vk::Format, 3> depthCandidates = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
    };

    for (const vk::Format candidate : depthCandidates)
    {
        const vk::FormatProperties props = physicalDevice.getFormatProperties(candidate);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            { return candidate; }
    }

    throw std::runtime_error("No supported depth format found for depth resources");
}

// =========================================================================
// 
//  Manages the lifecycle of depth buffers across window resizes and frames.
// =========================================================================
class DepthResources 
{
public:
    // Takes ownership of the depth format and initializes the memory.
    DepthResources(
        const svk::Device& device,
        vk::Extent2D initialExtent,
        uint32_t imageCount)
        : m_device(&device),
        m_depthFormat(svk::pickDepthFormat(device.physicalDevice())),
        m_imageCount(imageCount),
        m_currentExtent(initialExtent)
        { m_images.reserve(m_imageCount); buildImages(); }

    // Ironclad Constraints: Non-Copyable
    DepthResources(const DepthResources&) = delete;
    DepthResources& operator=(const DepthResources&) = delete;

    // Default Movable: The RAII handles inside svk::Image manage themselves safely
    DepthResources(DepthResources&&) noexcept = default;
    DepthResources& operator=(DepthResources&&) noexcept = default;

    ~DepthResources() = default;

    // Accessors
    [[nodiscard]] inline const svk::Image& operator[](uint32_t currentFrame) const { return m_images[currentFrame]; }
    [[nodiscard]] inline vk::Format getFormat() const { return m_depthFormat; }
    [[nodiscard]] inline vk::Extent2D getExtent() const { return m_currentExtent; }

    // Destroys old depth images and provisions new ones matching the window.
    void inline recreate(vk::Extent2D newExtent)
    {
        m_currentExtent = newExtent;
        m_images.clear();
        buildImages();
    }

private:
    const svk::Device* m_device = nullptr;
    const vk::Format m_depthFormat;
    const uint32_t m_imageCount;
    
    vk::Extent2D m_currentExtent;
    
    std::vector<svk::Image> m_images;

    inline void buildImages()
    {
        const vk::ImageCreateInfo imageInfo {
            .imageType = vk::ImageType::e2D,
            .format = m_depthFormat,
            .extent = vk::Extent3D { m_currentExtent.width, m_currentExtent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };

        for (uint32_t i = 0; i < m_imageCount; ++i)
        {
            m_images.emplace_back(
                m_device->createImage(
                    imageInfo,
                    vk::MemoryPropertyFlagBits::eDeviceLocal,
                    vk::ImageAspectFlagBits::eDepth));
        }
    }
};

} // namespace svk