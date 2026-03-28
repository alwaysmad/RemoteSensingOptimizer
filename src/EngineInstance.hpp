// src/EngineInstance.hpp
#pragma once

#include <cassert> // assert for invariant checks in move operations
#include <optional>
#include <vector>
#include <utility> // std::move for move assignment

#include "BufferStructs.hpp"
#include "Settings.hpp"
#include "engine/Buffer.hpp"
#include "engine/Device.hpp"
#include "engine/Instance.hpp"
#include "engine/Logger.hpp"
#include "engine/RenderRoutine.hpp"
#include "engine/Swapchain.hpp"
#include "engine/TransferRoutine.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowContext.hpp"

class EngineInstance
{
private:
	Settings& m_settings;
	svk::Logger& m_logger;
	const Mesh& m_mesh;
	svk::WindowContext m_windowContext;
	svk::Instance m_instance;
	svk::Window m_window;
	svk::Device m_device;
	svk::Swapchain m_swapchain;
	svk::RenderRoutine m_renderRoutine;
	svk::TransferRoutine m_transferRoutine;

	std::optional<svk::Buffer> m_vertexBuffer;
	std::optional<svk::Buffer> m_indexBuffer;

	std::vector<vk::raii::Fence> m_inFlightFences;
	uint32_t m_currentFrame = 0;

public:
	EngineInstance(const EngineInstance&) = delete;
	EngineInstance& operator=(const EngineInstance&) = delete;
	EngineInstance(EngineInstance&&) = delete;
	EngineInstance& operator=(EngineInstance&& other) = delete;

	EngineInstance(Settings& settings, svk::Logger& logger, const Mesh& mesh);
	~EngineInstance();

	void tick();
	[[nodiscard]] bool shouldClose() const;
};
