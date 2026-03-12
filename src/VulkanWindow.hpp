// src/VulkanWindow.hpp
#pragma once
#include "VulkanInstance.hpp"

class VulkanWindow
{
public:
	VulkanWindow(const VulkanInstance& instance, uint32_t width, uint32_t height, const std::string& name) :
		window(nullptr), surface(nullptr), m_lastTime(0.0), m_nbFrames(0)
	{
		window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
		if (!window) { throw std::runtime_error("Failed to create GLFW window"); }

		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance.getInstance(), window, nullptr, &_surface) != VK_SUCCESS)
			{ throw std::runtime_error("Failed to create window surface"); }

		surface = vk::raii::SurfaceKHR(instance.getInstance(), _surface);

		// Initialize time
		m_lastTime = glfwGetTime();

		LOG_DEBUG("Window and Surface created");
	}
	
	~VulkanWindow()
	{
		if (window) { glfwDestroyWindow(window); }
		LOG_DEBUG("Window and Surface destroyed");
	}

	inline void updateFPS(const std::string& titlePrefix)
	{
		double currentTime = glfwGetTime();
		m_nbFrames++;
		if (currentTime - m_lastTime >= 1.0)
		{
			std::string title = titlePrefix + " - " + std::to_string(m_nbFrames) + " FPS";
			glfwSetWindowTitle(window, title.c_str());
			m_nbFrames = 0;
			m_lastTime = currentTime;
		}
	}

	inline const vk::raii::SurfaceKHR& getSurface() const { return surface; }
	inline bool shouldClose() const { return glfwWindowShouldClose(window); }
	inline void waitEvents() const { glfwWaitEvents(); }
	inline void pollEvents() const { glfwPollEvents(); }

	inline vk::Extent2D getExtent() const
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		return vk::Extent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	}

	inline void setWindowTitle(const std::string& title) { glfwSetWindowTitle(window, title.c_str()); }
	inline double getTime() const { return glfwGetTime(); }
	inline GLFWwindow* getGLFWwindow() const { return window; }

private:
	GLFWwindow* window;
	vk::raii::SurfaceKHR surface;

	// FPS State
	double m_lastTime = 0.0;
	uint32_t m_nbFrames = 0;
};
