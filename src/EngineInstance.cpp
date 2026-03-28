// src/EngineInstance.cpp

#include <cstring>
#include <string> // std::string

#include <glm/glm.hpp>

#include "EngineInstance.hpp"

#include "triangle.hpp"

EngineInstance::EngineInstance(const Settings& settings, const svk::Logger& logger, const Mesh& mesh)
	: m_settings(settings),
	  m_logger(logger),
	  m_mesh(mesh),
	  m_windowContext(),
	  m_instance(std::string(Settings::appName), m_windowContext.getRequiredInstanceExtensions(), logger),
	  m_window(m_windowContext, m_instance.getInstance(), m_settings.windowWidth, m_settings.windowHeight, std::string(Settings::appName)),
	  m_camera(m_window.getGLFWwindow(), vk::Extent2D{ m_settings.windowWidth, m_settings.windowHeight }),
	  m_device(m_instance, m_window.getSurface(), m_settings.deviceName, logger),
	  m_swapchain(m_device, m_window),
	  m_renderRoutine(m_device, m_swapchain, svk::MAX_FRAMES_IN_FLIGHT),
	  m_transferRoutine(m_device, svk::MAX_FRAMES_IN_FLIGHT)
{
	m_uboUpdatedSemaphores.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	m_inFlightFences.reserve(svk::MAX_FRAMES_IN_FLIGHT);
	for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_uboUpdatedSemaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo {});
		m_inFlightFences.emplace_back(m_device.device(), vk::FenceCreateInfo {.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	const vk::DeviceSize vertexBytes = static_cast<vk::DeviceSize>(sizeof(VertexCoords) * m_mesh.vertices.size());
	const vk::DeviceSize indexBytes = static_cast<vk::DeviceSize>(sizeof(uint32_t) * m_mesh.indices.size());

	// Create final (device local) buffers
	m_vertexBuffer.emplace(m_device.createBuffer(
		vertexBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{svk::Device::TRANSFER, svk::Device::GRAPHICS}));

	m_indexBuffer.emplace(m_device.createBuffer(
		indexBytes,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		{svk::Device::TRANSFER, svk::Device::GRAPHICS}));
	
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

	{ // Upload index data via staging buffer
		auto indexStaging = m_device.createBuffer(
			indexBytes,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			{svk::Device::TRANSFER});

		{ // Map to copy
			auto map = indexStaging.map(0, indexBytes);
			std::memcpy(map.get(), m_mesh.indices.data(), static_cast<size_t>(indexBytes));
		}
		// copy from staging to device local
		m_transferRoutine.bakeCommands(
			0,
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
			indexStaging,
			*m_indexBuffer,
			0,
			0,
			indexBytes);
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
			{svk::Device::TRANSFER, svk::Device::GRAPHICS}));

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
				vk::CommandBufferUsageFlagBits::eSimultaneousUse,
				*m_uboStagingBuffer,
				*m_uboDeviceBuffer,
				offset,
				0,
				sizeof(UBO));
		}
	}

	{ // Create shadermodule and render task for the triangle
		const vk::raii::ShaderModule triangleModule(m_device.device(), triangle::smci);
		const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eVertex,
				.module = *triangleModule,
				.pName = "vertMain",
			},
			vk::PipelineShaderStageCreateInfo {
				.stage = vk::ShaderStageFlagBits::eFragment,
				.module = *triangleModule,
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
		auto& triangleTask = m_renderRoutine.m_tasks.emplace_back(
			m_device.device(),
			shaderStages,
			vertexInput,
			vk::PrimitiveTopology::eTriangleList,
			vk::CullModeFlagBits::eNone,
			descriptorBindings,
			m_swapchain.getFormat(),
			m_renderRoutine.getDepthFormat());
		triangleTask.m_active = true;
		triangleTask.registerBuffers(
			std::vector<svk::BufferBinding> { svk::BufferBinding(*m_uboDeviceBuffer, 0) },
			std::vector<svk::BufferBinding> { svk::BufferBinding(*m_vertexBuffer, 0) },
			std::optional<svk::BufferBinding> { svk::BufferBinding(*m_indexBuffer, 0) },
			static_cast<uint32_t>(m_mesh.indices.size()),
			1);
	}
}

void EngineInstance::tick()
{
	m_window.pollEvents();
	m_window.updateFPS(std::string(Settings::appName));

	const vk::Fence fence = *m_inFlightFences[m_currentFrame];
	if (m_device.device().waitForFences({fence}, vk::True, UINT64_MAX) != vk::Result::eSuccess)
		{ throw std::runtime_error("Fence wait failed"); }
	m_device.device().resetFences({fence});

	updateUBO(m_currentFrame);

	m_renderRoutine.draw(m_currentFrame, fence, *m_uboUpdatedSemaphores[m_currentFrame]);
	m_currentFrame = svk::advanceFrame(m_currentFrame);
}

bool EngineInstance::shouldClose() const { return m_window.shouldClose(); }

void EngineInstance::updateUBO(uint32_t currentFrame)
{
	m_ubo.viewProj = m_camera.getViewProj();
	// Copy UBO data to the correct precomputed frame pointer.
	std::memcpy(m_uboStagingPtrs[currentFrame], &m_ubo, sizeof(UBO));

	// Submit pre-baked transfer command with frame-specific signal semaphore.
	m_transferRoutine.submitCommands(
		currentFrame,
		nullptr,
		*m_uboUpdatedSemaphores[currentFrame]);
}

EngineInstance::~EngineInstance()
{
	m_device.waitIdle();
}
