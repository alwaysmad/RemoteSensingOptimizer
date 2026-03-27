// src/EngineInstance.cpp

#include <string> // std::string

#include "EngineInstance.hpp"
#include "engine/Frames.hpp"
#include "triangle.hpp"

EngineInstance::EngineInstance(Settings& settings, svk::Logger& logger)
	: m_settings(settings),
	  m_logger(logger),
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
	  m_renderRoutine(m_device, m_swapchain, m_device.graphicsQueue(), svk::MAX_FRAMES_IN_FLIGHT)
{
	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{ m_inFlightFences.emplace_back(m_device.device(), vk::FenceCreateInfo {.flags = vk::FenceCreateFlagBits::eSignaled}); }

	{
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

	const vk::PipelineVertexInputStateCreateInfo vertexInput {};
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
		std::vector<svk::BufferBinding> {},
		std::optional<svk::BufferBinding> {},
		3,
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
