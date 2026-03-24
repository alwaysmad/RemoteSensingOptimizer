// src/EngineInstance.hpp
#pragma once

#include <cassert>
#include <utility>

#include "Settings.hpp"
#include "engine/Instance.hpp"
#include "engine/Logger.hpp"
#include "engine/window/WindowContext.hpp"

class EngineInstance
{
private:
	Settings& m_settings;
	Logger& m_logger;
	svk::WindowContext m_windowContext;
	svk::Instance m_instance;

public:
	EngineInstance(const EngineInstance&) = delete;
	EngineInstance& operator=(const EngineInstance&) = delete;
	EngineInstance(EngineInstance&&) = delete;
	EngineInstance& operator=(EngineInstance&& other) = delete;

	EngineInstance(Settings& settings, Logger& logger)
		: m_settings(settings),
		  m_logger(logger),
		  m_windowContext(),
		  m_instance(std::string(Settings::appName), m_windowContext.getRequiredInstanceExtensions(), m_logger)
	{}

	int run();
};
