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
class DepthResources;
}

// =========================================================================
//  Manages the lifecycle of depth buffers across window resizes and frames.
// =========================================================================
class svk::DepthResources
{
public:
	[[nodiscard]] static vk::Format pickDepthFormat(const vk::raii::PhysicalDevice& physicalDevice)
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
			{
				return candidate;
			}
		}

		throw std::runtime_error("No supported depth format found for depth resources");
	}

private:
	const svk::Device* m_device = nullptr;
	vk::Format m_depthFormat = vk::Format::eUndefined;
	uint32_t m_imageCount = 0;
	vk::Extent2D m_currentExtent{};
	std::vector<svk::Image> m_images;

	void buildImages()
	{
		const vk::ImageCreateInfo imageInfo{
		    .imageType = vk::ImageType::e2D,
		    .format = m_depthFormat,
		    .extent = vk::Extent3D{m_currentExtent.width, m_currentExtent.height, 1},
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
			m_images.emplace_back(m_device->createImage(
			    imageInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageAspectFlagBits::eDepth));
		}
	}

public:
	// Takes ownership of the depth format and initializes the memory.
	DepthResources(const svk::Device& device, vk::Extent2D initialExtent, uint32_t imageCount)
	    : m_device(&device), m_depthFormat(pickDepthFormat(device.physicalDevice())), m_imageCount(imageCount),
	      m_currentExtent(initialExtent)
	{
		m_images.reserve(m_imageCount);
		buildImages();
	}

	// Ironclad Constraints: Non-Copyable
	DepthResources(const DepthResources&) = delete;
	DepthResources& operator=(const DepthResources&) = delete;

	// Default Movable: The RAII handles inside svk::Image manage themselves safely
	DepthResources(DepthResources&&) noexcept = default;
	DepthResources& operator=(DepthResources&&) noexcept = default;

	~DepthResources() = default;

	// Accessors
	[[nodiscard]] const svk::Image& operator[](uint32_t currentFrame) const { return m_images[currentFrame]; }
	[[nodiscard]] vk::Format getFormat() const { return m_depthFormat; }
	[[nodiscard]] vk::Extent2D getExtent() const { return m_currentExtent; }

	// Destroys old depth images and provisions new ones matching the window.
	void recreate(vk::Extent2D newExtent)
	{
		m_currentExtent = newExtent;
		m_images.clear();
		buildImages();
	}
};