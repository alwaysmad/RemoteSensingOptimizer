// src/Application.hpp
#pragma once

#include <optional>
#include <utility>

#include "Settings.hpp"
#include "engine/Logger.hpp"

class Application
{
	// 'Emperor' of the application
	// Is a singleton
private:
	// Passkey (THE CROWN)
	// A permission slip that only 'Application' can sign.
	// Only run() can crown an emperor
	class PassKey {
		friend class Application; // Only App can create this
		private: explicit PassKey() = default;
	};

	// The privy council
	static std::optional<Application> s_instance; // The throne
	Settings m_settings; // Rulebook
	Logger m_logger; // Herald and Scribe

	// The Imperial court
	// TODO more

	// Helpers
	static Settings configure(/*TODO parse cli args*/);
	int launch(); // 'the reign of the Emperor'

public:
	// Delete copy/move to enforce singleton
	Application(const Application&) = delete;
	Application(Application&&) = delete;
	Application& operator= (const Application&) = delete;
	Application& operator= (Application&&) = delete;

	// start the application
	// 'crown emperor and start his reign'
	static int run(/*TODO parse cli args*/)
	{
		// Crown a new emperor with a (empty) crown and a new rulebook
		s_instance.emplace(PassKey(), configure(/*TODO args */));

		// emperor reigns
		int result = s_instance->launch();

		// end the reign gracefully
		s_instance.reset();

		// Report back
		return result;
	}

	// Public so std::optional can create and destroy
	// Since only 'run()' can make the Key, this is effectively private.
	explicit Application(PassKey, Settings&& s) : m_settings(std::move(s)), m_logger(m_settings.logPath)
		{ m_logger.cInfo("Application started"); }
	~Application() { m_logger.cInfo("Application ended"); }
};
