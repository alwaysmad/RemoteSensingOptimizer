#include "engine/window/Camera.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

namespace
{

[[nodiscard]] inline svk::CameraState* getCameraState(GLFWwindow* window)
    { return reinterpret_cast<svk::CameraState*>(glfwGetWindowUserPointer(window)); }

inline void updateViewProj(svk::CameraState& state)
{
    // Spherical to Cartesian
	// Y-Up coordinate system
    const float x = static_cast<float>(state.radius * std::sin(state.phi) * std::sin(state.theta));
    const float y = static_cast<float>(state.radius * std::cos(state.phi));
    const float z = static_cast<float>(state.radius * std::sin(state.phi) * std::cos(state.theta));

    const glm::vec3 eye(x, y, z);
    const glm::vec3 center(0.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4 proj = svk::CameraState::proj_unscaled;
    const float minDim = static_cast<float>(std::min(state.width, state.height));
    if (state.width != 0u)
        { proj[0][0] *= minDim / static_cast<float>(state.width); }
    if (state.height != 0u)
        { proj[1][1] *= minDim / static_cast<float>(state.height); }

    const glm::mat4 view = glm::lookAt(eye, center, up);
    state.viewProj = proj * view;
}

} // namespace

namespace svk
{

Camera::Camera(GLFWwindow* window, vk::Extent2D initialExtent)
    : m_window(window)
{
    m_state.width = initialExtent.width;
    m_state.height = initialExtent.height;
    updateViewProj(m_state);

    glfwSetWindowUserPointer(window, &m_state);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
}

void Camera::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    CameraState* state = getCameraState(window);
    if (!state || !state->isDragging) { return; }

    // Delta movement
    const double dx = xpos - state->lastX;
    const double dy = ypos - state->lastY;
    state->lastX = xpos;
    state->lastY = ypos;

    // Update Angles
    constexpr double sensitivity = 0.005;
    state->theta -= dx * sensitivity;
    state->phi -= dy * sensitivity;

    // Clamp Phi (prevent flipping over the pole)
    constexpr double epsilon = 0.01;
    state->phi = std::max( epsilon, std::min(glm::pi<double>() - epsilon, state->phi));

    updateViewProj(*state);
}

void Camera::mouseButtonCallback(GLFWwindow* window, int button, int action, [[maybe_unused]] int mods)
{
    CameraState* state = getCameraState(window);
    if (!state) { return; }

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            state->isDragging = true;
            glfwGetCursorPos(window, &state->lastX, &state->lastY);
        }
        else if (action == GLFW_RELEASE)
            { state->isDragging = false; }
    }
}

void Camera::scrollCallback(GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset)
{
    CameraState* state = getCameraState(window);
    if (!state) { return; }

    constexpr double zoomSpeed = 0.2;
    state->radius -= yoffset * zoomSpeed;

    constexpr double minRadius = 0.5;
    constexpr double maxRadius = 20.0;
    if (state->radius < minRadius)
        { state->radius = minRadius; }
    if (state->radius > maxRadius)
        { state->radius = maxRadius; }

    updateViewProj(*state);
}

void Camera::keyCallback(GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
{
    CameraState* state = getCameraState(window);
    if (!state) { return; }

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        { state->isPaused = !state->isPaused; }
}

void Camera::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    CameraState* state = getCameraState(window);
    if (!state) { return; }

    state->width = static_cast<uint32_t>(std::max(width, 0));
    state->height = static_cast<uint32_t>(std::max(height, 0));
    updateViewProj(*state);
}

} // namespace svk
