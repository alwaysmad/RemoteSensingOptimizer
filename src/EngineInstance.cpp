// src/EngineInstance.cpp

#include <string> // std::string

#include "EngineInstance.hpp"

void EngineInstance::tick()
{
	m_window.pollEvents();
	m_window.updateFPS(std::string(Settings::appName));
}

bool EngineInstance::shouldClose() const { return m_window.shouldClose(); }
