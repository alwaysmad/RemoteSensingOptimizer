#pragma once

#include <vector>    // std::vector for required Vulkan extensions
#include <stdexcept> // std::runtime_error on GLFW init failure
#include <GLFW/glfw3.h>

namespace svk
{
class WindowContext
{
public:
	WindowContext(const WindowContext&) = delete;
	WindowContext& operator=(const WindowContext&) = delete;
	WindowContext(WindowContext&& other) noexcept = delete;
	WindowContext& operator=(WindowContext&& other) = delete;

	WindowContext()
	{
		if (glfwInit() != GLFW_TRUE)
		{
			throw std::runtime_error("Failed to initialize GLFW");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	}

	~WindowContext()
	{
		glfwTerminate();
	}

	[[nodiscard]] inline std::vector<const char*> getRequiredInstanceExtensions() const
	{
		uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);
		return std::vector<const char*>(extensions, extensions + count);
	}
};
}
