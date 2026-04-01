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
    constexpr float tanHalfFov = 0.2f;
    constexpr float aspect = 1.0f;
    constexpr float zNear = 0.1f;
    constexpr float zFar = 0.3f;

    // Realistic angular velocities (radians per second)
    constexpr float earthRotSpeed = glm::two_pi<float>() / 86400.0f; // 24 hours (GEO)
    constexpr float leoRotSpeed   = glm::two_pi<float>() / 5400.0f;  // 90 minutes (LEO)

    const uint32_t count = static_cast<uint32_t>(agents.size());

    for (uint32_t i = 0; i < count; ++i)
    {
        float speed = 0.0f;
        float inc = 0.0f;
        float raan = 0.0f;
        float offsetAngle = 0.0f;
        float r = 1.4f;

        if (i == 0)
        {
            // Geostationary Anchor
            r = 1.8f;
            speed = earthRotSpeed; // Matches the model matrix rotation exactly
            inc = 0.0f;            // 0 inclination = orbits perfectly over the equator
            raan = 0.0f;
            offsetAngle = 0.0f;    // The longitude it hovers over
        }
        else
        {
            // LEO Swarm (Walker-Delta Constellation)
            const uint32_t swarmIndex = i - 1;
            const uint32_t swarmCount = count - 1;
            
            const uint32_t numPlanes = 6;
            const uint32_t planeIndex = swarmIndex % numPlanes;
            const uint32_t satInPlane = swarmIndex / numPlanes;
            const uint32_t satsPerPlane = std::max(1u, (swarmCount + numPlanes - 1) / numPlanes);

            r = 1.3f;
            speed = leoRotSpeed; // Orbits much faster than the Earth spins
            inc = glm::radians(55.0f); 
            
            raan = static_cast<float>(planeIndex) * (glm::two_pi<float>() / static_cast<float>(numPlanes));
            offsetAngle = static_cast<float>(satInPlane) * (glm::two_pi<float>() / static_cast<float>(satsPerPlane));
            offsetAngle += static_cast<float>(planeIndex) * 0.3f;
        }

        // 1. Calculate position in flat equatorial plane
        const float theta = static_cast<float>(time) * speed + offsetAngle;
        glm::vec3 pos(r * std::cos(theta), 0.0f, r * std::sin(theta));

        // 2. Apply Inclination
        const float y_inc = pos.z * std::sin(inc);
        const float z_inc = pos.z * std::cos(inc);
        pos = glm::vec3(pos.x, y_inc, z_inc);

        // 3. Apply RAAN
        const float x_final = pos.x * std::cos(raan) + pos.z * std::sin(raan);
        const float z_final = -pos.x * std::sin(raan) + pos.z * std::cos(raan);
        pos = glm::vec3(x_final, pos.y, z_final);

        // 4. Build Camera View Matrix
        const glm::vec3 target(0.0f);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::mat4 view = glm::lookAt(pos, target, up);

        view[0][3] = tanHalfFov;
        view[1][3] = aspect;
        view[2][3] = zNear;
        view[3][3] = zFar;

        agents[i].camera = view;

        const auto colorOffset = static_cast<double>(i) * 0.8;
        const auto col = getCosineColor(0, colorOffset);
        std::memcpy(agents[i].data, col.data(), sizeof(col));
    }
}

static inline void updateModel(glm::mat4& model, double time)
{
	constexpr auto rotSpeed = glm::two_pi<float>() / (24*60*60); // one full rotation per 24 hours
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
    mesh.populateMesh(m_settings.meshResolution);

	std::vector<AgentData> agents;
	glm::mat4 model = glm::mat4(1.0f);
	float J_T = 0.0f;
	float J_av = 0.0f;

	constexpr double SIM_START_TIME = 0.0;
	constexpr double SIM_END_TIME = 24.0 * 60.0 * 60.0;
	constexpr float SIMULATION_TIME_STEP = 5.0f;

	double m_simTime = SIM_START_TIME;

	agents.resize(1);

	EngineInstance engine(m_settings, m_logger, J_T, J_av, mesh, agents, model);
	while (!engine.shouldClose() /*&& m_simTime < SIM_END_TIME*/)
	{
        if (engine.isPaused())
        {
            engine.tick(SIMULATION_TIME_STEP);
            continue;
        }

		updateAgents(agents, m_simTime);
		updateModel(model, m_simTime);
		engine.tick(SIMULATION_TIME_STEP);
		m_simTime += static_cast<double>(SIMULATION_TIME_STEP);
	}

	const double elapsedTime = m_simTime - SIM_START_TIME;
	const float J_av_mean = (elapsedTime > 0.0) ? static_cast<float>(J_av / elapsedTime) : 0.0f;
	m_logger.cInfo("Final J_T: {}", J_T);
	m_logger.cInfo("Final J_av: {}", J_av_mean);

	return EXIT_SUCCESS;
}
