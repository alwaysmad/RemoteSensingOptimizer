// src/EngineInstance.cpp

#include <algorithm>
#include <cstring>
#include <string>

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
	  m_model(model)
{
	if constexpr (!Settings::headlessMode)
	{
		m_windowContext.emplace();
		m_instance.emplace(
			Settings::appName,
			m_windowContext->getRequiredInstanceExtensions(),
			logger);

		m_window.emplace(
			*m_windowContext,
			m_instance->getInstance(),
			m_settings.windowWidth,
			m_settings.windowHeight,
			std::string(Settings::appName));

		m_camera.emplace(
			m_window->getGLFWwindow(),
			vk::Extent2D{ m_settings.windowWidth, m_settings.windowHeight });

		m_device.emplace(
			*m_instance,
			m_window->getSurface(),
			m_settings.deviceName,
			logger);
	}
	else
	{
		m_instance.emplace(Settings::appName, std::vector<const char*>{}, logger);
		m_device.emplace(*m_instance, m_settings.deviceName, logger);
	}

	m_transferRoutine.emplace(*m_device, svk::MAX_FRAMES_IN_FLIGHT * 2);

	m_uboUpdatedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_computeFinishedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_reduceFinishedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_resultTransferredSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_uboUpdatedSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_computeFinishedSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_reduceFinishedSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_resultTransferredSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_inFlightFences.emplace_back(m_device->device(), vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
	}

	const vk::DeviceSize vertexBytes = static_cast<vk::DeviceSize>(sizeof(VertexCoords) * m_mesh.vertices.size());
	const vk::DeviceSize vertexDataBytes = static_cast<vk::DeviceSize>(sizeof(VertexData) * m_mesh.vertexData.size());

	/*{ // Log resource usage based on device limits and mesh size
		const vk::PhysicalDeviceProperties properties = m_device->physicalDevice().getProperties();
		const uint32_t maxSharedMemoryBytes = properties.limits.maxComputeSharedMemorySize;
		const uint32_t maxUboBytes = properties.limits.maxUniformBufferRange;
		const vk::DeviceSize l1SharedCacheBytes = static_cast<vk::DeviceSize>(sizeof(glm::mat4) * MAX_AGENTS);
		const vk::DeviceSize uboBytes = static_cast<vk::DeviceSize>(sizeof(UBO));
		constexpr double bytesPerKiB = 1024.0;

		m_logger.cInfo(
			"Shared L1 cache: used {:.2f} KiB / max {:.2f} KiB | UBO: used {:.2f} KiB / max {:.2f} KiB",
			static_cast<double>(l1SharedCacheBytes) / bytesPerKiB,
			static_cast<double>(maxSharedMemoryBytes) / bytesPerKiB,
			static_cast<double>(uboBytes) / bytesPerKiB,
			static_cast<double>(maxUboBytes) / bytesPerKiB);
	}*/

	// Create device-local buffer for vertices
	m_vertexBuffer.emplace(m_device->createBuffer(
		vertexBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{ svk::Device::TRANSFER, svk::Device::COMPUTE, svk::Device::GRAPHICS }));
	// Create device-local buffer for vertex data
	m_vertexDataBuffer.emplace(m_device->createBuffer(
		vertexDataBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{ svk::Device::TRANSFER, svk::Device::COMPUTE }));
	// Create device-local buffer for results
	m_resultDeviceBuffer.emplace(m_device->createBuffer(
		sizeof(float),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{ svk::Device::TRANSFER, svk::Device::COMPUTE }));
	// Create host-visible staging buffer for results
	const vk::DeviceSize resultRingSize = sizeof(float) * svk::MAX_FRAMES_IN_FLIGHT;
	m_resultStagingBuffer.emplace(m_device->createBuffer(
		resultRingSize,
		vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		{ svk::Device::TRANSFER }));
	m_resultMap.emplace(m_resultStagingBuffer->map(0, resultRingSize));

	{ // Create staging buffer for vertices
		auto vertexStaging = m_device->createBuffer(
			vertexBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{ svk::Device::TRANSFER });
		{ // copy vertices to staging buffer
			auto map = vertexStaging.map(0, vertexBytes);
			std::memcpy(map.get(), m_mesh.vertices.data(), static_cast<size_t>(vertexBytes));
		}
		// Copy from staging buffer to device-local buffer using the Transfer Queue
		m_transferRoutine->bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			vertexStaging,
			*m_vertexBuffer,
			0,
			0,
			vertexBytes);
		m_transferRoutine->submitCommands(0);
		m_device->transferQueue().waitIdle();
	}

	{ // Create staging buffer for vertex data
		auto vertexDataStaging = m_device->createBuffer(
			vertexDataBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{ svk::Device::TRANSFER });
		{ // copy vertex data to staging buffer
			auto map = vertexDataStaging.map(0, vertexDataBytes);
			std::memcpy(map.get(), m_mesh.vertexData.data(), static_cast<size_t>(vertexDataBytes));
		}
		// Copy from staging buffer to device-local buffer using the Transfer Queue
		m_transferRoutine->bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			vertexDataStaging,
			*m_vertexDataBuffer,
			0,
			0,
			vertexDataBytes);
		m_transferRoutine->submitCommands(0);
		m_device->transferQueue().waitIdle();
	}

	{
		const vk::DeviceSize rawSize = sizeof(UBO);
		const vk::DeviceSize alignment = m_device->physicalDevice().getProperties().limits.minUniformBufferOffsetAlignment;
		m_uboFrameSize = (rawSize + alignment - 1) / alignment * alignment;

		const vk::DeviceSize stagingTotalSize = m_uboFrameSize * svk::MAX_FRAMES_IN_FLIGHT;

		m_uboDeviceBuffer.emplace(m_device->createBuffer(
			rawSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			{ svk::Device::TRANSFER, svk::Device::COMPUTE, svk::Device::GRAPHICS }));

		m_uboStagingBuffer.emplace(m_device->createBuffer(
			stagingTotalSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{ svk::Device::TRANSFER }));

		m_uboStagingMap.emplace(m_uboStagingBuffer->map(0, stagingTotalSize));
		m_uboStagingPtrs.resize(svk::MAX_FRAMES_IN_FLIGHT);
		char* mappedBase = static_cast<char*>(m_uboStagingMap->get());
		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			const vk::DeviceSize offset = m_uboFrameSize * i;
			m_uboStagingPtrs[i] = mappedBase + offset;
			m_transferRoutine->bakeCommands(
				i,
				vk::CommandBufferUsageFlags{},
				*m_uboStagingBuffer,
				*m_uboDeviceBuffer,
				offset,
				0,
				sizeof(UBO));
		}
	}

	{
		const vk::raii::ShaderModule computeModule(m_device->device(), compute_alpha::smci);
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
		m_computeRoutine.emplace(*m_device, computeStage, computeDescriptorBindings, 1);
		m_computeRoutine->registerBuffers(0, {
			svk::BufferBinding(*m_uboDeviceBuffer, 0),
			svk::BufferBinding(*m_vertexBuffer, 1),
			svk::BufferBinding(*m_vertexDataBuffer, 2),
		});

		const uint32_t totalGroups = (static_cast<uint32_t>(m_mesh.vertices.size()) + 255) / 256;
		constexpr uint32_t MAX_GROUPS = 65535u;
		const uint32_t groupCountX = std::min(totalGroups, MAX_GROUPS);
		const uint32_t groupCountY = (totalGroups + MAX_GROUPS - 1) / MAX_GROUPS;

		m_computeRoutine->bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eSimultaneousUse,
			groupCountX,
			groupCountY,
			1);
	}

	{
		const vk::raii::ShaderModule reduceModule(m_device->device(), reduce::smci);
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

		m_reduceRoutine.emplace(*m_device, reduceStage, reduceDescriptorBindings, svk::MAX_FRAMES_IN_FLIGHT);
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
		m_transferRoutine->bakeCommands(
			kResultReadbackTransferCmdBase + i,
			vk::CommandBufferUsageFlags{},
			*m_resultDeviceBuffer,
			*m_resultStagingBuffer,
			0,
			i * sizeof(float),
			sizeof(float));
	}

	if constexpr (!Settings::headlessMode)
	{
		m_swapchain.emplace(*m_device, *m_window);
		m_renderRoutine.emplace(*m_device, *m_swapchain, svk::MAX_FRAMES_IN_FLIGHT);

		{
			const vk::raii::ShaderModule meshModule(m_device->device(), mesh::smci);
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
			auto& meshTask = m_renderRoutine->m_tasks.emplace_back(
				m_device->device(),
				shaderStages,
				vertexInput,
				vk::PrimitiveTopology::ePointList,
				vk::CullModeFlagBits::eNone,
				descriptorBindings,
				m_swapchain->getFormat(),
				m_renderRoutine->getDepthFormat());
			meshTask.m_active = true;
			meshTask.registerBuffers(
				std::vector<svk::BufferBinding> { svk::BufferBinding(*m_uboDeviceBuffer, 0) },
				std::vector<svk::BufferBinding> { svk::BufferBinding(*m_vertexBuffer, 0) },
				std::nullopt,
				static_cast<uint32_t>(m_mesh.vertices.size()),
				1);
		}

		{
			const vk::raii::ShaderModule satelliteModule(m_device->device(), satellite::smci);
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

			const vk::PipelineVertexInputStateCreateInfo vertexInput{};
			const std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings = {
				vk::DescriptorSetLayoutBinding {
					.binding = 0,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.descriptorCount = 1,
					.stageFlags = vk::ShaderStageFlagBits::eVertex,
				}
			};

			auto& satelliteTask = m_renderRoutine->m_tasks.emplace_back(
				m_device->device(),
				shaderStages,
				vertexInput,
				vk::PrimitiveTopology::eLineList,
				vk::CullModeFlagBits::eNone,
				descriptorBindings,
				m_swapchain->getFormat(),
				m_renderRoutine->getDepthFormat());
			satelliteTask.m_active = true;
			satelliteTask.registerBuffers(
				std::vector<svk::BufferBinding> { svk::BufferBinding(*m_uboDeviceBuffer, 0) },
				std::vector<svk::BufferBinding> {},
				std::nullopt,
				32,
				static_cast<uint32_t>(m_agents.size()));
		}
	}
}

void EngineInstance::tick(float timeStep)
{
	if constexpr (!Settings::headlessMode)
	{
		m_window->pollEvents();
		m_window->updateFPS(std::string(Settings::appName));
	}

	const vk::Fence fence = *m_inFlightFences[m_currentFrame];
	if (m_device->device().waitForFences({ fence }, vk::True, UINT64_MAX) != vk::Result::eSuccess)
		{ throw std::runtime_error("Fence wait failed"); }
	m_device->device().resetFences({ fence });

	updateUBO(m_currentFrame, timeStep);

	if constexpr (!Settings::headlessMode)
	{
		if (m_camera->isPaused())
		{
			m_renderRoutine->draw(m_currentFrame, fence, *m_uboUpdatedSemaphores[m_currentFrame]);
			m_currentFrame = svk::advanceFrame(m_currentFrame);
			return;
		}
	}	

	float* mappedResults = static_cast<float*>(m_resultMap->get());
	const float reducedResult = mappedResults[m_currentFrame];
	m_J_T = reducedResult;
	m_J_av += reducedResult * timeStep;

	m_computeRoutine->submitCommands(
		0,
		*m_uboUpdatedSemaphores[m_currentFrame],
		*m_computeFinishedSemaphores[m_currentFrame]);

	m_reduceRoutine->submitCommands(
		m_currentFrame,
		*m_computeFinishedSemaphores[m_currentFrame],
		*m_reduceFinishedSemaphores[m_currentFrame]);

	m_transferRoutine->submitCommands(
		kResultReadbackTransferCmdBase + m_currentFrame,
		*m_reduceFinishedSemaphores[m_currentFrame],
		Settings::headlessMode ? nullptr : *m_resultTransferredSemaphores[m_currentFrame],
		Settings::headlessMode ? fence : nullptr);

	if constexpr (!Settings::headlessMode)
		{ m_renderRoutine->draw(m_currentFrame, fence, *m_resultTransferredSemaphores[m_currentFrame]); }

	m_currentFrame = svk::advanceFrame(m_currentFrame);
}

void EngineInstance::updateUBO(uint32_t currentFrame, float timeStep)
{
	if constexpr (!Settings::headlessMode)
	{
		m_ubo.view = m_camera->getView();
		m_ubo.proj = m_camera->getProj();
	}
	else
	{
		m_ubo.view = glm::mat4(1.0f);
		m_ubo.proj = glm::mat4(1.0f);
	}
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

	std::memcpy(m_uboStagingPtrs[currentFrame], &m_ubo, sizeof(UBO));
	m_transferRoutine->submitCommands(
		currentFrame,
		nullptr,
		*m_uboUpdatedSemaphores[currentFrame]);
}

void EngineInstance::reloadMesh()
{
	m_device->waitIdle();

	const vk::DeviceSize vertexBytes = static_cast<vk::DeviceSize>(sizeof(VertexCoords) * m_mesh.vertices.size());
	const vk::DeviceSize vertexDataBytes = static_cast<vk::DeviceSize>(sizeof(VertexData) * m_mesh.vertexData.size());

	{ // Create staging buffer for vertices
		auto vertexStaging = m_device->createBuffer(
			vertexBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{ svk::Device::TRANSFER });
		{ // copy vertices to staging buffer
			auto map = vertexStaging.map(0, vertexBytes);
			std::memcpy(map.get(), m_mesh.vertices.data(), static_cast<size_t>(vertexBytes));
		}
		// Copy from staging buffer to device-local buffer using the Transfer Queue
		m_transferRoutine->bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			vertexStaging,
			*m_vertexBuffer,
			0,
			0,
			vertexBytes);
		m_transferRoutine->submitCommands(0);
		m_device->transferQueue().waitIdle();
	}

	{ // Create staging buffer for vertex data
		auto vertexDataStaging = m_device->createBuffer(
			vertexDataBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{ svk::Device::TRANSFER });
		{ // copy vertex data to staging buffer
			auto map = vertexDataStaging.map(0, vertexDataBytes);
			std::memcpy(map.get(), m_mesh.vertexData.data(), static_cast<size_t>(vertexDataBytes));
		}
		// Copy from staging buffer to device-local buffer using the Transfer Queue
		m_transferRoutine->bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			vertexDataStaging,
			*m_vertexDataBuffer,
			0,
			0,
			vertexDataBytes);
		m_transferRoutine->submitCommands(0);
		m_device->transferQueue().waitIdle();
	}
}

EngineInstance::~EngineInstance()
{
	if (m_device)
		{ m_device->waitIdle(); }
}
