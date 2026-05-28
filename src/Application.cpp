// src/Application.cpp
#include <cstdlib>    // EXIT_SUCCESS
#include <cstring>
#include <filesystem> // std::filesystem::temp_directory_path

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <sstream>
#include <string>

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

static inline std::array<float, 4> getCosineColor(double offset)
{
	const float r = 0.5f + 0.5f * std::cos(offset);
	const float g = 0.5f + 0.5f * std::cos(offset + 2.0f);
	const float b = 0.5f + 0.5f * std::cos(offset + 4.0f);
	return {r, g, b, 1.0f};
}

static inline glm::vec3 getSunVector(const libsgp4::DateTime& currentTime)
{
	const double julianDate = currentTime.ToJulian();
	const double julianCenturies = (julianDate - 2451545.0) / 36525.0;

	double meanLongitude = 280.460 + 36000.770 * julianCenturies;
	double meanAnomaly = 357.528 + 35999.050 * julianCenturies;

	meanLongitude = std::fmod(meanLongitude, 360.0);
	if (meanLongitude < 0.0)
		{ meanLongitude += 360.0; }
	meanAnomaly = std::fmod(meanAnomaly, 360.0);
	if (meanAnomaly < 0.0)
		{ meanAnomaly += 360.0; }

	const double meanAnomalyRad = meanAnomaly * std::numbers::pi / 180.0;
	const double eclipticLongitude = (meanLongitude + 1.915 * std::sin(meanAnomalyRad) + 0.020 * std::sin(2.0 * meanAnomalyRad)) * std::numbers::pi / 180.0;
	const double obliquityRad = (23.439 - 0.013 * julianCenturies) * std::numbers::pi / 180.0;

	const auto eciX = static_cast<float>(std::cos(eclipticLongitude));
	const auto eciY = static_cast<float>(std::sin(eclipticLongitude) * std::cos(obliquityRad));
	const auto eciZ = static_cast<float>(std::sin(eclipticLongitude) * std::sin(obliquityRad));

	return glm::vec3{ eciX, eciZ, -eciY };
}

static inline float getSunElevationAngle(const glm::vec3& pos, const glm::vec3& sunDir)
{
	const glm::vec3 earthNormal = glm::normalize(pos);
	float cosZenith = glm::dot(earthNormal, sunDir);
	cosZenith = glm::clamp(cosZenith, -1.0f, 1.0f);

	const float elevationRad = std::asin(cosZenith);
	const float elevationDeg = glm::degrees(elevationRad);
	return (elevationDeg > 0.0f) ? elevationDeg : 0.0f;
}

static inline void updateAgents(std::vector<AgentData>& agents, const std::vector<libsgp4::SGP4>& propagators, const libsgp4::DateTime& currentTime)
{
	constexpr float aspect = 32.5 / 19.6;
	constexpr float tanHalfFov = 19.6 / 2.0 / 525.0;
	constexpr float zNear = 400.0 * Settings::scaling;
	constexpr float zFar = 525.0 * Settings::scaling;
	constexpr float OmegaStrength = (1.0 / (5.0 / 0.5)) * (19.6 * Settings::scaling * 32.5 * Settings::scaling);

	const std::size_t count = std::min(agents.size(), propagators.size());
	const glm::vec3 sunDir = getSunVector(currentTime);

	for (std::size_t i = 0; i < count; ++i)
	{
		const libsgp4::Eci eci = propagators[i].FindPosition(currentTime);

		const libsgp4::Vector eciPosition = eci.Position();
		const libsgp4::Vector eciVelocity = eci.Velocity();

		const glm::vec3 pos(
			static_cast<float>(eciPosition.x * Settings::scaling),
			static_cast<float>(eciPosition.z * Settings::scaling),
			static_cast<float>(-eciPosition.y * Settings::scaling));

		const glm::vec3 vel(
			static_cast<float>(eciVelocity.x * Settings::scaling),
			static_cast<float>(eciVelocity.z * Settings::scaling),
			static_cast<float>(-eciVelocity.y * Settings::scaling));

		const float sunElevation = getSunElevationAngle(pos, sunDir);
		const bool isOn = (sunElevation > 10.0f);

		glm::mat4 view = glm::lookAt(pos, glm::vec3{0.0, 0.0, 0.0}, vel);
		view[0][3] = tanHalfFov;
		view[1][3] = aspect;
		view[2][3] = zNear;
		view[3][3] = zFar;

		agents[i].camera = view;

		const auto col = getCosineColor(static_cast<double>(i));
		std::memcpy(agents[i].data, col.data(), sizeof(col));
		agents[i].data[3] = isOn ? OmegaStrength : 0.0f;
	}
}

static inline void updateModel(glm::mat4& model, const libsgp4::DateTime& currentTime)
{
	const auto siderealAngle = currentTime.ToGreenwichSiderealTime();
	const auto cosTime = static_cast<float>(std::cos(siderealAngle));
	const auto sinTime = static_cast<float>(std::sin(siderealAngle));

	model = glm::mat4{
		cosTime, 0.0f, -sinTime, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		sinTime, 0.0f, cosTime, 0.0f,
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
	std::vector<std::string> activeNames; activeNames.reserve(FLOCK_tle_data::tle_data.size());

	constexpr double SIMULATION_TIME_STEP = 1.0;
	const libsgp4::DateTime startSimTime(2026, 6, 20, 10, 0, 0);
	const libsgp4::DateTime endSimTime = libsgp4::DateTime(2026, 6, 20, 13, 0, 0).AddSeconds(SIMULATION_TIME_STEP * 2.0);
	const glm::mat4 initialModel = glm::mat4(1.0f);

	m_logger.cInfo("Active satellites: {}", FLOCK_tle_data::tle_data.size());
	for (const auto& tle : FLOCK_tle_data::tle_data)
	{
		m_logger.cInfo("{} : {}", propagators.size() + 1, tle.Name());
		propagators.emplace_back(tle);
		activeNames.emplace_back(std::string(tle.Name()));
	}
	agents.resize(propagators.size());

	const auto runSimulation = [&, startSimTime, endSimTime, initialModel](
		std::vector<libsgp4::SGP4>& simulationPropagators,
		std::vector<AgentData>& simulationAgents,
		const std::vector<std::string>& simulationActiveNames) -> float
	{
		float simulationJ_T = 0.0f;
		float simulationJ_av = 0.0f;
		glm::mat4 simulationModel = initialModel;
		libsgp4::DateTime simTime = startSimTime;

		{
			[[maybe_unused]] const auto activeNameCount = simulationActiveNames.size();
			EngineInstance engine(m_settings, m_logger, simulationJ_T, simulationJ_av, mesh, simulationAgents, simulationModel);
			while (!engine.shouldClose() && simTime <= endSimTime)
			{
				updateAgents(simulationAgents, simulationPropagators, simTime);
				updateModel(simulationModel, simTime);
				engine.tick(static_cast<float>(SIMULATION_TIME_STEP));
				simTime = simTime.AddSeconds(SIMULATION_TIME_STEP);
			}
		}
		return simulationJ_T;
	};

	const float baselineJ_T = runSimulation(propagators, agents, activeNames);
	{
		std::ostringstream baselineList;
		for (std::size_t index = 0; index < activeNames.size(); ++index)
		{
			if (index != 0)
				{ baselineList << ", "; }
			baselineList << activeNames[index];
		}

		m_logger.cInfo(
			"Baseline -> active satellites: {}, J_T: {:.6f}",
			agents.size(),
			baselineJ_T);
	}

	while (propagators.size() > 110)
	{
		float bestJ_T = std::numeric_limits<float>::infinity();
		std::size_t bestIndex = 0;
		std::string removedName;

		for (std::size_t candidateIndex = 0; candidateIndex < propagators.size(); ++candidateIndex)
		{
			auto candidatePropagators = propagators;
			auto candidateAgents = agents;
			auto candidateNames = activeNames;

			candidatePropagators.erase(candidatePropagators.begin() + static_cast<std::ptrdiff_t>(candidateIndex));
			candidateAgents.erase(candidateAgents.begin() + static_cast<std::ptrdiff_t>(candidateIndex));
			removedName = candidateNames[candidateIndex];
			candidateNames.erase(candidateNames.begin() + static_cast<std::ptrdiff_t>(candidateIndex));

			const float candidateJ_T = runSimulation(candidatePropagators, candidateAgents, candidateNames);
			if (candidateJ_T < bestJ_T)
			{
				bestJ_T = candidateJ_T;
				bestIndex = candidateIndex;
				removedName = activeNames[candidateIndex];
			}
		}

		propagators.erase(propagators.begin() + static_cast<std::ptrdiff_t>(bestIndex));
		agents.erase(agents.begin() + static_cast<std::ptrdiff_t>(bestIndex));
		activeNames.erase(activeNames.begin() + static_cast<std::ptrdiff_t>(bestIndex));

		m_logger.cInfo(
			"Backward greedy step -> active satellites: {}, J_T: {:.6f}, removed: {}",
			agents.size(),
			bestJ_T,
			removedName);
	}

	m_logger.cInfo(
		"Optimization completed -> active satellites: {}",
		agents.size());

	if (!activeNames.empty())
	{
		std::ostringstream activeCycle;
		for (std::size_t index = 0; index < activeNames.size(); ++index)
		{
			if (index != 0)
				{ activeCycle << " -> "; }
			activeCycle << activeNames[index];
		}
		activeCycle << " -> " << activeNames.front();

		m_logger.cInfo("Final constellation cycle: {}", activeCycle.str());
	}
	
	return EXIT_SUCCESS;
}