// src/Application.cpp
#include <cstdlib>    // EXIT_SUCCESS
#include <filesystem> // std::filesystem::temp_directory_path

#include "Application.hpp"
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

int Application::launch()
{
	// Declare start of the reign
	m_logger.cInfo("Application name is {}", Settings::appName);

	EngineInstance engine(m_settings, m_logger);
	while (!engine.shouldClose())
	{
		engine.tick();
	}

	return EXIT_SUCCESS;
}
