#include "engine/DepthResources.hpp"

namespace svk
{

DepthResources::DepthResources(
    const svk::Device& device,
    vk::Format depthFormat,
    vk::Extent2D initialExtent,
    uint32_t imageCount)
    : m_device(&device),
      m_depthFormat(depthFormat),
      m_imageCount(imageCount),
      m_currentExtent(initialExtent)
{
    m_images.reserve(m_imageCount);
    buildImages();
}

void DepthResources::recreate(vk::Extent2D newExtent)
{
    m_currentExtent = newExtent;
    m_images.clear();
    buildImages();
}

void DepthResources::buildImages()
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

} // namespace svk
