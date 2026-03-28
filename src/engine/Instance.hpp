// src/engine/Instance.hpp
#pragma once

#include <string> // std::string for app name
#include <vector> // std::vector for required extensions
#include <vulkan/vulkan_raii.hpp>

namespace svk
{
class Instance
{
public:
	Instance(const std::string& appName, const std::vector<const char*>& requiredExtensions, const Logger& logger);
	~Instance() = default;

	[[nodiscard]] inline const vk::raii::Instance& getInstance() const { return m_instance; }

	// No copy, no Move
	Instance(const Instance&) noexcept = delete;
	Instance& operator= (const Instance&) noexcept = delete;
	Instance(Instance&&) noexcept = delete;
	Instance& operator= (Instance&&) noexcept = delete;

private:
	vk::raii::Context m_context;
	vk::raii::Instance m_instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
		vk::DebugUtilsMessageTypeFlagsEXT type,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
};
}
