// src/engine/Instance.cpp
#include <stdexcept>     // std::runtime_error
#include <string>        // std::string
#include <string_view>   // std::string_view
#include <unordered_set> // std::unordered_set
#include <vector>        // std::vector

#include "engine/ConsoleLogger.hpp"
#include "engine/Flags.hpp"
#include "engine/Instance.hpp"

svk::Instance::Instance(const std::string& appName,
                        std::span<const char* const> extensions,
                        const ConsoleLogger& logger)
{
	const vk::ApplicationInfo appInfo{.pApplicationName = appName.c_str(),
	                                  .applicationVersion = vk::makeVersion(1, 0, 0),
	                                  .pEngineName = engineName,
	                                  .engineVersion = vk::makeVersion(1, 0, 0),
	                                  .apiVersion = vk::ApiVersion13};

	// Create a vector of required extensions
	std::vector<const char*> requiredExtensions;
	requiredExtensions.reserve(extensions.size() + (svk::enableValidationLayers ? 1 : 0));
	requiredExtensions.assign(extensions.begin(), extensions.end()); // copy the provided extensions
	if constexpr (svk::enableValidationLayers) // Add the debug extension if validation layers are enabled
	{
		requiredExtensions.emplace_back(vk::EXTDebugUtilsExtensionName);
	}

	const auto extensionProperties = m_context.enumerateInstanceExtensionProperties();

	// Create a set of available extensions for quick lookup
	std::unordered_set<std::string_view> availableExtensions;
	availableExtensions.reserve(extensionProperties.size());
	for (const auto& prop : extensionProperties) { availableExtensions.emplace(prop.extensionName.data()); }

	if constexpr (svk::enableValidationLayers)
	{
		logger.cDebug("Available Vulkan extensions ({}) :", extensionProperties.size());
		for ([[maybe_unused]] const auto& i : extensionProperties)
		{
			logger.cDebug("\t{}", std::string_view(i.extensionName.data()));
		}

		logger.cDebug("Required extensions ({}) :", requiredExtensions.size());
		for ([[maybe_unused]] const auto& name : requiredExtensions) { logger.cDebug("\t{}", name); }
	}

	// Check if all required extensions are available
	for (const auto& requiredExtension : requiredExtensions)
	{
		if (!availableExtensions.contains(requiredExtension))
		{
			throw std::runtime_error(std::string("Required extension not supported: ") + requiredExtension);
		}
	}

	// Create a vector of required layers
	std::vector<const char*> requiredLayers;
	if constexpr (svk::enableValidationLayers) { requiredLayers.emplace_back(validationLayersName); }

	const auto layerProperties = m_context.enumerateInstanceLayerProperties();

	// Create a set of available layers for quick lookup
	std::unordered_set<std::string_view> availableLayers;
	availableLayers.reserve(layerProperties.size());
	for (const auto& prop : layerProperties) { availableLayers.emplace(prop.layerName.data()); }

	if constexpr (svk::enableValidationLayers)
	{
		logger.cDebug("Available layers ({}) :", layerProperties.size());
		for ([[maybe_unused]] const auto& i : layerProperties)
		{
			logger.cDebug("\t{}", std::string_view(i.layerName.data()));
		}

		logger.cDebug("Required layers ({}) :", requiredLayers.size());
		for ([[maybe_unused]] const auto& name : requiredLayers) { logger.cDebug("\t{}", name); }
	}

	// Check if all required layers are available
	for (const auto& requiredLayer : requiredLayers)
	{
		if (!availableLayers.contains(requiredLayer))
		{
			throw std::runtime_error(std::string("Required layer not supported: ") + requiredLayer);
		}
	}

	// Create the debug messenger
	const vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
	    .messageSeverity = severityFlags,
	    .messageType = messageTypeFlags,
	    .pfnUserCallback = &debugCallback,
	    .pUserData =
	        static_cast<void*>(const_cast<ConsoleLogger*>(&logger)) // NOLINT(cppcoreguidelines-pro-type-const-cast)
	};

	// Include the debug messenger during instance creation
	const vk::InstanceCreateInfo createInfo{.pNext = (svk::enableValidationLayers) ? &debugUtilsMessengerCreateInfoEXT
	                                                                               : nullptr,
	                                        .pApplicationInfo = &appInfo,
	                                        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
	                                        .ppEnabledLayerNames = requiredLayers.data(),
	                                        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
	                                        .ppEnabledExtensionNames = requiredExtensions.data()};
	m_instance = vk::raii::Instance(m_context, createInfo);

	if constexpr (svk::enableValidationLayers)
	{
		logger.cDebug("Vulkan instance created successfully");
		m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
		logger.cDebug("Debug callback set up successfully");
	}
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL
svk::Instance::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                             vk::DebugUtilsMessageTypeFlagsEXT type,
                             const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                             void* pUserData)
{
	const auto* logger = static_cast<const ConsoleLogger*>(pUserData);
	const std::string typeStr = vk::to_string(type);
	if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
	{
		logger->cError("{} {}", typeStr, pCallbackData->pMessage);
	}
	else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		logger->cWarn("{} {}", typeStr, pCallbackData->pMessage);
	}
	else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
	{
		logger->cInfo("{} {}", typeStr, pCallbackData->pMessage);
	}
	else
	{
		logger->cDebug("{} {}", typeStr, pCallbackData->pMessage);
	}

	return vk::False;
}
