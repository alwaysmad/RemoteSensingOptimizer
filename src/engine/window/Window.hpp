// src/engine/window/Window.hpp
#pragma once

#include <cstdint>    // uint32_t dimensions and counters
#include <stdexcept>  // std::runtime_error
#include <string>     // std::string title and name

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include "engine/window/WindowContext.hpp"

namespace svk
{
class Window
{
public:
	Window(const WindowContext& windowContext, const vk::raii::Instance& instance, uint32_t width, uint32_t height, const std::string& name)
		: m_windowContext(windowContext),
		  m_instance(instance)
	{
		m_window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
		if (!m_window)
		    { throw std::runtime_error("Failed to create GLFW window"); }

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface) != VK_SUCCESS)
		    { throw std::runtime_error("Failed to create window surface"); }

		m_surface = vk::raii::SurfaceKHR(m_instance, surface);
		m_lastTime = glfwGetTime();
	}

	~Window() { if (m_window) { glfwDestroyWindow(m_window); } }

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) noexcept = delete;
	Window& operator=(Window&&) noexcept = delete;

	inline void updateFPS(const std::string& titlePrefix)
	{
		double currentTime = glfwGetTime();
		m_nbFrames++;
		if (currentTime - m_lastTime >= 1.0)
		{
			std::string title = titlePrefix + " - " + std::to_string(m_nbFrames) + " FPS";
			glfwSetWindowTitle(m_window, title.c_str());
			m_nbFrames = 0;
			m_lastTime = currentTime;
		}
	}

	[[nodiscard]] inline const vk::raii::SurfaceKHR& getSurface() const { return m_surface; }
	[[nodiscard]] inline bool shouldClose() const { return glfwWindowShouldClose(m_window); }
	inline void waitEvents() const { glfwWaitEvents(); }
	inline void pollEvents() const { glfwPollEvents(); }

	[[nodiscard]] inline vk::Extent2D getExtent() const
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(m_window, &width, &height);
		return vk::Extent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	}

	inline void setWindowTitle(const std::string& title) { glfwSetWindowTitle(m_window, title.c_str()); }
	[[nodiscard]] inline double getTime() const { return glfwGetTime(); }
	[[nodiscard]] inline GLFWwindow* getGLFWwindow() const { return m_window; }

private:
	const WindowContext& m_windowContext;
	const vk::raii::Instance& m_instance;
	GLFWwindow* m_window = nullptr;
	vk::raii::SurfaceKHR m_surface = nullptr;

	double m_lastTime = 0.0;
	uint32_t m_nbFrames = 0;
};
}
