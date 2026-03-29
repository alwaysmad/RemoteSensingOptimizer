// src/Application.cpp
#include <cstdlib>    // EXIT_SUCCESS
#include <cstring>
#include <filesystem> // std::filesystem::temp_directory_path

#include <array>
#include <chrono>
#include <cmath>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "Application.hpp"
#include "BufferStructs.hpp"
#include "EngineInstance.hpp"

// Define the static Throne
std::optional<Application> Application::s_instance;

Settings Application::configure(/*TODO parse cli args*/)
{
	// Create rulebook
	Settings s;

	// Adjust it according to god's will
		// Set a path for log file
		// 'give Scribe the parchment'
		// Cross-platform temporary directory
		// e.g., Linux: /tmp/SimpleVK.log
		// e.g., Windows: C:\Users\User\AppData\Local\Temp\SimpleVK.log
	s.logPath = std::filesystem::temp_directory_path() / ("rso.log");

	return s;
}

static inline std::array<float, 4> getCosineColor(double t, double offset)
{
	const float r = 0.5f + 0.5f * std::cos(t + offset);
	const float g = 0.5f + 0.5f * std::cos(t + offset + 2.0f);
	const float b = 0.5f + 0.5f * std::cos(t + offset + 4.0f);
	return {r, g, b, 1.0f};
}

static inline void updateAgents(std::vector<AgentData>& agents, double time)
{
	constexpr float tanHalfFov = 0.5f;
	constexpr float aspect = 1.0f;
	constexpr float zNear = 0.1f;
	constexpr float zFar = 0.4f;

	const uint32_t count = static_cast<uint32_t>(agents.size());

	for (uint32_t i = 0; i < count; ++i)
	{
		const float theta = static_cast<float>(i) / static_cast<float>(count) * glm::two_pi<float>();
		const float phi = glm::half_pi<float>() * 0.0f;

		const float r = 1.5f;
		const glm::vec3 pos(
			r * std::sin(theta) * std::cos(phi),
			r * std::cos(theta),
			r * std::sin(theta) * std::sin(phi));

		const glm::vec3 target(0.0f);
		const glm::vec3 up(
			std::cos(theta) * std::cos(phi),
			-std::sin(theta),
			std::cos(theta) * std::sin(phi));
		glm::mat4 view = glm::lookAt(pos, target, up);

		view[0][3] = tanHalfFov;
		view[1][3] = aspect;
		view[2][3] = zNear;
		view[3][3] = zFar;

		agents[i].camera = view;

		const auto offset = static_cast<double>(i) * 0.8;
		const auto col = getCosineColor(time, offset);
		std::memcpy(agents[i].data, col.data(), sizeof(col));
	}
}

static inline void updateModel(glm::mat4& model, double time)
{
	constexpr auto rotSpeed = 0.00;
	const auto cosTime = static_cast<float>(std::cos(time * rotSpeed));
	const auto sinTime = static_cast<float>(std::sin(time * rotSpeed));

	model = glm::mat4{
		cosTime, 0.0f, sinTime, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sinTime, 0.0f, cosTime, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f};
}

int Application::launch()
{
	// Declare start of the reign
	m_logger.cInfo("Application name is {}", Settings::appName);

	Mesh mesh;
	//mesh.populateMesh(3336); // 66773376
	mesh.populateMesh(300); // 540000

	std::vector<AgentData> agents;
	glm::mat4 model = glm::mat4(1.0f);
	double m_simTime = 0.0;

	agents.resize(8);

	EngineInstance engine(m_settings, m_logger, mesh, agents, model);
	auto lastTick = std::chrono::steady_clock::now();
	while (!engine.shouldClose())
	{
		const auto now = std::chrono::steady_clock::now();
		const double realDt = std::chrono::duration<double>(now - lastTick).count();
		lastTick = now;

		m_simTime += realDt;
		updateAgents(agents, m_simTime);
		updateModel(model, m_simTime);

		engine.tick(static_cast<float>(realDt));
	}

	return EXIT_SUCCESS;
}
