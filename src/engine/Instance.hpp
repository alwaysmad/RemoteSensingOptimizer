// src/engine/Instance.hpp
#pragma once

#include <span>   // std::span for required extensions
#include <string> // std::string for app name
#include <vulkan/vulkan_raii.hpp>

#include "engine/ConsoleLogger.hpp"

namespace svk
{
class Instance;
}

class svk::Instance
{
private:
	vk::raii::Context m_context;
	vk::raii::Instance m_instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;

	static constexpr const char* engineName = "svk";
	static constexpr const char* validationLayersName = "VK_LAYER_KHRONOS_validation";

	static constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags =
	    vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
	    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

	static constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags =
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

	// Debug callback function for Vulkan validation layers
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
	                                                      vk::DebugUtilsMessageTypeFlagsEXT type,
	                                                      const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	                                                      void* pUserData);

public:
	Instance(const std::string& appName, std::span<const char* const> requiredExtensions, const ConsoleLogger& logger);
	~Instance() = default;

	[[nodiscard]] const vk::raii::Instance& getInstance() const { return m_instance; }

	// No copy, no Move
	Instance(const Instance&) noexcept = delete;
	Instance& operator=(const Instance&) noexcept = delete;
	Instance(Instance&&) noexcept = delete;
	Instance& operator=(Instance&&) noexcept = delete;
};
