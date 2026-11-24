// src/EngineInstance.hpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "BufferStructs.hpp"
#include "engine/Buffer.hpp"
#include "engine/ComputeRoutine.hpp"
#include "engine/ConsoleLogger.hpp"
#include "engine/Device.hpp"
#include "engine/Frames.hpp"
#include "engine/Instance.hpp"
#include "engine/RenderRoutine.hpp"
#include "engine/Swapchain.hpp"
#include "engine/TransferRoutine.hpp"
#include "engine/window/Camera.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowContext.hpp"
#include "Settings.hpp"

class EngineInstance
{
private:
	const Settings& m_settings;
	const svk::ConsoleLogger& m_logger;
	double& m_J_T;
	double& m_J_av;
	const Mesh& m_mesh;
	const std::vector<AgentData>& m_agents;
	const glm::mat4& m_model;
	std::optional<svk::WindowContext> m_windowContext;
	std::optional<svk::Instance> m_instance;
	std::optional<svk::Window> m_window;
	std::optional<svk::Camera> m_camera;
	std::optional<svk::Device> m_device;
	std::optional<svk::Swapchain> m_swapchain;
	std::optional<svk::RenderRoutine> m_renderRoutine;
	std::optional<svk::TransferRoutine> m_transferRoutine;
	std::optional<svk::ComputeRoutine> m_computeRoutine;
	std::optional<svk::ComputeRoutine> m_reducePartialsRoutine;
	std::optional<svk::ComputeRoutine> m_reduceFinalRoutine;

	std::optional<svk::Buffer> m_vertexBuffer;
	std::optional<svk::Buffer> m_vertexDataBuffer;
	std::optional<svk::Buffer> m_partialsDeviceBuffer;
	std::optional<svk::Buffer> m_resultDeviceBuffer;
	std::optional<svk::Buffer> m_resultStagingBuffer;
	std::optional<svk::BufferMap> m_resultMap;
	vk::DeviceSize m_resultFrameStride = 0;

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
	std::vector<vk::raii::Semaphore> m_reducePartialsFinishedSemaphores;
	std::vector<vk::raii::Semaphore> m_reduceFinishedSemaphores;
	std::vector<vk::raii::Semaphore> m_resultTransferredSemaphores;
	std::vector<vk::raii::Fence> m_inFlightFences;
	uint32_t m_currentFrame = 0;

	// Per-slot in-flight reduction tracking (isolated from paused visualization frames)
	std::array<bool, svk::MAX_FRAMES_IN_FLIGHT> m_hasPendingResult{};
	std::array<double, svk::MAX_FRAMES_IN_FLIGHT> m_pendingTimeStep{};
	uint64_t m_harvestedStepCount = 0;
	double m_harvestedTime = 0.0;

	/// Base command buffer index in m_transferRoutine for result readback (slots [0..MAX_FRAMES_IN_FLIGHT) are UBO
	/// uploads).
	static constexpr uint32_t kResultReadbackTransferCmdBase = svk::MAX_FRAMES_IN_FLIGHT;
	/// Compute shader workgroup size along X (matches [numthreads(256, 1, 1)] in compute_alpha.slang and reduce.slang).
	static constexpr uint32_t kComputeWorkgroupSize = 256U;
	/// Maximum compute dispatch workgroups per dimension per Vulkan physicalDeviceLimits.maxComputeWorkGroupCount[0]
	/// guarantee (65535).
	static constexpr uint32_t kMaxDispatchGroupsPerDim = 65535U;

public:
	EngineInstance(const EngineInstance&) = delete;
	EngineInstance& operator=(const EngineInstance&) = delete;
	EngineInstance(EngineInstance&&) = delete;
	EngineInstance& operator=(EngineInstance&& other) = delete;

	EngineInstance(const Settings& settings,
	               const svk::ConsoleLogger& logger,
	               double& J_T,
	               double& J_av,
	               const Mesh& mesh,
	               const std::vector<AgentData>& agents,
	               const glm::mat4& model);
	~EngineInstance();

	void tick(float timeStep);
	void drainPendingResults();
	[[nodiscard]] uint64_t harvestedStepCount() const { return m_harvestedStepCount; }
	[[nodiscard]] double harvestedTime() const { return m_harvestedTime; }
	[[nodiscard]] bool shouldClose() const
	{
		if constexpr (!Settings::headlessMode)
		{
			if (m_window) { return m_window->shouldClose(); }
		}
		return false;
	}
	[[nodiscard]] bool isPaused() const
	{
		if constexpr (!Settings::headlessMode)
		{
			if (m_camera) { return m_camera->isPaused(); }
		}
		return false;
	}
	bool setPause(bool paused)
	{
		if constexpr (!Settings::headlessMode)
		{
			if (m_camera) { return m_camera->setPause(paused); }
		}
		return false;
	}

private:
	void updateUBO(uint32_t currentFrame, float timeStep);
	void harvestSlot(std::size_t slotIndex);
};
