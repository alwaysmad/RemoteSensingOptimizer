// src/engine/window/Camera.hpp
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_raii.hpp>

// Forward declare to avoid including glfw3.h in the header
struct GLFWwindow;

namespace svk 
{
// =========================================================================
//  
//  Holds all mathematical state for the Arcball Camera.
// =========================================================================
struct CameraState 
{
    double theta = 0.0;
    double phi = 1.1;
    double radius = 3.35;
    
    double lastX = 0.0;
    double lastY = 0.0;
    
    bool isDragging = false;
    bool isPaused = false;

    uint32_t width = 800;
    uint32_t height = 600;

    // MANUAL PROJECTION MATRIX CONSTRUCTION
	// FOV: 45 degrees
	// Near: 1.0f
	// Far: Infinity
	// Z-Range: [0, 1] (Standard Vulkan)
	
	// f = 1.0 / tan(fov / 2)
	// For 45 deg, tan(22.5) approx 0.4142 -> f approx 2.4142
	static constexpr float f = 2.41421356f;
	static constexpr float near = 0.05f;

	static constexpr glm::mat4 proj_unscaled = {
		f,        0.0f,	 0.0f,	 0.0f,	// Col 0 (Right)
		0.0f,     -f,	 0.0f,	 0.0f,	// Col 1 (Up-ish)
		0.0f,	  0.0f,	-1.0f,	-1.0f,	// Col 2 (Forward-ish)
		0.0f,     0.0f,	-near,	 0.0f	// Col 3 (Translation)
	};
    glm::mat4 viewProj = glm::mat4(1.0f);
};
// =========================================================================
// Because GLFW only has one user pointer
// creating a second camera will incvalidate the callbacks of the first one.
// =========================================================================
class Camera 
{
public:
    // Ironclad Constraints
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    Camera(GLFWwindow* window, vk::Extent2D initialExtent);

    // Accessors
    [[nodiscard]] inline const glm::mat4& getViewProj() const { return m_state.viewProj; }
    [[nodiscard]] inline bool isPaused() const { return m_state.isPaused; }

private:
    GLFWwindow* m_window = nullptr;
    CameraState m_state;

    // GLFW C-Callbacks
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
};

} // namespace svk