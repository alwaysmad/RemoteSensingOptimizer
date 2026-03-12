#include "Computer.hpp"

#include "VulkanDevice.hpp"
#include "Satellite.hpp"
#include "Mesh.hpp"

Computer::Computer(const VulkanDevice& device) :
	m_device(device),
	// Use the Dedicated Compute Queue (faster on AMD/NVIDIA)
	m_command(device, device.getComputeQueueIndex()),
	m_pipeline(device)
{
	// 1. Create Pool
	static constexpr std::array<vk::DescriptorPoolSize, 2> poolSizes =
	{{
		{ vk::DescriptorType::eUniformBuffer, 1 },
		{ vk::DescriptorType::eStorageBuffer, 1 }
	}};

	constexpr vk::DescriptorPoolCreateInfo poolInfo {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = 1,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};
	m_descriptorPool = vk::raii::DescriptorPool(m_device.device(), poolInfo);

	// 2. Allocate Set (Empty for now)
	const vk::DescriptorSetAllocateInfo allocInfo {
		.descriptorPool = *m_descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &*m_pipeline.getDescriptorSetLayout()
	};
	m_descriptorSets = vk::raii::DescriptorSets(m_device.device(), allocInfo);

	LOG_DEBUG("Computer initialized");
}

Computer::~Computer() { LOG_DEBUG("Computer destroyed"); }

void Computer::registerResources(const Mesh& earthMesh, const SatelliteNetwork& satNet)
{
	// 1. Update stored counts for Push Constants
	m_pc.vertexCount = static_cast<uint32_t>(earthMesh.vertices.size());
	m_pc.satelliteCount = static_cast<uint32_t>(satNet.satellites.size());

	// 2. Update Descriptors
	// Binding 0: Satellites (UBO)
	const vk::DescriptorBufferInfo uboInfo {
		.buffer = *satNet.getBuffer(),
		.offset = 0,
		//.range = satNet.getFrameSize()
		.range = vk::WholeSize
	};

	// Binding 1: Earth (SSBO)
	const vk::DescriptorBufferInfo ssboInfo {
		.buffer = *earthMesh.getVertexBuffer(),
		.offset = 0,
		.range = vk::WholeSize
	};

	const std::array<vk::WriteDescriptorSet, 2> descriptorWrites =
	{{
		{
			.dstSet = *m_descriptorSets[0],
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &uboInfo
		},
		{
			.dstSet = *m_descriptorSets[0],
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &ssboInfo
		}
	}};

	m_device.device().updateDescriptorSets(descriptorWrites, nullptr);
	LOG_DEBUG("Computer resources registered");
}

void Computer::compute (
		uint32_t currentFrame,
		const glm::mat4& modelMatrix,
		float deltaTime,
		vk::Fence fence, 
		vk::Semaphore waitSemaphore, 
		vk::Semaphore signalSemaphore )
{
	// 1. Reset Fence (CPU Sync)
	// Only reset/use fence if one is actually provided (in Headless mode)
	if (fence) { m_device.device().resetFences({fence}); }

	// Update push constants
	m_pc.modelMatrix = modelMatrix;
	m_pc.deltaTime = deltaTime;

	// 2. Record Commands
	const auto& cmd = m_command.getBuffer(currentFrame);
	recordComputeCommands(cmd);

	// 3. Submit
	constexpr vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eComputeShader;
	
	const vk::SubmitInfo submitInfo {
		.waitSemaphoreCount = waitSemaphore ? 1u : 0u,
		.pWaitSemaphores = waitSemaphore ? &waitSemaphore : nullptr,
		.pWaitDstStageMask = waitSemaphore ? &waitStage : nullptr, // Wait at Compute Stage
		.commandBufferCount = 1,
		.pCommandBuffers = &*cmd,
		.signalSemaphoreCount = signalSemaphore ? 1u : 0u,
		.pSignalSemaphores = signalSemaphore ? &signalSemaphore : nullptr
	};

	// Submit to the compute queue
	m_device.computeQueue().submit(submitInfo, fence);
}

void Computer::recordComputeCommands(const vk::raii::CommandBuffer& cmd)
{
	cmd.reset();
	cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *m_pipeline.getPipeline());

	// Bind the set we updated in registerResources
	cmd.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			*m_pipeline.getLayout(),
			0,
			{*m_descriptorSets[0]},
			nullptr
	);

	// --- PUSH CONSTANTS ---
	cmd.pushConstants<ComputePushConstants> (
			*m_pipeline.getLayout(),
			vk::ShaderStageFlagBits::eCompute,
			0,
			m_pc
	);

	// Dispatch 1 thread per vertex
	uint32_t groupCount = (m_pc.vertexCount + (blockSize - 1)) >> shift;
	cmd.dispatch(groupCount, 1, 1);	

	cmd.end();
}
