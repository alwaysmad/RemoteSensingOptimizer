// src/Application.cpp
#include <cstdlib> // EXIT_SUCCESS
#include <cstring>
#include <filesystem> // std::filesystem::temp_directory_path
#include <iostream>   // std::cerr for SGP4 error reporting

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include <glm/ext/matrix_transform.hpp>

#include <SGP4.h>

#include "Application.hpp"
#include "BufferStructs.hpp"
#include "EngineInstance.hpp"
#include "SYNTH_tle_data.hpp"

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
	// offset is double; std::cos returns double — cast explicitly at the boundary
	const float r = static_cast<float>(0.5 + 0.5 * std::cos(offset));
	const float g = static_cast<float>(0.5 + 0.5 * std::cos(offset + 2.0));
	const float b = static_cast<float>(0.5 + 0.5 * std::cos(offset + 4.0));
	return {r, g, b, 1.0f};
}

static inline glm::vec3 getSunVector(const libsgp4::DateTime& currentTime)
{
	// 1. Extract the Julian Date from your existing time object
	const double julian_date = currentTime.ToJulian();

	// Julian centuries since J2000.0
	const double T = (julian_date - 2451545.0) / 36525.0;

	// Mean longitude and anomaly of the Sun
	double L = 280.460 + 36000.770 * T;
	double M = 357.528 + 35999.050 * T;

	L = std::fmod(L, 360.0);
	if (L < 0) L += 360.0;
	M = std::fmod(M, 360.0);
	if (M < 0) M += 360.0;

	const double M_rad = M * std::numbers::pi / 180.0;

	// Ecliptic longitude & Obliquity
	const double lambda = (L + 1.915 * std::sin(M_rad) + 0.020 * std::sin(2.0 * M_rad)) * std::numbers::pi / 180.0;
	const double obliq_rad = (23.439 - 0.013 * T) * std::numbers::pi / 180.0;

	// Standard ECI Frame components (where Z is North)
	const auto eci_x = static_cast<float>(std::cos(lambda));
	const auto eci_y = static_cast<float>(std::sin(lambda) * std::cos(obliq_rad));
	const auto eci_z = static_cast<float>(std::sin(lambda) * std::sin(obliq_rad));

	// 2. swizzle axes (engine mapping: x, z, -y)
	return glm::vec3{eci_x, eci_z, -eci_y};
}

static inline float getSunElevationAngle(const glm::vec3& pos, const glm::vec3& sunDir)
{
	// The normal vector pointing up from the Earth's surface directly under the satellite
	const glm::vec3 earthNormal = glm::normalize(pos);

	// Dot product gives the cosine of the zenith angle
	float cosZenith = glm::dot(earthNormal, sunDir);
	cosZenith = glm::clamp(cosZenith, -1.0f, 1.0f); // Protect against floating-point drift

	// Elevation is the complement of the zenith angle (sin(elev) = cos(zenith))
	const float elevationRad = std::asin(cosZenith);
	const float elevationDeg = glm::degrees(elevationRad);

	// If it's on the dark side (below the horizon), return 0
	return (elevationDeg > 0.0f) ? elevationDeg : 0.0f;
}

static inline void updateAgents(std::vector<AgentData>& agents,
                                const std::vector<libsgp4::SGP4>& propagators,
                                const libsgp4::DateTime& currentTime)
{
	// Specifications derived from SuperDove sensor.
	// Settings::scaling is constexpr double — arithmetic stays double, cast at the boundary.
	constexpr float aspect = static_cast<float>(32.5 / 19.6);
	constexpr float tanHalfFov = static_cast<float>(19.6 / 2.0 / 525.0); // TODO: remove /525 multiplier after testing
	constexpr float zNear = static_cast<float>(400.0 * Settings::scaling);
	constexpr float zFar = static_cast<float>(525.0 * Settings::scaling);
	// 0.5 fps effective rate, 5 frames for 'full' survey pass
	constexpr float OmegaStrength =
	    static_cast<float>((1.0 / (5.0 / 0.5)) * (19.6 * Settings::scaling * 32.5 * Settings::scaling));

	const std::size_t count = std::min(agents.size(), propagators.size());

	// Compute the Sun vector once for this time step
	// const glm::vec3 sunDir = getSunVector(currentTime);

	for (std::size_t i = 0; i < count; ++i)
	{
		try
		{
			const libsgp4::Eci eci = propagators[i].FindPosition(currentTime);

			const libsgp4::Vector eciPosition = eci.Position();
			const libsgp4::Vector eciVelocity = eci.Velocity();

			// Scale and swizzle axes (engine mapping: x, z, -y)
			const glm::vec3 pos(static_cast<float>(eciPosition.x * Settings::scaling),
			                    static_cast<float>(eciPosition.z * Settings::scaling),
			                    static_cast<float>(-eciPosition.y * Settings::scaling));

			const glm::vec3 vel(static_cast<float>(eciVelocity.x * Settings::scaling),
			                    static_cast<float>(eciVelocity.z * Settings::scaling),
			                    static_cast<float>(-eciVelocity.y * Settings::scaling));

			// const float sunElevation = getSunElevationAngle(pos, sunDir);
			//  FLOCK Satellite only works is sun elevation > 10
			// const bool isOn = (sunElevation > 10.0f);
			const bool isOn = true; // For testing, keep it always on

			// ==========================================
			// Off-Nadir Pointing Calculation
			// ==========================================
			/*
			// 1. Define angle (in degrees))
			const float angleOffset = glm::radians(0.0f);

			// 2. Determine the original nadir forward vector (pointing exactly at 0,0,0)
			const glm::vec3 nadirForward = glm::normalize(-pos);

			// 3. Create a rotation matrix around the normalized velocity axis
			const glm::vec3 rotAxis = glm::normalize(vel);
			const glm::mat4 rotMatrix = glm::rotate(glm::mat4(1.0f), angleOffset, rotAxis);

			// 4. Rotate the forward vector
			const glm::vec3 offNadirForward = glm::vec3(rotMatrix * glm::vec4(nadirForward, 0.0f));

			// 5. Calculate the new target point in world space
			const glm::vec3 target = pos + offNadirForward;
			*/
			// 6. Generate the view matrix using the new target
			glm::mat4 view = glm::lookAt(pos, glm::vec3{0.0, 0.0, 0.0} /*target*/, vel);
			// ==========================================

			view[0][3] = tanHalfFov;
			view[1][3] = aspect;
			view[2][3] = zNear;
			view[3][3] = zFar;

			agents[i].camera = view;

			const auto col = getCosineColor(static_cast<double>(i));
			std::memcpy(agents[i].data, col.data(), sizeof(col));
			// no survey if no On
			agents[i].data[3] = isOn ? OmegaStrength : 0.0f;
		}
		catch (const std::exception& e)
		{
			std::cerr << "[WARNING] SGP4 propagation failed for satellite " << i << ": " << e.what()
			          << " — disabling this satellite for the current timestep.\n";
			agents[i].data[3] = 0.0f;
		}
	}
}

static inline void updateModel(glm::mat4& model, const libsgp4::DateTime& currentTime)
{
	const auto siderealAngle = currentTime.ToGreenwichSiderealTime();
	const auto cosTime = static_cast<float>(std::cos(siderealAngle));
	const auto sinTime = static_cast<float>(std::sin(siderealAngle));

	model = glm::mat4{
	    cosTime, 0.0f, -sinTime, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, sinTime, 0.0f, cosTime, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
}

int Application::launch()
{
	// Declare start of the reign
	m_logger.cInfo("Application name is {}", Settings::appName);

	Mesh mesh;
	mesh.populateMesh(m_settings.meshResolution);

	const auto& tle_data = SYNTH_tle_data::tle_data;

	std::vector<libsgp4::SGP4> propagators;
	propagators.reserve(tle_data.size());
	std::vector<AgentData> agents(tle_data.size());
	glm::mat4 model = glm::mat4(1.0f);
	float J_T = 0.0f;
	float J_av = 0.0f;
	// Ring buffer warmup: the result staging buffer uses MAX_FRAMES_IN_FLIGHT
	// slots. The first N reads return zero (no GPU results yet). We must run
	// N extra simulation steps to drain the pipeline, and exclude those
	// warmup reads from the J_av accumulation.
	constexpr uint32_t RING_WARMUP_STEPS = svk::MAX_FRAMES_IN_FLIGHT;
	uint32_t stepIndex = 0;

	constexpr double SIMULATION_TIME_STEP = 1.0;
	const libsgp4::DateTime startSimTime(2026, 9, 11, 10, 0, 0);
	const auto endSimTime =
	    libsgp4::DateTime(2026, 9, 18, 10, 0, 0).AddSeconds(SIMULATION_TIME_STEP * RING_WARMUP_STEPS);

	libsgp4::DateTime simTime = startSimTime;

	m_logger.cInfo("Active satellites: {}", tle_data.size());
	for (const auto& tle : tle_data)
	{
		m_logger.cInfo("{} : {}", propagators.size() + 1, tle.Name());
		propagators.emplace_back(tle);
	}
	agents.resize(propagators.size());
	// agents.resize(1);

	EngineInstance engine(m_settings, m_logger, J_T, J_av, mesh, agents, model);
	while (!engine.shouldClose() && simTime <= endSimTime)
	{
		if (engine.isPaused())
		{
			engine.tick(static_cast<float>(SIMULATION_TIME_STEP));
			continue;
		}
		updateAgents(agents, propagators, simTime);
		updateModel(model, simTime);
		engine.tick(static_cast<float>(SIMULATION_TIME_STEP));
		++stepIndex;
		const auto reportedSimTime = (simTime - startSimTime).TotalSeconds() - SIMULATION_TIME_STEP * RING_WARMUP_STEPS;
		if constexpr (!Settings::headlessMode)
		{
			if (stepIndex > RING_WARMUP_STEPS) { m_logger.cInfo("{} | J_T: {:.6f}", reportedSimTime, J_T); }
		}
		simTime = simTime.AddSeconds(SIMULATION_TIME_STEP);
	}

	m_logger.cInfo("Simulation completed");
	// Effective simulation duration excludes warmup steps
	const auto elapsedTime = (simTime - startSimTime).TotalSeconds() - SIMULATION_TIME_STEP * RING_WARMUP_STEPS;
	if constexpr (Settings::headlessMode) { m_logger.cInfo("{} | J_T: {:.6f}", elapsedTime, J_T); }
	if (elapsedTime > 0.0) { m_logger.cInfo("Final J_av: {:.6f}", J_av / elapsedTime); }

	// Once simulation is done pause to show the final state
	engine.setPause(true);
	while (!engine.shouldClose() && engine.isPaused()) { engine.tick(static_cast<float>(SIMULATION_TIME_STEP)); }

	return EXIT_SUCCESS;
}
