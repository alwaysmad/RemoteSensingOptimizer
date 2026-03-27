// src/EngineInstance.cpp

#include <string> // std::string

#include "EngineInstance.hpp"

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
	  m_swapchain(m_device, m_window)
{
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
}

bool EngineInstance::shouldClose() const { return m_window.shouldClose(); }
