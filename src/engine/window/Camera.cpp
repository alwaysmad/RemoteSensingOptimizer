#include "engine/window/Camera.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

// Needs GLFW includes to compile, but we don't want to expose GLFW in the header, so we use a forward declaration.
inline svk::Camera::CameraState* svk::Camera::getCameraState(GLFWwindow* window)
{
	return reinterpret_cast<CameraState*>(glfwGetWindowUserPointer(window));
}

void svk::Camera::updateViewAndProj(CameraState& state)
{
	// Spherical to Cartesian, Y-up. Trig stays in double; only the final result is float.
	const double sinPhi = std::sin(state.phi);
	const double cosPhi = std::cos(state.phi);
	const double sinTheta = std::sin(state.theta);
	const double cosTheta = std::cos(state.theta);

	const glm::vec3 eye(static_cast<float>(state.radius * sinPhi * sinTheta),
	                    static_cast<float>(state.radius * cosPhi),
	                    static_cast<float>(state.radius * sinPhi * cosTheta));
	const glm::vec3 center(0.0f, 0.0f, 0.0f);
	const glm::vec3 up(0.0f, 1.0f, 0.0f);

	glm::mat4 proj = CameraState::proj_unscaled;
	const float minDim = static_cast<float>(std::min(state.width, state.height));
	if (state.width != 0u) { proj[0][0] *= minDim / static_cast<float>(state.width); }
	if (state.height != 0u) { proj[1][1] *= minDim / static_cast<float>(state.height); }

	state.view = glm::lookAt(eye, center, up);
	state.proj = proj;
}

svk::Camera::Camera(GLFWwindow* window, vk::Extent2D initialExtent) : m_window(window)
{
	// Initialize camera state
	m_state.width = initialExtent.width;
	m_state.height = initialExtent.height;
	updateViewAndProj(m_state);

	// Set GLFW callbacks
	glfwSetWindowUserPointer(window, &m_state);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
}

void svk::Camera::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	CameraState* state = getCameraState(window);
	if (!static_cast<bool>(state)) [[unlikely]] { return; }
	
	if (!state->isDragging) { return; }

	// Delta movement
	const double dx = xpos - state->lastX;
	const double dy = ypos - state->lastY;
	state->lastX = xpos;
	state->lastY = ypos;

	// Update Angles
	state->theta -= dx * kOrbitSensitivity;
	state->phi -= dy * kOrbitSensitivity;

	// Clamp Phi (prevent flipping over the pole)
	state->phi = std::clamp(state->phi, kPolarAngleMargin, glm::pi<double>() - kPolarAngleMargin);

	updateViewAndProj(*state);
}

void svk::Camera::mouseButtonCallback(GLFWwindow* window, int button, int action, [[maybe_unused]] int mods)
{
	CameraState* state = getCameraState(window);
	if (!static_cast<bool>(state)) [[unlikely]] { return; }

	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			state->isDragging = true;
			glfwGetCursorPos(window, &state->lastX, &state->lastY);
		}
		else if (action == GLFW_RELEASE) { state->isDragging = false; }
	}
}

void svk::Camera::scrollCallback(GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset)
{
	CameraState* state = getCameraState(window);
	if (!static_cast<bool>(state)) [[unlikely]] { return; }

	state->radius -= yoffset * kZoomSensitivity;
	state->radius = std::clamp(state->radius, kMinOrbitRadius, kMaxOrbitRadius);

	updateViewAndProj(*state);
}

void svk::Camera::keyCallback(
    GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
{
	CameraState* state = getCameraState(window);
	if (!static_cast<bool>(state)) [[unlikely]] { return; }

	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) { state->isPaused = !state->isPaused; }
}

void svk::Camera::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	CameraState* state = getCameraState(window);
	if (!static_cast<bool>(state)) [[unlikely]] { return; }

	state->width = static_cast<uint32_t>(std::max(width, 0));
	state->height = static_cast<uint32_t>(std::max(height, 0));
	updateViewAndProj(*state);
}
