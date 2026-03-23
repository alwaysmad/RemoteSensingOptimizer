#include "VulkanApplication.hpp"

VulkanApplication::VulkanApplication(const std::string& AppName, const std::string& DeviceName, uint32_t w, uint32_t h) :
	appName(AppName),
	glfwContext(),
	vulkanInstance(appName, glfwContext.getRequiredInstanceExtensions()),
	vulkanWindow(vulkanInstance, w, h, appName),
	vulkanDevice(vulkanInstance, &vulkanWindow.getSurface(), DeviceName),
	vulkanLoader(vulkanDevice),
	satelliteNetwork(vulkanDevice, 8), m_mesh(),
	computer(vulkanDevice),
	renderer(vulkanDevice, vulkanWindow, satelliteNetwork)
{
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{	
		// Create Fences (Signaled so we don't wait on first frame)
		constexpr vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };
		m_inFlightFences.emplace_back(vulkanDevice.device(), fenceInfo);
		
		// Create Semaphores
		constexpr vk::SemaphoreCreateInfo semInfo{};
		m_computeFinishedSemaphores.emplace_back(vulkanDevice.device(), semInfo);
		m_uploadFinishedSemaphores.emplace_back(vulkanDevice.device(), semInfo);
	}
	
	// --- SETUP INPUT ---
	GLFWwindow* window = vulkanWindow.getGLFWwindow();
	glfwSetWindowUserPointer(window, this); // Store 'this' so callbacks can access App

	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetKeyCallback(window, keyCallback);

	// Set initial camera angles matching the old "defaultView"
	// Eye approx (0, 1.5, 3.0) -> Radius ~3.35
	m_camera.radius = 3.35f;
	m_camera.theta = 0.0f;
	m_camera.phi = 1.1f; // ~63 degrees down from Y-up

	LOG_DEBUG("VulkanApplication instance created");
}

glm::mat4 VulkanApplication::getCameraView() const
{
	// Spherical to Cartesian
	// Y-Up coordinate system
	const float x = m_camera.radius * std::sin(m_camera.phi) * std::sin(m_camera.theta);
	const float y = m_camera.radius * std::cos(m_camera.phi);
	const float z = m_camera.radius * std::sin(m_camera.phi) * std::cos(m_camera.theta);

	const glm::vec3 eye(x, y, z);
	const glm::vec3 center(0.0f, 0.0f, 0.0f);
	const glm::vec3 up(0.0f, 1.0f, 0.0f);

	return glm::lookAt(eye, center, up);
}

void VulkanApplication::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	auto* app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
	if (!app->m_camera.isDragging) return;

	// Delta movement
	const double dx = xpos - app->m_camera.lastX;
	const double dy = ypos - app->m_camera.lastY;
	app->m_camera.lastX = xpos;
	app->m_camera.lastY = ypos;

	// Sensitivity
	const float sensitivity = 0.005f;

	// Update Angles
	app->m_camera.theta -= dx * sensitivity;
	app->m_camera.phi   -= dy * sensitivity;

	// Clamp Phi (prevent flipping over the pole)
	const float epsilon = 0.1f;
	app->m_camera.phi = std::max(epsilon, std::min(glm::pi<float>() - epsilon, app->m_camera.phi));
}

void VulkanApplication::mouseButtonCallback(GLFWwindow* window, int button, int action, [[maybe_unused]] int mods)
{
	auto* app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));

	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
		    app->m_camera.isDragging = true;
		    glfwGetCursorPos(window, &app->m_camera.lastX, &app->m_camera.lastY);
		}
		else if (action == GLFW_RELEASE)
		{
		    app->m_camera.isDragging = false;
		}
	}
}

void VulkanApplication::scrollCallback(GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset)
{
	auto* app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));

	// Zoom in/out
	const float zoomSpeed = 0.2f;
	app->m_camera.radius -= yoffset * zoomSpeed;

	// Clamp radius
	if (app->m_camera.radius < 0.5f) app->m_camera.radius = 0.5f;
	if (app->m_camera.radius > 20.0f) app->m_camera.radius = 20.0f;
}

void VulkanApplication::keyCallback(GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
{
	auto* app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));

	// Toggle pause state on Spacebar press
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	{
		app->m_isPaused = !app->m_isPaused;
		LOG_DEBUG("Simulation Paused: " << (app->m_isPaused ? "TRUE" : "FALSE"));
	}
}

VulkanApplication::~VulkanApplication()
{
	// Wait for GPU to finish all work
	// This protects members from being destroyed while still in use
	vulkanDevice.device().waitIdle();
	LOG_DEBUG("VulkanApplication instance destroyed");
}

void VulkanApplication::fillMesh()
{
	// A simple unit cube (radius 0.5) with colors
	// Position (x,y,z,w) | Color (r,g,b,a)
	m_mesh.vertices = {
		// Front Face (Z = -0.5)
		Vertex(std::array<float, 8>{-0.5f, -0.5f, -0.5f, 1.0f,  0.0f, 0.0f, 0.0f, 1.0f}), // 0: black
		Vertex(std::array<float, 8>{ 0.5f, -0.5f, -0.5f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f}), // 1: red
		Vertex(std::array<float, 8>{ 0.5f,  0.5f, -0.5f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f}), // 2: yellow
		Vertex(std::array<float, 8>{-0.5f,  0.5f, -0.5f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f}), // 3: green
		// Back Face (Z = +0.5)
		Vertex(std::array<float, 8>{-0.5f, -0.5f,  0.5f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f}), // 4: blue
		Vertex(std::array<float, 8>{ 0.5f, -0.5f,  0.5f, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f}), // 5: magenta
		Vertex(std::array<float, 8>{ 0.5f,  0.5f,  0.5f, 1.0f,  1.0f, 1.0f, 1.0f, 1.0f}), // 6: white
		Vertex(std::array<float, 8>{-0.5f,  0.5f,  0.5f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f})  // 7: cyan
	};
	m_mesh.indices = {
		0, 1, 2, 2, 3, 0, // Front
		1, 5, 6, 6, 2, 1, // Right
		5, 4, 7, 7, 6, 5, // Back
		4, 0, 3, 3, 7, 4, // Left
		3, 2, 6, 6, 7, 3, // Top
		4, 5, 1, 1, 0, 4  // Bottom
	};
}

// Helper for Cosine Palette in C++
// A simple wrapper to keep the main loop clean
static inline std::array<float, 4> getCosineColor(double t, double offset)
{
	// Simple Rainbow: r, g, b phase shifted
	const float r = 0.5f + 0.5f * std::cos(t + offset);
	const float g = 0.5f + 0.5f * std::cos(t + offset + 2.0f);
	const float b = 0.5f + 0.5f * std::cos(t + offset + 4.0f);
	return { r, g, b, 1.0f };
}

void VulkanApplication::updateSatellites(double time)
{
	// --- SATELLITE CAMERAS PARAMETERS ---
	//const float fovY = glm::radians(15.0f);
	constexpr float tanHalfFov = 0.5; //std::tan(fovY / 2.0f);
	constexpr float aspect = 1.0f; // Square frustum for satellites
	constexpr float zNear = 0.1f;
	constexpr float zFar = 0.4f;   // Length of the cone

	const uint32_t count = satelliteNetwork.satellites.size();

	// --- GENERATE MATRICES ---
	for (uint32_t i = 0; i < count; ++i)
	{
		// 1. Position on sphere
		// (Simple placeholder distribution)
		const float theta = (float)i / count * glm::two_pi<float>();
		const float phi = glm::half_pi<float>() * 0.0f; // 45 deg latitude

		const float r = 1.5f; // Altitude
		const glm::vec3 pos(
			r * std::sin(theta) * std::cos(phi),
			r * std::cos(theta), // Y-up
			r * std::sin(theta) * std::sin(phi)
		);

		// 2. Look At Center (0,0,0)
		const glm::vec3 target(0.0f);
		const glm::vec3 up(std::cos(theta)*std::cos(phi), -std::sin(theta), std::cos(theta)*std::sin(phi));
		glm::mat4 view = glm::lookAt(pos, target, up);

		// 3. PACK PARAMS (Column-Major: view[col][row])
		// We overwrite Row 3 (the W components)
		view[0][3] = tanHalfFov;
		view[1][3] = aspect;
		view[2][3] = zNear;
		view[3][3] = zFar;

		satelliteNetwork.satellites[i].camera = view;

		// 4. Color
		// Give each index a different phase offset so they blink differently
		const auto offset = double(i) * 0.8;
		const auto col = getCosineColor(time, offset);

		// Copy to the 'data' field (which maps to 'color' in shader)
		std::memcpy(satelliteNetwork.satellites[i].data, col.data(), sizeof(col));
	}
}

int VulkanApplication::run()
{
	LOG_DEBUG("VulkanApplication started run()");

	// 1. Prepare Data
	fillMesh();
	m_mesh.upload(vulkanLoader);
	
	satelliteNetwork.satellites.resize(8);
	updateSatellites(0.0);
	
	// 2. Register Resources (Link Mesh + Satellites to Computer)
	computer.registerResources(m_mesh, satelliteNetwork);

	uint32_t currentFrame = 0;
	constexpr float dt = 0.1;
	glm::mat4 model = glm::mat4(1.0f);

	// Track real time to calculate delta
	double lastRealTime = vulkanWindow.getTime();

	// 3. Loop
	while (!vulkanWindow.shouldClose())
	{
		vulkanWindow.pollEvents();
		vulkanWindow.updateFPS(appName);

		const auto& uploadSem = m_uploadFinishedSemaphores[currentFrame];
		const auto& computeSem = m_computeFinishedSemaphores[currentFrame];
		const auto& fence = m_inFlightFences[currentFrame];

		// Wait for CPU to be ready
		if (vulkanDevice.device().waitForFences({*fence}, vk::True, UINT64_MAX) != vk::Result::eSuccess)
			{ throw std::runtime_error("Fence wait failed"); }

		// --- TIME LOGIC ---
		const double currentRealTime = vulkanWindow.getTime();
		const double realDt = currentRealTime - lastRealTime;
		lastRealTime = currentRealTime;

		if (!m_isPaused)
		{
			// Advance simulation time
			m_simTime += realDt;

			// --- Update Satellite Colors (Cosine) ---
			updateSatellites(m_simTime);

			// Upload new data to UBO
			satelliteNetwork.upload(currentFrame, vulkanLoader, *uploadSem);

			// --- Update Model matrix ---
			constexpr auto rotSpeed = 0.08f;
			const auto cos_time = std::cos(m_simTime * rotSpeed);
			const auto sin_time = std::sin(m_simTime * rotSpeed);
			
			model = glm::mat4 {
				cos_time,   0.0f,      sin_time, 0.0f,
				0.0f,       1.0f,      0.0f,     0.0f,
				-sin_time,  0.0f,      cos_time, 0.0f,
				0.0f,       0.0f,      0.0f,     1.0f };

			// --- Compute ---
			computer.compute(currentFrame, model, dt, {}, *uploadSem, *computeSem);

			// --- Render (Wait for Compute) ---
			renderer.draw(m_mesh, satelliteNetwork, currentFrame, *fence, *computeSem, model, getCameraView());
		}
		else
		{
			// --- Render ONLY ---
			// Notice the empty `{}` for waitSemaphore. The renderer instantly draws using 
			// the previous frame's SSBO and UBO data, which is completely thread-safe!
			renderer.draw(m_mesh, satelliteNetwork, currentFrame, *fence, {}, model, getCameraView());
		}
		
		// 3. Flow Guaranteed: Always advance
		currentFrame = advanceFrame(currentFrame);	
	}
	return EXIT_SUCCESS;
}
