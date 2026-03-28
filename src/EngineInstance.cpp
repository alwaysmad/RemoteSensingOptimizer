// src/EngineInstance.cpp

#include <cstring>
#include <string> // std::string

#include "EngineInstance.hpp"
#include "engine/Frames.hpp"

#include "triangle.hpp"

EngineInstance::EngineInstance(Settings& settings, svk::Logger& logger, const Mesh& mesh)
	: m_settings(settings),
	  m_logger(logger),
	  m_mesh(mesh),
	  m_windowContext(),
	  m_instance(std::string(Settings::appName), m_windowContext.getRequiredInstanceExtensions(), m_logger),
	  m_window(m_windowContext, m_instance.getInstance(), m_settings.windowWidth, m_settings.windowHeight, std::string(Settings::appName)),
	  m_device([this]() {
		if constexpr (svk::enableValidationLayers)
		{
			m_logger.cInfo("Available physical devices:");
			for (const auto& physicalDevice : m_instance.getInstance().enumeratePhysicalDevices())
			{
				const auto props = physicalDevice.getProperties();
				const auto availableDeviceName = std::string(props.deviceName.data());
				m_logger.cInfo("  - {} (api {}.{}.{})",
					availableDeviceName,
					VK_API_VERSION_MAJOR(props.apiVersion),
					VK_API_VERSION_MINOR(props.apiVersion),
					VK_API_VERSION_PATCH(props.apiVersion));
			}

			m_logger.cInfo("Creating device '{}'", m_settings.deviceName);
		}
		return svk::Device(m_instance, m_window.getSurface(), m_settings.deviceName);
	}()),
	  m_swapchain(m_device, m_window),
	  m_renderRoutine(m_device, m_swapchain, svk::MAX_FRAMES_IN_FLIGHT),
	  m_transferRoutine(m_device, 1)
{
	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{ m_inFlightFences.emplace_back(m_device.device(), vk::FenceCreateInfo {.flags = vk::FenceCreateFlagBits::eSignaled}); }

	const vk::DeviceSize vertexBytes = static_cast<vk::DeviceSize>(sizeof(VertexCoords) * m_mesh.vertices.size());
	const vk::DeviceSize indexBytes = static_cast<vk::DeviceSize>(sizeof(uint32_t) * m_mesh.indices.size());

	// Create final (device local) buffers
	m_vertexBuffer.emplace(m_device.createBuffer(
		vertexBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{svk::Device::TRANSFER, svk::Device::GRAPHICS}));

	m_indexBuffer.emplace(m_device.createBuffer(
		indexBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{svk::Device::TRANSFER, svk::Device::GRAPHICS}));
	
	{ // Upload vertex data via staging buffer
		auto vertexStaging = m_device.createBuffer(
			vertexBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{svk::Device::TRANSFER});
		{ // Map to copy
			auto map = vertexStaging.map(0, vertexBytes);
			std::memcpy(map.get(), m_mesh.vertices.data(), static_cast<size_t>(vertexBytes));
		}
		// copy from staging to device local
		m_transferRoutine.bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			vertexStaging,
			*m_vertexBuffer,
			0,
			0,
			vertexBytes);
		m_transferRoutine.submitCommands(0);
		m_device.transferQueue().waitIdle();
	}

	{ // Upload index data via staging buffer
		auto indexStaging = m_device.createBuffer(
			indexBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{svk::Device::TRANSFER});

		{ // Map to copy
			auto map = indexStaging.map(0, indexBytes);
			std::memcpy(map.get(), m_mesh.indices.data(), static_cast<size_t>(indexBytes));
		}
		// copy from staging to device local
		m_transferRoutine.bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			indexStaging,
			*m_indexBuffer,
			0,
			0,
			indexBytes);
		m_transferRoutine.submitCommands(0);
		m_device.transferQueue().waitIdle();
	}

	{ // Create shadermodule and render task for the triangle
		const vk::raii::ShaderModule triangleModule(m_device.device(), triangle::smci);
		const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = *triangleModule,
				.pName = "vertMain",
			},
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = *triangleModule,
				.pName = "fragMain",
			},
		};

		const vk::PipelineVertexInputStateCreateInfo vertexInput {
			.vertexBindingDescriptionCount = static_cast<uint32_t>(kVertexCoordsBindingDescriptions.size()),
			.pVertexBindingDescriptions = kVertexCoordsBindingDescriptions.data(),
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(kVertexCoordsAttributeDescriptions.size()),
			.pVertexAttributeDescriptions = kVertexCoordsAttributeDescriptions.data(),
		};
		auto& triangleTask = m_renderRoutine.m_tasks.emplace_back(
			m_device.device(),
			shaderStages,
			vertexInput,
			vk::PrimitiveTopology::eTriangleList,
			vk::CullModeFlagBits::eNone,
			std::vector<vk::DescriptorSetLayoutBinding> {},
			m_swapchain.getFormat(),
			m_renderRoutine.getDepthFormat());
		triangleTask.m_active = true;
		triangleTask.registerBuffers(
			std::vector<svk::BufferBinding> {},
			std::vector<svk::BufferBinding> { svk::BufferBinding(*m_vertexBuffer, 0) },
			std::optional<svk::BufferBinding> { svk::BufferBinding(*m_indexBuffer, 0) },
			static_cast<uint32_t>(m_mesh.indices.size()),
			1);
	}
	if constexpr (svk::enableValidationLayers)
	{
		m_logger.cInfo(
			"Queue binding: transfer={}, compute={}, graphics={}, present={}",
			m_device.transferQueue().getFamilyIndex(),
			m_device.computeQueue().getFamilyIndex(),
			m_device.graphicsQueue().getFamilyIndex(),
			m_device.presentQueue().getFamilyIndex());

		const auto queueFamilies = m_device.physicalDevice().getQueueFamilyProperties();
		for (uint32_t i = 0; i < queueFamilies.size(); ++i)
		{
			const auto flags = queueFamilies[i].queueFlags;
			const bool supportsGraphics = (flags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags {};
			const bool supportsCompute = (flags & vk::QueueFlagBits::eCompute) != vk::QueueFlags {};
			const bool supportsTransfer = (flags & vk::QueueFlagBits::eTransfer) != vk::QueueFlags {};
			const bool supportsPresent = m_device.physicalDevice().getSurfaceSupportKHR(i, *m_window.getSurface());

			m_logger.cInfo(
				"Queue family {}: count={}, graphics={}, compute={}, transfer={}, present={}",
				i,
				queueFamilies[i].queueCount,
				supportsGraphics,
				supportsCompute,
				supportsTransfer,
				supportsPresent);
		}
	}
}

void EngineInstance::tick()
{
	m_window.pollEvents();
	m_window.updateFPS(std::string(Settings::appName));

	const vk::Fence fence = *m_inFlightFences[m_currentFrame];
	if (m_device.device().waitForFences({fence}, vk::True, UINT64_MAX) != vk::Result::eSuccess)
		{ throw std::runtime_error("Fence wait failed"); }
	m_device.device().resetFences({fence});

	m_renderRoutine.draw(m_currentFrame, fence, nullptr);
	m_currentFrame = svk::advanceFrame(m_currentFrame);
}

bool EngineInstance::shouldClose() const { return m_window.shouldClose(); }

EngineInstance::~EngineInstance()
{
	m_device.waitIdle();
}
