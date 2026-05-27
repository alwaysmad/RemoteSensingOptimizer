// src/Settings.hpp
#pragma once

#include <cstdint>     // uint32_t for window dimensions
#include <string>      // std::string for paths and device names
#include <string_view> // std::string_view for static app name

struct Settings
{
public:
	// Emperor's rulebook
	static constexpr std::string appName = "RSO";
	// Build-time mode switch for the dual-binary setup.
#ifdef HEADLESS
    static constexpr bool headlessMode = true;
#else
    static constexpr bool headlessMode = false;
#endif
	
	std::string logPath = "rso.log";
	std::string deviceName = "Intel(R) Iris(R) Xe Graphics (ADL GT2)";
	uint32_t windowWidth = 800;
	uint32_t windowHeight = 600;

	uint32_t meshResolution = 400;
	// const double kXKMPER = 6378.135; // libsgp4::kXKMPER Earth radius in km
	static constexpr double scaling = 1.0 / 6378.135; 

	Settings() noexcept = default;
	~Settings() noexcept = default;

	// Both copy and move are allowed
	Settings(const Settings&) noexcept = default;
	Settings& operator= (const Settings&) noexcept = default;
	Settings(Settings&&) noexcept = default;
	Settings& operator= (Settings&&) noexcept = default;
};
