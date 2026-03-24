// src/EngineInstance.hpp
#pragma once

#include <cassert> // assert for invariant checks in move operations
#include <utility> // std::move for move assignment

#include "Settings.hpp"
#include "engine/Instance.hpp"
#include "engine/Logger.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowContext.hpp"

class EngineInstance
{
private:
	Settings& m_settings;
	Logger& m_logger;
	svk::WindowContext m_windowContext;
	svk::Instance m_instance;
	svk::Window m_window;

public:
	EngineInstance(const EngineInstance&) = delete;
	EngineInstance& operator=(const EngineInstance&) = delete;
	EngineInstance(EngineInstance&&) = delete;
	EngineInstance& operator=(EngineInstance&& other) = delete;

	EngineInstance(Settings& settings, Logger& logger)
		: m_settings(settings),
		  m_logger(logger),
		  m_windowContext(),
		  m_instance(std::string(Settings::appName), m_windowContext.getRequiredInstanceExtensions(), m_logger),
		  m_window(m_windowContext, m_instance.getInstance(), m_settings.windowWidth, m_settings.windowHeight, std::string(Settings::appName))
	{}

	void tick();
	[[nodiscard]] bool shouldClose() const;
};
