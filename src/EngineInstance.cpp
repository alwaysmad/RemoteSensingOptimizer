// src/EngineInstance.cpp

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string> // std::string

#include <glm/glm.hpp>

#include "EngineInstance.hpp"

#include "compute_alpha.hpp"
#include "mesh.hpp"
#include "reduce.hpp"
#include "satellite.hpp"

EngineInstance::EngineInstance(
	const Settings& settings,
	const svk::Logger& logger,
	float& J_T,
	float& J_av,
	const Mesh& mesh,
	const std::vector<AgentData>& agents,
	const glm::mat4& model)
	: m_settings(settings),
	  m_logger(logger),
	  m_J_T(J_T),
	  m_J_av(J_av),
	  m_mesh(mesh),
	  m_agents(agents),
	  m_model(model),
	  m_windowContext(),
	  m_instance(std::string(Settings::appName), m_windowContext.getRequiredInstanceExtensions(), logger),
	  m_window(m_windowContext, m_instance.getInstance(), m_settings.windowWidth, m_settings.windowHeight, std::string(Settings::appName)),
	  m_camera(m_window.getGLFWwindow(), vk::Extent2D{ m_settings.windowWidth, m_settings.windowHeight }),
	  m_device(m_instance, m_window.getSurface(), m_settings.deviceName, logger),
	  m_swapchain(m_device, m_window),
	  m_renderRoutine(m_device, m_swapchain, svk::MAX_FRAMES_IN_FLIGHT),
	  m_transferRoutine(m_device, svk::MAX_FRAMES_IN_FLIGHT * 2)
{
	m_uboUpdatedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_computeFinishedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_reduceFinishedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_resultTransferredSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_uboUpdatedSemaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo {});
		m_computeFinishedSemaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo {});
		m_reduceFinishedSemaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo {});
		m_resultTransferredSemaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo {});
		m_inFlightFences.emplace_back(m_device.device(), vk::FenceCreateInfo {.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	const vk::DeviceSize vertexBytes = static_cast<vk::DeviceSize>(sizeof(VertexCoords) * m_mesh.vertices.size());
	const vk::DeviceSize vertexDataBytes = static_cast<vk::DeviceSize>(sizeof(VertexData) * m_mesh.vertexData.size());

	// Create final (device local) buffers
	m_vertexBuffer.emplace(m_device.createBuffer(
		vertexBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{svk::Device::TRANSFER, svk::Device::COMPUTE, svk::Device::GRAPHICS}));
	m_vertexDataBuffer.emplace(m_device.createBuffer(
		vertexDataBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{svk::Device::TRANSFER, svk::Device::COMPUTE}));
	m_resultDeviceBuffer.emplace(m_device.createBuffer(
		sizeof(float),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{svk::Device::TRANSFER, svk::Device::COMPUTE}));
	const vk::DeviceSize resultRingSize = sizeof(float) * svk::MAX_FRAMES_IN_FLIGHT;
	m_resultStagingBuffer.emplace(m_device.createBuffer(
		resultRingSize,
		vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		{svk::Device::TRANSFER}));
	m_resultMap.emplace(m_resultStagingBuffer->map(0, resultRingSize));
	
	{ // Upload vertex data via staging buffer
		auto vertexStaging = m_device.createBuffer(
			vertexBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{svk::Device::TRANSFER});
		{ // Map to copy
			auto map = vertexStaging.map(0, vertexBytes);
			std::memcpy(map.get(), m_mesh.vertices.data(), static_cast<size_t>(vertexBytes));
		}
		// copy from staging to device local
		m_transferRoutine.bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			vertexStaging,
			*m_vertexBuffer,
			0,
			0,
			vertexBytes);
		m_transferRoutine.submitCommands(0);
		m_device.transferQueue().waitIdle();
	}

	{ // Upload vertexData data via staging buffer
		auto vertexDataStaging = m_device.createBuffer(
			vertexDataBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{svk::Device::TRANSFER});

		{ // Map to copy
			auto map = vertexDataStaging.map(0, vertexDataBytes);
			std::memcpy(map.get(), m_mesh.vertexData.data(), static_cast<size_t>(vertexDataBytes));
		}
		// copy from staging to device local
		m_transferRoutine.bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			vertexDataStaging,
			*m_vertexDataBuffer,
			0,
			0,
			vertexDataBytes);
		m_transferRoutine.submitCommands(0);
		m_device.transferQueue().waitIdle();
	}

	{ // Initialize UBO system
		// Frame size must be aligned to minUniformBufferOffsetAlignment
		const vk::DeviceSize rawSize = sizeof(UBO);
		const vk::DeviceSize alignment = m_device.physicalDevice().getProperties().limits.minUniformBufferOffsetAlignment;
		m_uboFrameSize = (rawSize + alignment - 1) / alignment * alignment;

		const vk::DeviceSize stagingTotalSize = m_uboFrameSize * svk::MAX_FRAMES_IN_FLIGHT;

		// Device-local UBO buffer
		m_uboDeviceBuffer.emplace(m_device.createBuffer(
			rawSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			{svk::Device::TRANSFER, svk::Device::COMPUTE, svk::Device::GRAPHICS}));

		// Host-visible staging ring buffer for UBO updates
		m_uboStagingBuffer.emplace(m_device.createBuffer(
			stagingTotalSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{svk::Device::TRANSFER}));

		// Single persistent mapping + per-frame raw pointers (one map only).
		m_uboStagingMap.emplace(m_uboStagingBuffer->map(0, stagingTotalSize));
		m_uboStagingPtrs.resize(svk::MAX_FRAMES_IN_FLIGHT);
		char* mappedBase = static_cast<char*>(m_uboStagingMap->get());
		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			const vk::DeviceSize offset = m_uboFrameSize * i;
			// Store offsets in pointers
			m_uboStagingPtrs[i] = mappedBase + offset;
			// Bake frame-specific transfer command once, then reuse every tick.
			m_transferRoutine.bakeCommands(
				i,
				vk::CommandBufferUsageFlags{},
				*m_uboStagingBuffer,
				*m_uboDeviceBuffer,
				offset,
				0,
				sizeof(UBO));
		}
	}

	{ // Create compute routine and bake commands
		const vk::raii::ShaderModule computeModule(m_device.device(), compute_alpha::smci);
		const vk::PipelineShaderStageCreateInfo computeStage {
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = *computeModule,
			.pName = "computeMain",
		};
		const std::vector<vk::DescriptorSetLayoutBinding> computeDescriptorBindings = {
			vk::DescriptorSetLayoutBinding {
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
			},
			vk::DescriptorSetLayoutBinding {
				.binding = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
			},
			vk::DescriptorSetLayoutBinding {
				.binding = 2,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
			},
		};
		m_computeRoutine.emplace(m_device, computeStage, computeDescriptorBindings, 1);
		m_computeRoutine->registerBuffers(0, {
			svk::BufferBinding(*m_uboDeviceBuffer, 0),
			svk::BufferBinding(*m_vertexBuffer, 1),
			svk::BufferBinding(*m_vertexDataBuffer, 2),
		});

		// Calculate total workgroups needed
		const uint32_t totalGroups = (static_cast<uint32_t>(m_mesh.vertices.size()) + 255) / 256;
		// Wrap them into a 2D grid
		constexpr uint32_t MAX_GROUPS = 65535u;
		const uint32_t groupCountX = std::min(totalGroups, MAX_GROUPS);
		const uint32_t groupCountY = (totalGroups + MAX_GROUPS - 1) / MAX_GROUPS;

		// Dispatch with Y included!
		m_computeRoutine->bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eSimultaneousUse,
			groupCountX,
			groupCountY,
			1);
	}

	{ // Create reduce routine and bake commands
		const vk::raii::ShaderModule reduceModule(m_device.device(), reduce::smci);
		const vk::PipelineShaderStageCreateInfo reduceStage {
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = *reduceModule,
			.pName = "reduceMain",
		};
		const std::vector<vk::DescriptorSetLayoutBinding> reduceDescriptorBindings = {
			vk::DescriptorSetLayoutBinding {
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
			},
			vk::DescriptorSetLayoutBinding {
				.binding = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
			},
			vk::DescriptorSetLayoutBinding {
				.binding = 2,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
			},
			vk::DescriptorSetLayoutBinding {
				.binding = 3,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
			},
		};

		m_reduceRoutine.emplace(m_device, reduceStage, reduceDescriptorBindings, svk::MAX_FRAMES_IN_FLIGHT);
		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			m_reduceRoutine->registerBuffers(
				i,
				{
					svk::BufferBinding(*m_uboDeviceBuffer, 0),
					svk::BufferBinding(*m_vertexBuffer, 1),
					svk::BufferBinding(*m_vertexDataBuffer, 2),
					svk::BufferBinding(*m_resultDeviceBuffer, 3),
				},
				svk::BufferBinding(*m_resultDeviceBuffer, 3));
		}

		const uint32_t totalGroups = (static_cast<uint32_t>(m_mesh.vertices.size()) + 255) / 256;
		constexpr uint32_t MAX_GROUPS = 65535u;
		const uint32_t groupCountX = std::min(totalGroups, MAX_GROUPS);
		const uint32_t groupCountY = (totalGroups + MAX_GROUPS - 1) / MAX_GROUPS;

		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			m_reduceRoutine->bakeCommands(
				i,
				vk::CommandBufferUsageFlags{},
				groupCountX,
				groupCountY,
				1);
		}
	}

	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_transferRoutine.bakeCommands(
			kResultReadbackTransferCmdBase + i,
			vk::CommandBufferUsageFlags{},
			*m_resultDeviceBuffer,
			*m_resultStagingBuffer,
			0,
			i * sizeof(float),
			sizeof(float));
	}

	{ // Create shader module and render task for the mesh
		const vk::raii::ShaderModule meshModule(m_device.device(), mesh::smci);
		const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = *meshModule,
				.pName = "vertMain",
			},
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = *meshModule,
				.pName = "fragMain",
			},
		};

		const vk::PipelineVertexInputStateCreateInfo vertexInput {
			.vertexBindingDescriptionCount = static_cast<uint32_t>(kVertexCoordsBindingDescriptions.size()),
			.pVertexBindingDescriptions = kVertexCoordsBindingDescriptions.data(),
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(kVertexCoordsAttributeDescriptions.size()),
			.pVertexAttributeDescriptions = kVertexCoordsAttributeDescriptions.data(),
		};
		const std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings = {
			vk::DescriptorSetLayoutBinding {
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex,
			}
		};
		auto& meshTask = m_renderRoutine.m_tasks.emplace_back(
			m_device.device(),
			shaderStages,
			vertexInput,
			vk::PrimitiveTopology::ePointList,
			vk::CullModeFlagBits::eNone,
			descriptorBindings,
			m_swapchain.getFormat(),
			m_renderRoutine.getDepthFormat());
		meshTask.m_active = true;
		meshTask.registerBuffers(
			std::vector<svk::BufferBinding> { svk::BufferBinding(*m_uboDeviceBuffer, 0) },
			std::vector<svk::BufferBinding> { svk::BufferBinding(*m_vertexBuffer, 0) },
			std::nullopt,
			static_cast<uint32_t>(m_mesh.vertices.size()),
			1);
	}

	{ // Create shader module and render task for satellite frustum lines
		const vk::raii::ShaderModule satelliteModule(m_device.device(), satellite::smci);
		const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = *satelliteModule,
				.pName = "vertMain",
			},
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = *satelliteModule,
				.pName = "fragMain",
			},
		};

		const vk::PipelineVertexInputStateCreateInfo vertexInput {};
		const std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings = {
			vk::DescriptorSetLayoutBinding {
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eVertex,
			}
		};

		auto& satelliteTask = m_renderRoutine.m_tasks.emplace_back(
			m_device.device(),
			shaderStages,
			vertexInput,
			vk::PrimitiveTopology::eLineList,
			vk::CullModeFlagBits::eNone,
			descriptorBindings,
			m_swapchain.getFormat(),
			m_renderRoutine.getDepthFormat());
		satelliteTask.m_active = true;
		satelliteTask.registerBuffers(
			std::vector<svk::BufferBinding> { svk::BufferBinding(*m_uboDeviceBuffer, 0) },
			std::vector<svk::BufferBinding> {},
			std::nullopt,
			32,
			static_cast<uint32_t>(m_agents.size()));
	}
}

void EngineInstance::tick(float timeStep)
{
	m_window.pollEvents();
	m_window.updateFPS(std::string(Settings::appName));
	const bool paused = m_camera.isPaused();

	const vk::Fence fence = *m_inFlightFences[m_currentFrame];
	if (m_device.device().waitForFences({fence}, vk::True, UINT64_MAX) != vk::Result::eSuccess)
		{ throw std::runtime_error("Fence wait failed"); }
	m_device.device().resetFences({fence});

	updateUBO(m_currentFrame, timeStep);

	if (paused)
	{
		m_renderRoutine.draw(m_currentFrame, fence, *m_uboUpdatedSemaphores[m_currentFrame]);
		m_currentFrame = svk::advanceFrame(m_currentFrame);
		return;
	}

	float* mappedResults = static_cast<float*>(m_resultMap->get());
	const float reducedResult = mappedResults[m_currentFrame];
	m_J_T = reducedResult;
	m_J_av += reducedResult * timeStep;
	std::printf("[Reduce] alpha*s*w sum: %.6f\n", reducedResult);

	m_computeRoutine->submitCommands(
		0,
		*m_uboUpdatedSemaphores[m_currentFrame],
		*m_computeFinishedSemaphores[m_currentFrame]);

	m_reduceRoutine->submitCommands(
		m_currentFrame,
		*m_computeFinishedSemaphores[m_currentFrame],
		*m_reduceFinishedSemaphores[m_currentFrame]);

	m_transferRoutine.submitCommands(
		kResultReadbackTransferCmdBase + m_currentFrame,
		*m_reduceFinishedSemaphores[m_currentFrame],
		*m_resultTransferredSemaphores[m_currentFrame]);

	m_renderRoutine.draw(m_currentFrame, fence, *m_resultTransferredSemaphores[m_currentFrame]);
	m_currentFrame = svk::advanceFrame(m_currentFrame);
}

bool EngineInstance::shouldClose() const { return m_window.shouldClose(); }

bool EngineInstance::isPaused() const { return m_camera.isPaused(); }

void EngineInstance::updateUBO(uint32_t currentFrame, float timeStep)
{
	m_ubo.view = m_camera.getView();
	m_ubo.proj = m_camera.getProj();
	m_ubo.model = m_model;
	m_ubo.vertexCount = static_cast<uint32_t>(m_mesh.vertices.size());
	m_ubo.agentCount = std::min(static_cast<uint32_t>(m_agents.size()), MAX_AGENTS);
	m_ubo.timeStep = timeStep;

	if (m_ubo.agentCount > 0u)
	{
		std::memcpy(
			m_ubo.agents,
			m_agents.data(),
			static_cast<size_t>(m_ubo.agentCount) * sizeof(AgentData));
	}

	// Copy UBO data to the correct precomputed frame pointer.
	std::memcpy(m_uboStagingPtrs[currentFrame], &m_ubo, sizeof(UBO));

	// Submit pre-baked transfer command with frame-specific signal semaphore.
	m_transferRoutine.submitCommands(
		currentFrame,
		nullptr,
		*m_uboUpdatedSemaphores[currentFrame]);
}

EngineInstance::~EngineInstance() { m_device.waitIdle(); }
