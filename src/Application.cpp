// src/Application.cpp
#include <cstdlib>    // EXIT_SUCCESS
#include <cstring>
#include <filesystem> // std::filesystem::temp_directory_path

#include <array>
#include <chrono>
#include <cmath>
#include <algorithm>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <SGP4.h>

#include "FLOCK_tle_data.hpp"
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

static inline void updateAgents(std::vector<AgentData>& agents, const std::vector<libsgp4::SGP4>& propagators, const libsgp4::DateTime& currentTime)
{
    constexpr float aspect = 32.5 / 19.6;
    constexpr float tanHalfFov = 19.6 / 2.0 / 525.0;
    constexpr float zNear = 400.0 * Settings::scaling;
    constexpr float zFar = 525.0 * Settings::scaling;

    const std::size_t count = std::min(agents.size(), propagators.size());

    for (std::size_t i = 0; i < count; ++i)
    {
        const libsgp4::Eci eci = propagators[i].FindPosition(currentTime);

        const libsgp4::Vector eciPosition = eci.Position();
        const libsgp4::Vector eciVelocity = eci.Velocity();

        // Scale and swizzle axes (engine mapping: x, z, -y)
        const glm::vec3 pos(
            static_cast<float>(eciPosition.x * Settings::scaling),
            static_cast<float>(eciPosition.z * Settings::scaling),
            static_cast<float>(-eciPosition.y * Settings::scaling));

        const glm::vec3 vel(
            static_cast<float>(eciVelocity.x * Settings::scaling),
            static_cast<float>(eciVelocity.z * Settings::scaling),
            static_cast<float>(-eciVelocity.y * Settings::scaling));

        const glm::vec3 target(0.0f);
        glm::mat4 view = glm::lookAt(pos, target, vel);

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

static inline void updateModel(glm::mat4& model, const libsgp4::DateTime& currentTime)
{
    const auto siderealAngle = currentTime.ToGreenwichSiderealTime();
    const auto cosTime = static_cast<float>(std::cos(siderealAngle));
    const auto sinTime = static_cast<float>(std::sin(siderealAngle));

	model = glm::mat4{
		cosTime, 0.0f, sinTime, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sinTime, 0.0f, cosTime, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };
}

int Application::launch()
{
	// Declare start of the reign
	m_logger.cInfo("Application name is {}", Settings::appName);

	Mesh mesh;
    mesh.populateMesh(m_settings.meshResolution);

    std::vector<libsgp4::SGP4> propagators; propagators.reserve(FLOCK_tle_data::tle_data.size());
    std::vector<AgentData> agents(FLOCK_tle_data::tle_data.size());
	glm::mat4 model = glm::mat4(1.0f);
	float J_T = 0.0f;
	float J_av = 0.0f;

	constexpr double SIMULATION_TIME_STEP = 5.0;
    const libsgp4::DateTime startSimTime(2026, 5, 20, 12, 0, 0);
    libsgp4::DateTime m_simTime = startSimTime;

    m_logger.cInfo("Active satellites: {}", FLOCK_tle_data::tle_data.size());
    for (const auto& tle : FLOCK_tle_data::tle_data)
    {
        m_logger.cInfo("{} : {}", propagators.size() + 1, tle.Name());
        propagators.emplace_back(tle);
    }
    agents.resize(propagators.size());
    //agents.resize(5);

	EngineInstance engine(m_settings, m_logger, J_T, J_av, mesh, agents, model);
	while (!engine.shouldClose()/*&& m_simTime < SIM_END_TIME*/)
	{
        if (engine.isPaused())
        {
            engine.tick(static_cast<float>(SIMULATION_TIME_STEP));
            continue;
        }

        updateAgents(agents, propagators, m_simTime);
		updateModel(model, m_simTime);
		engine.tick(static_cast<float>(SIMULATION_TIME_STEP));
        const auto reportedSimTime = (m_simTime - startSimTime).TotalSeconds() - SIMULATION_TIME_STEP;
        if (reportedSimTime >= 0.0)
            { m_logger.cInfo("{} | J_T: {}", reportedSimTime, J_T); }
        m_simTime = m_simTime.AddSeconds(SIMULATION_TIME_STEP);
	}

    const auto elapsedTime = static_cast<float>((m_simTime - startSimTime).TotalSeconds() - SIMULATION_TIME_STEP);
    if (elapsedTime > 0.0)
        { m_logger.cInfo("Final J_av: {}", J_av / elapsedTime); }

	return EXIT_SUCCESS;
}
