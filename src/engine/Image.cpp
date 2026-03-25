// src/engine/Image.cpp
#include <atomic>
#include <stdexcept>
#include <utility>

#include "engine/Image.hpp"

namespace svk
{

Image::Image(const vk::raii::Device& device,
             vk::raii::Image&& image,
             const vk::ImageCreateInfo& imageInfo,
             uint32_t memoryTypeIndex,
             vk::ImageAspectFlags aspectFlags,
             std::atomic<uint32_t>& allocCounter)
    : m_format(imageInfo.format),
      m_extent(imageInfo.extent),
      m_usage(imageInfo.usage)
{
    m_image = std::move(image);

    const vk::MemoryRequirements memoryRequirements = m_image.getMemoryRequirements();

    m_memory = vk::raii::DeviceMemory(
        device,
        vk::MemoryAllocateInfo {
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = memoryTypeIndex,
        });

    m_image.bindMemory(*m_memory, 0);

    const vk::ImageViewType viewType = m_extent.depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D;
    const vk::ImageViewCreateInfo viewInfo {
        .image = *m_image,
        .viewType = viewType,
        .format = m_format,
        .subresourceRange = vk::ImageSubresourceRange {
            .aspectMask = aspectFlags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    m_imageView = vk::raii::ImageView(device, viewInfo);

    m_allocCounter = &allocCounter;
    m_allocCounter->fetch_add(1, std::memory_order_relaxed);
}

Image::Image(Image&& other) noexcept
    : m_allocCounter(std::exchange(other.m_allocCounter, nullptr)),
      m_format(std::exchange(other.m_format, vk::Format::eUndefined)),
      m_extent(std::exchange(other.m_extent, vk::Extent3D {})),
      m_usage(std::exchange(other.m_usage, vk::ImageUsageFlags {})),
      m_image(std::exchange(other.m_image, nullptr)),
      m_memory(std::exchange(other.m_memory, nullptr)),
      m_imageView(std::exchange(other.m_imageView, nullptr))
{}

Image& Image::operator=(Image&& other) noexcept
{
    if (this != &other)
    {
        if (m_allocCounter != nullptr)
            { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }

        m_allocCounter = std::exchange(other.m_allocCounter, nullptr);
        m_format = std::exchange(other.m_format, vk::Format::eUndefined);
        m_extent = std::exchange(other.m_extent, vk::Extent3D {});
        m_usage = std::exchange(other.m_usage, vk::ImageUsageFlags {});
        m_image = std::exchange(other.m_image, nullptr);
        m_memory = std::exchange(other.m_memory, nullptr);
        m_imageView = std::exchange(other.m_imageView, nullptr);
    }
    return *this;
}

Image::~Image()
{
    if (m_allocCounter != nullptr)
        { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }
}

} // namespace svk
