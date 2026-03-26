// src/engine/DepthResources.hpp
#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Device.hpp"
#include "engine/Image.hpp"

namespace svk 
{
// =========================================================================
// 
//  Manages the lifecycle of depth buffers across window resizes and frames.
// =========================================================================
class DepthResources 
{
public:
    // Takes ownership of the depth format and initializes the memory.
    DepthResources(const svk::Device& device, vk::Format depthFormat, vk::Extent2D initialExtent, uint32_t imageCount);

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

    // Destroys old depth images and provisions new ones matching the window.
    void recreate(vk::Extent2D newExtent);

private:
    void buildImages();

    const svk::Device* m_device = nullptr;
    const vk::Format m_depthFormat;
    const uint32_t m_imageCount;
    
    vk::Extent2D m_currentExtent;
    
    std::vector<svk::Image> m_images;
};

} // namespace svk