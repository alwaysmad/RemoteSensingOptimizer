// src/EngineInstance.hpp
#pragma once

#include <cassert> // assert for invariant checks in move operations
#include <optional>
#include <vector>
#include <utility> // std::move for move assignment

#include "BufferStructs.hpp"
#include "Settings.hpp"
#include "engine/Frames.hpp"
#include "engine/Buffer.hpp"
#include "engine/Device.hpp"
#include "engine/Instance.hpp"
#include "engine/Logger.hpp"
#include "engine/ComputeRoutine.hpp"
#include "engine/RenderRoutine.hpp"
#include "engine/Swapchain.hpp"
#include "engine/TransferRoutine.hpp"
#include "engine/window/Camera.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowContext.hpp"

class EngineInstance
{
private:
	const Settings& m_settings;
	const svk::Logger& m_logger;
	const Mesh& m_mesh;
	const std::vector<AgentData>& m_agents;
	const glm::mat4& m_model;
	svk::WindowContext m_windowContext;
	svk::Instance m_instance;
	svk::Window m_window;
	svk::Camera m_camera;
	svk::Device m_device;
	svk::Swapchain m_swapchain;
	svk::RenderRoutine m_renderRoutine;
	svk::TransferRoutine m_transferRoutine;
	std::optional<svk::ComputeRoutine> m_computeRoutine;
	std::optional<svk::ComputeRoutine> m_reduceRoutine;

	std::optional<svk::Buffer> m_vertexBuffer;
	std::optional<svk::Buffer> m_vertexDataBuffer;
	std::optional<svk::Buffer> m_resultDeviceBuffer;
	std::optional<svk::Buffer> m_resultStagingBuffer;
	std::optional<svk::BufferMap> m_resultMap;

	// UBO data and buffers
	UBO m_ubo;
	std::optional<svk::Buffer> m_uboDeviceBuffer;
	std::optional<svk::Buffer> m_uboStagingBuffer;
	std::optional<svk::BufferMap> m_uboStagingMap;
	std::vector<void*> m_uboStagingPtrs;
	vk::DeviceSize m_uboFrameSize = 0;

	// sync
	std::vector<vk::raii::Semaphore> m_uboUpdatedSemaphores;
	std::vector<vk::raii::Semaphore> m_computeFinishedSemaphores;
	std::vector<vk::raii::Semaphore> m_reduceFinishedSemaphores;
	std::vector<vk::raii::Semaphore> m_resultTransferredSemaphores;
	std::vector<vk::raii::Fence> m_inFlightFences;
	uint32_t m_currentFrame = 0;

	static constexpr uint32_t kResultReadbackTransferCmd = svk::MAX_FRAMES_IN_FLIGHT;

public:
	EngineInstance(const EngineInstance&) = delete;
	EngineInstance& operator=(const EngineInstance&) = delete;
	EngineInstance(EngineInstance&&) = delete;
	EngineInstance& operator=(EngineInstance&& other) = delete;

	EngineInstance(
		const Settings& settings,
		const svk::Logger& logger,
		const Mesh& mesh,
		const std::vector<AgentData>& agents,
		const glm::mat4& model);
	~EngineInstance();

	void tick(float timeStep);
	[[nodiscard]] bool shouldClose() const;

private:
	void updateUBO(uint32_t currentFrame, float timeStep);
};
