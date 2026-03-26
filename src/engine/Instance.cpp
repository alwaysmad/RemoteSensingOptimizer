// src/engine/Instance.cpp
#include <algorithm>  // std::ranges::none_of
#include <cstring>    // std::strcmp
#include <stdexcept>  // std::runtime_error
#include <string>     // std::string
#include <string_view> // std::string_view
#include <vector>     // std::vector
#include<format>

#include "engine/Flags.hpp"
#include "engine/Instance.hpp"
#include "engine/Logger.hpp"

namespace
{
    constexpr const char* engineName = "svk";
    constexpr const char* validationLayersName = "VK_LAYER_KHRONOS_validation";

	constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
	);
	constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
	);
}

namespace svk
{
Instance::Instance(const std::string& appName, const std::vector<const char*>& extensions, Logger& logger)
	: m_logger(logger), m_debugMessenger(nullptr)
{
	const vk::ApplicationInfo appInfo {
		.pApplicationName = appName.c_str(),
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = engineName,
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = vk::ApiVersion13
	};

	std::vector<const char*> requiredExtensions = extensions;
	if constexpr (svk::enableValidationLayers)
		{ requiredExtensions.emplace_back(vk::EXTDebugUtilsExtensionName); }

	const auto extensionProperties = m_context.enumerateInstanceExtensionProperties();

	if constexpr (svk::enableValidationLayers)
	{
		m_logger.cDebug("Available Vulkan extensions ({}) :", extensionProperties.size());
		for ([[maybe_unused]] const auto& i : extensionProperties)
			{ m_logger.cDebug("\t{}", std::string_view(i.extensionName.data())); }

		m_logger.cDebug("Required extensions ({}) :", requiredExtensions.size());
		for ([[maybe_unused]] const auto& name : requiredExtensions)
			{ m_logger.cDebug("\t{}", name); }
	}

	for (const auto& requiredExtension : requiredExtensions)
	{
		if (std::ranges::none_of(
				extensionProperties,
				[requiredExtension](auto const& extensionProperty)
				{ return std::strcmp(extensionProperty.extensionName, requiredExtension) == 0; }
			))
			{ throw std::runtime_error(std::format("Required extension not supported: {}", requiredExtension)); }
	}

	std::vector<const char*> requiredLayers;
	if constexpr (svk::enableValidationLayers)
		{ requiredLayers.emplace_back(validationLayersName); }

	const auto layerProperties = m_context.enumerateInstanceLayerProperties();

	if constexpr (svk::enableValidationLayers)
	{
		m_logger.cDebug("Available layers ({}) :", layerProperties.size());
		for ([[maybe_unused]] const auto& i : layerProperties)
			{ m_logger.cDebug("\t{}", std::string_view(i.layerName.data())); }

		m_logger.cDebug("Required layers ({}) :", requiredLayers.size());
		for ([[maybe_unused]] const auto& name : requiredLayers)
			{ m_logger.cDebug("\t{}", name); }
	}

	for (const auto& requiredLayer : requiredLayers)
	{
		if (std::ranges::none_of(
				layerProperties,
				[requiredLayer](auto const& layerProperty)
				{ return std::strcmp(layerProperty.layerName, requiredLayer) == 0; }
			))
			{ throw std::runtime_error(std::format("Required layer not supported: {}", requiredLayer)); }
	}

	const vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT {
		.messageSeverity = severityFlags,
		.messageType = messageTypeFlags,
		.pfnUserCallback = &debugCallback,
		.pUserData = &m_logger
	};

	const vk::InstanceCreateInfo createInfo {
		.pNext = (svk::enableValidationLayers) ? &debugUtilsMessengerCreateInfoEXT : nullptr,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
		.ppEnabledLayerNames = requiredLayers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data()
	};

	m_instance = vk::raii::Instance(m_context, createInfo);
	m_logger.cDebug("Vulkan instance created successfully");

	if constexpr (svk::enableValidationLayers)
	{
		m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
		m_logger.cDebug("Debug callback set up successfully");
	}
	m_logger.cDebug("Instance initialized");
}

Instance::~Instance()
{
	m_logger.cDebug("Instance destroyed");
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Instance::debugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	vk::DebugUtilsMessageTypeFlagsEXT type,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	const Logger* logger = static_cast<Logger*>(pUserData);
	const std::string typeStr = vk::to_string(type);
	if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{ logger->cError("{} {}", typeStr, pCallbackData->pMessage); }
	else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{ logger->cWarn("{} {}", typeStr, pCallbackData->pMessage); }
	else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
		{ logger->fInfo("{} {}", typeStr, pCallbackData->pMessage); }
	else
	{ logger->fDebug("{} {}", typeStr, pCallbackData->pMessage); }

	return vk::False;
}
}
