// src/engine/window/Window.hpp
#pragma once

#include <cstdint>   // uint32_t dimensions and counters
#include <stdexcept> // std::runtime_error
#include <string>    // std::string title and name

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include "engine/window/WindowContext.hpp"

namespace svk
{
class Window;
}
class svk::Window
{
private:
	const WindowContext& m_windowContext; // GLFW context must be initialized
	const vk::raii::Instance& m_instance;
	GLFWwindow* m_window = nullptr;
	vk::raii::SurfaceKHR m_surface{nullptr};

	double m_lastTime = 0.0;
	uint32_t m_nbFrames = 0;

public:
	Window(const WindowContext& windowContext,
	       const vk::raii::Instance& instance,
	       uint32_t width,
	       uint32_t height,
	       const std::string& name)
	    : m_windowContext(windowContext), m_instance(instance),
	      m_window(glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), name.c_str(), nullptr, nullptr))
	{
		if (!static_cast<bool>(m_window)) { throw std::runtime_error("Failed to create GLFW window"); }

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (static_cast<vk::Result>(glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface)) != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create window surface");
		}

		// Hand off the raw C handle to the RAII surface wrapper:
		m_surface = vk::raii::SurfaceKHR(m_instance, surface);
		m_lastTime = glfwGetTime();
	}

	~Window()
	{
		if (static_cast<bool>(m_window)) { glfwDestroyWindow(m_window); }
	}

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) noexcept = delete;
	Window& operator=(Window&&) noexcept = delete;

	void updateFPS(const std::string& titlePrefix)
	{
		const double currentTime = glfwGetTime();
		m_nbFrames++;
		if (currentTime - m_lastTime >= 1.0)
		{
			const std::string title = titlePrefix + " - " + std::to_string(m_nbFrames) + " FPS";
			glfwSetWindowTitle(m_window, title.c_str());
			m_nbFrames = 0;
			m_lastTime = currentTime;
		}
	}

	[[nodiscard]] const vk::raii::SurfaceKHR& getSurface() const { return m_surface; }
	[[nodiscard]] bool shouldClose() const { return static_cast<bool>(glfwWindowShouldClose(m_window)); }
	void waitEvents() const { glfwWaitEvents(); }
	void pollEvents() const { glfwPollEvents(); }

	[[nodiscard]] vk::Extent2D getExtent() const
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(m_window, &width, &height);
		return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	}

	void setWindowTitle(const std::string& title) { glfwSetWindowTitle(m_window, title.c_str()); }
	[[nodiscard]] double getTime() const { return glfwGetTime(); }
	[[nodiscard]] GLFWwindow* getGLFWwindow() const { return m_window; }
};
