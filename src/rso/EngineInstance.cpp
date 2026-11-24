// src/EngineInstance.cpp

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

#include "EngineInstance.hpp"

#include "compute_alpha.hpp"
#include "mesh.hpp"
#include "reduce.hpp"
#include "reduce_final.hpp"
#include "satellite.hpp"
#include "shaders/shared_types.slangh"

EngineInstance::EngineInstance(const Settings& settings,
                               const svk::ConsoleLogger& logger,
                               double& J_T,
                               double& J_av,
                               const Mesh& mesh,
                               const std::vector<AgentData>& agents,
                               const glm::mat4& model)
    : m_settings(settings), m_logger(logger), m_J_T(J_T), m_J_av(J_av), m_mesh(mesh), m_agents(agents), m_model(model)
{
	if constexpr (!Settings::headlessMode) // NOLINT(bugprone-branch-clone)
	{
		m_windowContext.emplace();
		m_instance.emplace(Settings::appName, m_windowContext->getRequiredInstanceExtensions(), logger);

		m_window.emplace(*m_windowContext,
		                 m_instance->getInstance(),
		                 m_settings.windowWidth,
		                 m_settings.windowHeight,
		                 std::string(Settings::appName));

		m_camera.emplace(m_window->getGLFWwindow(), vk::Extent2D{m_settings.windowWidth, m_settings.windowHeight});

		m_device.emplace(*m_instance, m_window->getSurface(), m_settings.deviceName, logger);
	}
	else
	{
		m_instance.emplace(Settings::appName, std::vector<const char*>{}, logger);
		m_device.emplace(*m_instance, m_settings.deviceName, logger);
	}

	m_transferRoutine.emplace(*m_device, svk::MAX_FRAMES_IN_FLIGHT * 2);

	m_uboUpdatedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_computeFinishedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_reducePartialsFinishedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_reduceFinishedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_resultTransferredSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_uboUpdatedSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_computeFinishedSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_reducePartialsFinishedSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_reduceFinishedSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_resultTransferredSemaphores.emplace_back(m_device->device(), vk::SemaphoreCreateInfo{});
		m_inFlightFences.emplace_back(m_device->device(),
		                              vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	const vk::DeviceSize vertexBytes = static_cast<vk::DeviceSize>(sizeof(VertexCoords) * m_mesh.vertices.size());
	const vk::DeviceSize vertexDataBytes = static_cast<vk::DeviceSize>(sizeof(VertexData) * m_mesh.vertexData.size());

	const vk::PhysicalDeviceLimits deviceLimits = m_device->physicalDevice().getProperties().limits;
	{
		const uint32_t maxSharedMemoryBytes = deviceLimits.maxComputeSharedMemorySize;
		const uint32_t maxUboBytes = deviceLimits.maxUniformBufferRange;
		const vk::DeviceSize l1SharedCacheBytes = static_cast<vk::DeviceSize>(sizeof(glm::mat4) * MAX_AGENTS);
		const vk::DeviceSize uboBytes = static_cast<vk::DeviceSize>(sizeof(UBO));
		constexpr double bytesPerKiB = 1024.0;

		m_logger.cInfo("Shared L1 cache: used {:.2f} KiB / max {:.2f} KiB | UBO: used {:.2f} KiB / max {:.2f} KiB",
		               static_cast<double>(l1SharedCacheBytes) / bytesPerKiB,
		               static_cast<double>(maxSharedMemoryBytes) / bytesPerKiB,
		               static_cast<double>(uboBytes) / bytesPerKiB,
		               static_cast<double>(maxUboBytes) / bytesPerKiB);
	}

	static constexpr std::array<svk::Device::QueueType, 2> kHeadlessComputeTransferQueues{
	    svk::Device::TRANSFER,
	    svk::Device::COMPUTE,
	};
	static constexpr std::array<svk::Device::QueueType, 3> kGraphicsComputeTransferQueues{
	    svk::Device::TRANSFER,
	    svk::Device::COMPUTE,
	    svk::Device::GRAPHICS,
	};
	const std::span<const svk::Device::QueueType> sharedComputeQueues =
	    Settings::headlessMode ? std::span<const svk::Device::QueueType>(kHeadlessComputeTransferQueues)
	                           : std::span<const svk::Device::QueueType>(kGraphicsComputeTransferQueues);

	vk::BufferUsageFlags vertexUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer;
	if constexpr (!Settings::headlessMode) { vertexUsage |= vk::BufferUsageFlagBits::eVertexBuffer; }

	m_vertexBuffer.emplace(m_device->createBuffer(
	    vertexBytes, vertexUsage, vk::MemoryPropertyFlagBits::eDeviceLocal, sharedComputeQueues));
	m_vertexDataBuffer.emplace(
	    m_device->createBuffer(vertexDataBytes,
	                           vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
	                           vk::MemoryPropertyFlagBits::eDeviceLocal,
	                           {svk::Device::TRANSFER, svk::Device::COMPUTE}));
	const uint32_t totalGroups =
	    (static_cast<uint32_t>(m_mesh.vertices.size()) + kComputeWorkgroupSize - 1U) / kComputeWorkgroupSize;
	const uint32_t groupCountX = std::min(totalGroups, kMaxDispatchGroupsPerDim);
	const uint32_t groupCountY = (totalGroups + kMaxDispatchGroupsPerDim - 1U) / kMaxDispatchGroupsPerDim;

	const vk::DeviceSize storageAlignment =
	    std::max<vk::DeviceSize>(sizeof(float), deviceLimits.minStorageBufferOffsetAlignment);
	const vk::DeviceSize rawPartialsSliceBytes =
	    static_cast<vk::DeviceSize>(std::max<uint32_t>(1U, totalGroups)) * sizeof(float);
	const vk::DeviceSize partialsFrameStride =
	    ((rawPartialsSliceBytes + storageAlignment - 1) / storageAlignment) * storageAlignment;
	const vk::DeviceSize partialsRingSize = partialsFrameStride * svk::MAX_FRAMES_IN_FLIGHT;
	m_partialsDeviceBuffer.emplace(m_device->createBuffer(partialsRingSize,
	                                                      vk::BufferUsageFlagBits::eStorageBuffer,
	                                                      vk::MemoryPropertyFlagBits::eDeviceLocal,
	                                                      {svk::Device::COMPUTE}));

	m_resultFrameStride = ((sizeof(float) + storageAlignment - 1) / storageAlignment) * storageAlignment;
	const vk::DeviceSize resultDeviceRingSize = m_resultFrameStride * svk::MAX_FRAMES_IN_FLIGHT;
	m_resultDeviceBuffer.emplace(
	    m_device->createBuffer(resultDeviceRingSize,
	                           vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
	                           vk::MemoryPropertyFlagBits::eDeviceLocal,
	                           {svk::Device::TRANSFER, svk::Device::COMPUTE}));
	const vk::DeviceSize resultRingSize = sizeof(float) * svk::MAX_FRAMES_IN_FLIGHT;
	m_resultStagingBuffer.emplace(
	    m_device->createBuffer(resultRingSize,
	                           vk::BufferUsageFlagBits::eTransferDst,
	                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
	                           {svk::Device::TRANSFER}));
	m_resultMap.emplace(m_resultStagingBuffer->map(0, resultRingSize));
	// zero-init the ring buffer to avoid reading uninitialized values on the first few frames
	std::memset(m_resultMap->get(), 0, static_cast<size_t>(resultRingSize));

	{
		// Create a staging buffer for vertex data and copy the mesh vertices into it, then transfer to the device-local
		// vertex buffer
		auto vertexStaging =
		    m_device->createBuffer(vertexBytes,
		                           vk::BufferUsageFlagBits::eTransferSrc,
		                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		                           {svk::Device::TRANSFER});
		{
			auto map = vertexStaging.map(0, vertexBytes);
			std::memcpy(map.get(), m_mesh.vertices.data(), static_cast<size_t>(vertexBytes));
		}
		m_transferRoutine->bakeCommands(
		    0, vk::CommandBufferUsageFlagBits::eOneTimeSubmit, vertexStaging, *m_vertexBuffer, 0, 0, vertexBytes);
		m_transferRoutine->submitCommands(0);
		m_device->transferQueue().waitIdle();
	}

	{
		// Create a staging buffer for vertex data and copy the mesh vertex data into it, then transfer to the
		// device-local vertex data buffer
		auto vertexDataStaging =
		    m_device->createBuffer(vertexDataBytes,
		                           vk::BufferUsageFlagBits::eTransferSrc,
		                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		                           {svk::Device::TRANSFER});
		{
			auto map = vertexDataStaging.map(0, vertexDataBytes);
			std::memcpy(map.get(), m_mesh.vertexData.data(), static_cast<size_t>(vertexDataBytes));
		}
		m_transferRoutine->bakeCommands(0,
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
		// Create UBO and staging-UBO buffers; bake transfer commands for them
		const vk::DeviceSize rawSize = sizeof(UBO);
		const vk::DeviceSize alignment = deviceLimits.minUniformBufferOffsetAlignment;
		m_uboFrameSize = (rawSize + alignment - 1) / alignment * alignment;

		const vk::DeviceSize ringTotalSize = m_uboFrameSize * svk::MAX_FRAMES_IN_FLIGHT;

		m_uboDeviceBuffer.emplace(
		    m_device->createBuffer(ringTotalSize,
		                           vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
		                           vk::MemoryPropertyFlagBits::eDeviceLocal,
		                           sharedComputeQueues));

		m_uboStagingBuffer.emplace(
		    m_device->createBuffer(ringTotalSize,
		                           vk::BufferUsageFlagBits::eTransferSrc,
		                           vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
		                           {svk::Device::TRANSFER}));

		m_uboStagingMap.emplace(m_uboStagingBuffer->map(0, ringTotalSize));
		m_uboStagingPtrs.resize(svk::MAX_FRAMES_IN_FLIGHT);
		char* mappedBase = static_cast<char*>(m_uboStagingMap->get());
		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			const vk::DeviceSize offset = m_uboFrameSize * i;
			m_uboStagingPtrs[i] = mappedBase + offset;
			m_transferRoutine->bakeCommands(
			    i, vk::CommandBufferUsageFlags{}, *m_uboStagingBuffer, *m_uboDeviceBuffer, offset, offset, sizeof(UBO));
		}
	}

	{
		const vk::raii::ShaderModule computeModule(m_device->device(), compute_alpha::smci);
		const vk::PipelineShaderStageCreateInfo computeStage{
		    .stage = vk::ShaderStageFlagBits::eCompute,
		    .module = *computeModule,
		    .pName = "computeMain",
		};
		const std::vector<vk::DescriptorSetLayoutBinding> computeDescriptorBindings = {
		    vk::DescriptorSetLayoutBinding{
		        .binding = 0,
		        .descriptorType = vk::DescriptorType::eUniformBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		    vk::DescriptorSetLayoutBinding{
		        .binding = 1,
		        .descriptorType = vk::DescriptorType::eStorageBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		    vk::DescriptorSetLayoutBinding{
		        .binding = 2,
		        .descriptorType = vk::DescriptorType::eStorageBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		};
		// Ring-buffer per frame-in-flight rather than using eSimultaneousUse:
		// Hardware vendors (NVIDIA, Arm, PowerVR) advise against VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
		// due to driver-side tracking overhead, state cloning, or queue serialization.
		// Dedicated per-frame command buffers maintain architectural symmetry with the reduction routines
		// and guarantee race-free execution across in-flight frames.
		m_computeRoutine.emplace(*m_device, computeStage, computeDescriptorBindings, svk::MAX_FRAMES_IN_FLIGHT);
		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			const vk::DeviceSize uboOffset = m_uboFrameSize * i;
			m_computeRoutine->registerBuffers(i,
			                                  {
			                                      svk::BufferBinding(*m_uboDeviceBuffer, 0, uboOffset, sizeof(UBO)),
			                                      svk::BufferBinding(*m_vertexBuffer, 1),
			                                      svk::BufferBinding(*m_vertexDataBuffer, 2),
			                                  });
		}

		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			m_computeRoutine->bakeCommands(i, vk::CommandBufferUsageFlags{}, groupCountX, groupCountY, 1);
		}
	}

	{
		const vk::raii::ShaderModule reduceModule(m_device->device(), reduce::smci);
		const vk::PipelineShaderStageCreateInfo reducePartialsStage{
		    .stage = vk::ShaderStageFlagBits::eCompute,
		    .module = *reduceModule,
		    .pName = "reducePartials",
		};
		const std::vector<vk::DescriptorSetLayoutBinding> reducePartialsBindings = {
		    vk::DescriptorSetLayoutBinding{
		        .binding = 0,
		        .descriptorType = vk::DescriptorType::eUniformBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		    vk::DescriptorSetLayoutBinding{
		        .binding = 1,
		        .descriptorType = vk::DescriptorType::eStorageBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		    vk::DescriptorSetLayoutBinding{
		        .binding = 2,
		        .descriptorType = vk::DescriptorType::eStorageBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		    vk::DescriptorSetLayoutBinding{
		        .binding = 3,
		        .descriptorType = vk::DescriptorType::eStorageBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		};

		m_reducePartialsRoutine.emplace(
		    *m_device, reducePartialsStage, reducePartialsBindings, svk::MAX_FRAMES_IN_FLIGHT);
		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			const vk::DeviceSize uboOffset = m_uboFrameSize * i;
			const vk::DeviceSize partialsOffset = partialsFrameStride * i;
			m_reducePartialsRoutine->registerBuffers(
			    i,
			    {
			        svk::BufferBinding(*m_uboDeviceBuffer, 0, uboOffset, sizeof(UBO)),
			        svk::BufferBinding(*m_vertexBuffer, 1),
			        svk::BufferBinding(*m_vertexDataBuffer, 2),
			        svk::BufferBinding(*m_partialsDeviceBuffer, 3, partialsOffset, rawPartialsSliceBytes),
			    });
			m_reducePartialsRoutine->bakeCommands(i, vk::CommandBufferUsageFlags{}, groupCountX, groupCountY, 1);
		}

		const vk::raii::ShaderModule reduceFinalModule(m_device->device(), reduce_final::smci);
		const vk::PipelineShaderStageCreateInfo reduceFinalStage{
		    .stage = vk::ShaderStageFlagBits::eCompute,
		    .module = *reduceFinalModule,
		    .pName = "reduceFinal",
		};
		const std::vector<vk::DescriptorSetLayoutBinding> reduceFinalBindings = {
		    vk::DescriptorSetLayoutBinding{
		        .binding = 0,
		        .descriptorType = vk::DescriptorType::eUniformBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		    vk::DescriptorSetLayoutBinding{
		        .binding = 1,
		        .descriptorType = vk::DescriptorType::eStorageBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		    vk::DescriptorSetLayoutBinding{
		        .binding = 2,
		        .descriptorType = vk::DescriptorType::eStorageBuffer,
		        .descriptorCount = 1,
		        .stageFlags = vk::ShaderStageFlagBits::eCompute,
		    },
		};

		m_reduceFinalRoutine.emplace(*m_device, reduceFinalStage, reduceFinalBindings, svk::MAX_FRAMES_IN_FLIGHT);
		for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
		{
			const vk::DeviceSize uboOffset = m_uboFrameSize * i;
			const vk::DeviceSize partialsOffset = partialsFrameStride * i;
			const vk::DeviceSize resultOffset = m_resultFrameStride * i;
			m_reduceFinalRoutine->registerBuffers(
			    i,
			    {
			        svk::BufferBinding(*m_uboDeviceBuffer, 0, uboOffset, sizeof(UBO)),
			        svk::BufferBinding(*m_partialsDeviceBuffer, 1, partialsOffset, rawPartialsSliceBytes),
			        svk::BufferBinding(*m_resultDeviceBuffer, 2, resultOffset, sizeof(float)),
			    });
			m_reduceFinalRoutine->bakeCommands(i, vk::CommandBufferUsageFlags{}, 1U, 1U, 1U);
		}
	}

	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_transferRoutine->bakeCommands(kResultReadbackTransferCmdBase + i,
		                                vk::CommandBufferUsageFlags{},
		                                *m_resultDeviceBuffer,
		                                *m_resultStagingBuffer,
		                                static_cast<vk::DeviceSize>(i) * m_resultFrameStride,
		                                static_cast<vk::DeviceSize>(i) * sizeof(float),
		                                sizeof(float));
	}

	if constexpr (!Settings::headlessMode)
	{
		m_swapchain.emplace(*m_device, *m_window);
		m_renderRoutine.emplace(*m_device, *m_swapchain, svk::MAX_FRAMES_IN_FLIGHT);

		{
			const vk::raii::ShaderModule meshModule(m_device->device(), mesh::smci);
			const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
			    vk::PipelineShaderStageCreateInfo{
			        .stage = vk::ShaderStageFlagBits::eVertex,
			        .module = *meshModule,
			        .pName = "vertMain",
			    },
			    vk::PipelineShaderStageCreateInfo{
			        .stage = vk::ShaderStageFlagBits::eFragment,
			        .module = *meshModule,
			        .pName = "fragMain",
			    },
			};

			const vk::PipelineVertexInputStateCreateInfo vertexInput{
			    .vertexBindingDescriptionCount = static_cast<uint32_t>(kVertexCoordsBindingDescriptions.size()),
			    .pVertexBindingDescriptions = kVertexCoordsBindingDescriptions.data(),
			    .vertexAttributeDescriptionCount = static_cast<uint32_t>(kVertexCoordsAttributeDescriptions.size()),
			    .pVertexAttributeDescriptions = kVertexCoordsAttributeDescriptions.data(),
			};
			const std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings = {vk::DescriptorSetLayoutBinding{
			    .binding = 0,
			    .descriptorType = vk::DescriptorType::eUniformBuffer,
			    .descriptorCount = 1,
			    .stageFlags = vk::ShaderStageFlagBits::eVertex,
			}};
			auto& meshTask = m_renderRoutine->m_tasks.emplace_back(m_device->device(),
			                                                       shaderStages,
			                                                       vertexInput,
			                                                       vk::PrimitiveTopology::ePointList,
			                                                       vk::CullModeFlagBits::eNone,
			                                                       descriptorBindings,
			                                                       m_swapchain->getFormat(),
			                                                       m_renderRoutine->getDepthFormat(),
			                                                       svk::MAX_FRAMES_IN_FLIGHT);
			meshTask.registerBuffers({},
			                         {svk::BufferBinding(*m_vertexBuffer, 0)},
			                         std::nullopt,
			                         static_cast<uint32_t>(m_mesh.vertices.size()),
			                         1);
			for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
			{
				const svk::BufferBinding uboBinding(*m_uboDeviceBuffer, 0, m_uboFrameSize * i, sizeof(UBO));
				m_device->validateDescriptorBinding(uboBinding);
				meshTask.updateDescriptors(i, {uboBinding});
			}
		}

		{
			const vk::raii::ShaderModule satelliteModule(m_device->device(), satellite::smci);
			const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
			    vk::PipelineShaderStageCreateInfo{
			        .stage = vk::ShaderStageFlagBits::eVertex,
			        .module = *satelliteModule,
			        .pName = "vertMain",
			    },
			    vk::PipelineShaderStageCreateInfo{
			        .stage = vk::ShaderStageFlagBits::eFragment,
			        .module = *satelliteModule,
			        .pName = "fragMain",
			    },
			};

			const vk::PipelineVertexInputStateCreateInfo vertexInput{};
			const std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings = {vk::DescriptorSetLayoutBinding{
			    .binding = 0,
			    .descriptorType = vk::DescriptorType::eUniformBuffer,
			    .descriptorCount = 1,
			    .stageFlags = vk::ShaderStageFlagBits::eVertex,
			}};

			auto& satelliteTask = m_renderRoutine->m_tasks.emplace_back(m_device->device(),
			                                                            shaderStages,
			                                                            vertexInput,
			                                                            vk::PrimitiveTopology::eLineList,
			                                                            vk::CullModeFlagBits::eNone,
			                                                            descriptorBindings,
			                                                            m_swapchain->getFormat(),
			                                                            m_renderRoutine->getDepthFormat(),
			                                                            svk::MAX_FRAMES_IN_FLIGHT);
			satelliteTask.registerBuffers({}, {}, std::nullopt, 32, static_cast<uint32_t>(m_agents.size()));
			for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
			{
				const svk::BufferBinding uboBinding(*m_uboDeviceBuffer, 0, m_uboFrameSize * i, sizeof(UBO));
				m_device->validateDescriptorBinding(uboBinding);
				satelliteTask.updateDescriptors(i, {uboBinding});
			}
		}
	}
}

void EngineInstance::harvestSlot(std::size_t slotIndex)
{
	if (!m_hasPendingResult[slotIndex]) { return; }

	const std::span<const float> mappedResults(static_cast<const float*>(m_resultMap->get()),
	                                           svk::MAX_FRAMES_IN_FLIGHT);
	const double reducedResult = static_cast<double>(mappedResults[slotIndex]);
	m_J_T = reducedResult;
	m_J_av += reducedResult * m_pendingTimeStep[slotIndex];
	m_harvestedTime += m_pendingTimeStep[slotIndex];
	++m_harvestedStepCount;
	m_hasPendingResult[slotIndex] = false;
}

void EngineInstance::tick(float timeStep)
{
	if constexpr (!Settings::headlessMode)
	{
		m_window->pollEvents();
		m_window->updateFPS(std::string(Settings::appName));
	}

	const std::size_t frameIdx = static_cast<std::size_t>(m_currentFrame);
	const vk::Fence fence = *m_inFlightFences[frameIdx];
	if (m_device->device().waitForFences({fence}, vk::True, UINT64_MAX) != vk::Result::eSuccess)
	{
		throw std::runtime_error("Fence wait failed");
	}
	harvestSlot(frameIdx);
	m_device->device().resetFences({fence});

	updateUBO(m_currentFrame, timeStep);

	if constexpr (!Settings::headlessMode)
	{
		if (m_camera->isPaused())
		{
			m_renderRoutine->draw(m_currentFrame, fence, *m_uboUpdatedSemaphores[frameIdx]);
			m_currentFrame = svk::advanceFrame(m_currentFrame);
			return;
		}
	}

	m_computeRoutine->submitCommands(
	    m_currentFrame, *m_uboUpdatedSemaphores[frameIdx], *m_computeFinishedSemaphores[frameIdx]);

	m_reducePartialsRoutine->submitCommands(
	    m_currentFrame, *m_computeFinishedSemaphores[frameIdx], *m_reducePartialsFinishedSemaphores[frameIdx]);

	m_reduceFinalRoutine->submitCommands(
	    m_currentFrame, *m_reducePartialsFinishedSemaphores[frameIdx], *m_reduceFinishedSemaphores[frameIdx]);

	m_transferRoutine->submitCommands(kResultReadbackTransferCmdBase + m_currentFrame,
	                                  *m_reduceFinishedSemaphores[frameIdx],
	                                  Settings::headlessMode ? nullptr : *m_resultTransferredSemaphores[frameIdx],
	                                  Settings::headlessMode ? fence : nullptr);

	if constexpr (!Settings::headlessMode)
	{
		m_renderRoutine->draw(m_currentFrame, fence, *m_resultTransferredSemaphores[frameIdx]);
	}

	m_hasPendingResult[frameIdx] = true;
	m_pendingTimeStep[frameIdx] = static_cast<double>(timeStep);
	m_currentFrame = svk::advanceFrame(m_currentFrame);
}

void EngineInstance::drainPendingResults()
{
	// Wait in FIFO order starting from the oldest in-flight slot (m_currentFrame) without resetting fences,
	// so all fences remain signaled for any subsequent paused rendering or shutdown.
	for (uint32_t offset = 0; offset < svk::MAX_FRAMES_IN_FLIGHT; ++offset)
	{
		const std::size_t slotIndex = static_cast<std::size_t>((m_currentFrame + offset) % svk::MAX_FRAMES_IN_FLIGHT);
		if (!m_hasPendingResult[slotIndex]) { continue; }

		const vk::Fence fence = *m_inFlightFences[slotIndex];
		if (m_device->device().waitForFences({fence}, vk::True, UINT64_MAX) != vk::Result::eSuccess)
		{
			throw std::runtime_error("Fence wait failed");
		}
		harvestSlot(slotIndex);
	}
}

void EngineInstance::updateUBO(uint32_t currentFrame, float timeStep)
{
	if constexpr (!Settings::headlessMode) // NOLINT(bugprone-branch-clone)
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
		std::memcpy(m_ubo.agents, m_agents.data(), static_cast<size_t>(m_ubo.agentCount) * sizeof(AgentData));
	}

	std::memcpy(m_uboStagingPtrs[currentFrame], &m_ubo, sizeof(UBO));
	m_transferRoutine->submitCommands(currentFrame, nullptr, *m_uboUpdatedSemaphores[currentFrame]);
}

EngineInstance::~EngineInstance()
{
	if (m_device) { m_device->waitIdle(); }
}
