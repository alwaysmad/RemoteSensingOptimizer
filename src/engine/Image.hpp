// src/engine/Image.hpp
#pragma once

#include <atomic>
#include <utility> // for std::exchange
#include <vulkan/vulkan_raii.hpp>

namespace svk { class Device; class Image; }

class svk::Image
{
private:
    std::atomic<uint32_t>* m_allocCounter = nullptr; // points to the Device's allocation counter

    vk::Format m_format = vk::Format::eUndefined;
    vk::Extent3D m_extent {};
    vk::ImageUsageFlags m_usage;

    vk::raii::Image m_image = nullptr;
    vk::raii::DeviceMemory m_memory = nullptr;
    vk::raii::ImageView m_imageView = nullptr;

    // Only Device can create Images
    friend class Device;
    // The handles are pre-allocated by the Device and sunk into this wrapper.
    Image(const vk::raii::Device& device,
          vk::raii::Image&& image,
          vk::raii::DeviceMemory&& memory,
          vk::Format format,
          vk::Extent3D extent,
          vk::ImageUsageFlags usage,
          vk::ImageAspectFlags aspectFlags,
          std::atomic<uint32_t>& allocCounter)
        : m_allocCounter(&allocCounter),
          m_format(format),
          m_extent(extent),
          m_usage(usage),
          m_image(std::move(image)),
          m_memory(std::move(memory))
    {
        m_image.bindMemory(*m_memory, 0);
        const vk::ImageViewCreateInfo viewInfo {
            .image = *m_image,
            .viewType = vk::ImageViewType::e2D,
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
        m_allocCounter->fetch_add(1, std::memory_order_relaxed);
    }

public:
    // Accessors
    [[nodiscard]] vk::Format getFormat() const { return m_format; }
    [[nodiscard]] vk::Extent3D getExtent() const { return m_extent; }
    [[nodiscard]] vk::ImageUsageFlags getUsage() const { return m_usage; }

    [[nodiscard]] const vk::raii::Image& getImage() const { return m_image; }
    [[nodiscard]] const vk::raii::ImageView& getImageView() const { return m_imageView; }

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) noexcept
        : m_allocCounter(std::exchange(other.m_allocCounter, nullptr)),
          m_format(std::exchange(other.m_format, vk::Format::eUndefined)),
          m_extent(std::exchange(other.m_extent, vk::Extent3D {})),
          m_usage(std::exchange(other.m_usage, vk::ImageUsageFlags {})),
          m_image(std::exchange(other.m_image, nullptr)),
          m_memory(std::exchange(other.m_memory, nullptr)),
          m_imageView(std::exchange(other.m_imageView, nullptr))
    {}

    Image& operator=(Image&& other) noexcept
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

    ~Image()
    {
        if (m_allocCounter != nullptr)
            { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }
    }
};