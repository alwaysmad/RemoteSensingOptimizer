#pragma once

#include "GlfwContext.hpp"
#include "VulkanInstance.hpp"
#include "VulkanWindow.hpp"
#include "VulkanDevice.hpp"
#include "VulkanLoader.hpp"
#include "Computer.hpp"
#include "Renderer.hpp"
#include "Mesh.hpp"
#include "Satellite.hpp"

class VulkanApplication
{
private:
	const std::string appName;
	GlfwContext glfwContext;
	VulkanInstance vulkanInstance;
	VulkanWindow vulkanWindow;
	VulkanDevice vulkanDevice;
	VulkanLoader vulkanLoader;
	
	SatelliteNetwork satelliteNetwork;
	Mesh m_mesh;

	Computer computer;
	Renderer renderer;

	// Orchestration Sync (Owned by App)
	std::vector<vk::raii::Fence> m_inFlightFences; 
	std::vector<vk::raii::Semaphore> m_computeFinishedSemaphores;
	std::vector<vk::raii::Semaphore> m_uploadFinishedSemaphores;
	
	// Resource managament helpers
	void fillMesh();
	void updateSatellites(double time);

	// --- CAMERA STATE ---
	struct CameraState {
		float theta = 0.0f;     // Azimuth (Angle around Y)
		float phi = 1.0f;       // Polar (Angle down from Y)
		float radius = 3.5f;    // Distance from center

		// Input state
		bool isDragging = false;
		double lastX = 0.0;
		double lastY = 0.0;
	} m_camera;

	// --- NEW: SIMULATION STATE ---
	bool m_isPaused = false;
	double m_simTime = 0.0;
	
	// Helper to calculate view matrix based on state
	glm::mat4 getCameraView() const;

	// GLFW Callbacks (must be static or simple wrappers)
	static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
public:
	VulkanApplication(const std::string&, const std::string&, uint32_t, uint32_t);
	~VulkanApplication();
	int run();
};
