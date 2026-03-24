// src/Settings.hpp
#pragma once

#include <cstdint>     // uint32_t for window dimensions
#include <string>      // std::string for paths and device names
#include <string_view> // std::string_view for static app name

#ifdef NDEBUG
inline constexpr bool enableValidationLayers = false;
#else
inline constexpr bool enableValidationLayers = true;
#endif

struct Settings
{
public:
	// Emperor's rulebook
	static constexpr std::string_view appName = "RSO";
	std::string logPath = "rso.log";
	std::string deviceName = "Intel(R) Iris(R) Xe Graphics (ADL GT2)";
	uint32_t windowWidth = 1280;
	uint32_t windowHeight = 720;

	Settings() noexcept = default;
	~Settings() noexcept = default;

	// Both copy and move are allowed
	Settings(const Settings&) noexcept = default;
	Settings& operator= (const Settings&) noexcept = default;
	Settings(Settings&&) noexcept = default;
	Settings& operator= (Settings&&) noexcept = default;
};
