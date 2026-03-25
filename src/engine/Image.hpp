// src/engine/Image.hpp
#pragma once

#include <atomic>
#include <vulkan/vulkan_raii.hpp>

namespace svk
{

class Device;

class Image
{
public:
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    ~Image();

    [[nodiscard]] inline vk::Format getFormat() const { return m_format; }
    [[nodiscard]] inline vk::Extent3D getExtent() const { return m_extent; }
    [[nodiscard]] inline vk::ImageUsageFlags getUsage() const { return m_usage; }

    [[nodiscard]] inline const vk::raii::Image& getImage() const { return m_image; }
    [[nodiscard]] inline const vk::raii::ImageView& getImageView() const { return m_imageView; }

private:
    friend class Device;

    Image(const vk::raii::Device& device,
          vk::raii::Image&& image,
          const vk::ImageCreateInfo& imageInfo,
          uint32_t memoryTypeIndex,
          vk::ImageAspectFlags aspectFlags,
          std::atomic<uint32_t>& allocCounter);

    std::atomic<uint32_t>* m_allocCounter = nullptr;

    vk::Format m_format = vk::Format::eUndefined;
    vk::Extent3D m_extent {};
    vk::ImageUsageFlags m_usage {};

    vk::raii::Image m_image = nullptr;
    vk::raii::DeviceMemory m_memory = nullptr;
    vk::raii::ImageView m_imageView = nullptr;
};

} // namespace svk
