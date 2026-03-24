// src/EngineInstance.hpp
#pragma once

#include <cassert> // assert for invariant checks in move operations
#include <utility> // std::move for move assignment

#include "Settings.hpp"
#include "engine/Device.hpp"
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
	svk::Device m_device;

public:
	EngineInstance(const EngineInstance&) = delete;
	EngineInstance& operator=(const EngineInstance&) = delete;
	EngineInstance(EngineInstance&&) = delete;
	EngineInstance& operator=(EngineInstance&& other) = delete;

	EngineInstance(Settings& settings, Logger& logger);

	void tick();
	[[nodiscard]] bool shouldClose() const;
};
