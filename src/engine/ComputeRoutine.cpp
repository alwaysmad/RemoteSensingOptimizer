// src/engine/ComputeRoutine.cpp

#include "engine/ComputeRoutine.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Buffer.hpp"
#include "engine/Command.hpp"
#include "engine/Device.hpp"
#include "engine/Queue.hpp"
#include "engine/VulkanHelpers.hpp"

svk::ComputeRoutine::ComputeRoutine(const svk::Device& device,
                                    const vk::PipelineShaderStageCreateInfo& shaderStage,
                                    std::span<const vk::DescriptorSetLayoutBinding> descriptorBindings,
                                    uint32_t bufferCount)
    : m_device(&device), m_computeQueue(&device.computeQueue()),
      m_pipeline(device.device(), shaderStage, descriptorBindings), m_clearBuffers(bufferCount),
      m_command(
          device.createCommand(svk::Device::COMPUTE, bufferCount, vk::CommandPoolCreateFlagBits::eResetCommandBuffer))
{
	std::vector<vk::DescriptorPoolSize> scaledPoolSizes = svk::deduceDescriptorPoolSizes(descriptorBindings);
	for (vk::DescriptorPoolSize& poolSize : scaledPoolSizes) { poolSize.descriptorCount *= bufferCount; }

	// Per Vulkan spec §14.2.2 & vk.xml, poolSizeCount can be 0 when no descriptors are required.
	// maxSets must be > 0. Allocating an empty descriptor set layout consumes 0 descriptors.
	const vk::DescriptorPoolCreateInfo poolInfo{
	    .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
	    .maxSets = bufferCount,
	    .poolSizeCount = static_cast<uint32_t>(scaledPoolSizes.size()),
	    .pPoolSizes = scaledPoolSizes.data(),
	};
	m_descriptorPool = vk::raii::DescriptorPool(device.device(), poolInfo);

	const std::vector<vk::DescriptorSetLayout> layouts(bufferCount, *m_pipeline.getDescriptorSetLayout());
	const vk::DescriptorSetAllocateInfo allocInfo{
	    .descriptorPool = *m_descriptorPool,
	    .descriptorSetCount = bufferCount,
	    .pSetLayouts = layouts.data(),
	};
	m_descriptorSets = vk::raii::DescriptorSets(device.device(), allocInfo);
}

void svk::ComputeRoutine::registerBuffers(uint32_t setIndex,
                                          std::span<const svk::BufferBinding> descriptorBindings,
                                          std::span<const BufferClear> clearBuffers)
{
	if (setIndex < m_clearBuffers.size()) { m_clearBuffers[setIndex].assign(clearBuffers.begin(), clearBuffers.end()); }

	if (descriptorBindings.empty()) { return; }

	std::vector<vk::DescriptorBufferInfo> bufferInfos(descriptorBindings.size());
	std::vector<vk::WriteDescriptorSet> writes(descriptorBindings.size());

	for (size_t i = 0; i < descriptorBindings.size(); ++i)
	{
		const svk::BufferBinding& binding = descriptorBindings[i];
		m_device->validateDescriptorBinding(binding);

		bufferInfos[i] = vk::DescriptorBufferInfo{
		    .buffer = binding.buffer,
		    .offset = binding.offset,
		    .range = binding.range,
		};

		writes[i] = vk::WriteDescriptorSet{
		    .dstSet = *m_descriptorSets[setIndex],
		    .dstBinding = binding.binding,
		    .descriptorCount = 1,
		    .descriptorType = binding.deduceDescriptorType(),
		    .pBufferInfo = &bufferInfos[i],
		};
	}

	m_device->device().updateDescriptorSets(writes, nullptr);
}

void svk::ComputeRoutine::bakeCommands(uint32_t cmdIndex,
                                       vk::CommandBufferUsageFlags usage,
                                       uint32_t groupCountX,
                                       uint32_t groupCountY,
                                       uint32_t groupCountZ) const
{
	const vk::raii::CommandBuffer& cmd = m_command[cmdIndex];
	cmd.begin(vk::CommandBufferBeginInfo{.flags = usage});

	if (cmdIndex < m_clearBuffers.size() && !m_clearBuffers[cmdIndex].empty())
	{
		for (const BufferClear& cb : m_clearBuffers[cmdIndex])
		{
			cmd.fillBuffer(cb.buffer, cb.offset, cb.size, cb.data);
		}

		const vk::MemoryBarrier2 barrier{
		    .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		    .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		    .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
		    .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
		};
		cmd.pipelineBarrier2(vk::DependencyInfo{
		    .memoryBarrierCount = 1,
		    .pMemoryBarriers = &barrier,
		});
	}

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *m_pipeline.getPipeline());

	cmd.bindDescriptorSets(
	    vk::PipelineBindPoint::eCompute, *m_pipeline.getPipelineLayout(), 0, {*m_descriptorSets[cmdIndex]}, nullptr);

	cmd.dispatch(groupCountX, groupCountY, groupCountZ);
	cmd.end();
}
void svk::ComputeRoutine::submitCommands(uint32_t cmdIndex,
                                         vk::Semaphore waitSemaphore,
                                         vk::Semaphore signalSemaphore,
                                         vk::Fence fence) const
{
	const vk::raii::CommandBuffer& cmd = m_command[cmdIndex];

	const vk::CommandBufferSubmitInfo cmdInfo{
	    .commandBuffer = *cmd,
	};

	const vk::SemaphoreSubmitInfo waitInfo{
	    .semaphore = waitSemaphore,
	    .stageMask = vk::PipelineStageFlagBits2::eComputeShader,
	};

	const vk::SemaphoreSubmitInfo signalInfo{
	    .semaphore = signalSemaphore,
	    .stageMask = vk::PipelineStageFlagBits2::eComputeShader,
	};

	const vk::SubmitInfo2 submitInfo{
	    .waitSemaphoreInfoCount = static_cast<bool>(waitSemaphore) ? 1U : 0U,
	    .pWaitSemaphoreInfos = static_cast<bool>(waitSemaphore) ? &waitInfo : nullptr,
	    .commandBufferInfoCount = 1U,
	    .pCommandBufferInfos = &cmdInfo,
	    .signalSemaphoreInfoCount = static_cast<bool>(signalSemaphore) ? 1U : 0U,
	    .pSignalSemaphoreInfos = static_cast<bool>(signalSemaphore) ? &signalInfo : nullptr,
	};

	m_computeQueue->submit(submitInfo, fence);
}

svk::ComputeRoutine::ComputeRoutine(const svk::Device& device,
                                    const vk::PipelineShaderStageCreateInfo& shaderStage,
                                    std::initializer_list<vk::DescriptorSetLayoutBinding> descriptorBindings,
                                    uint32_t bufferCount)
    : ComputeRoutine(
          device,
          shaderStage,
          std::span<const vk::DescriptorSetLayoutBinding>(descriptorBindings.begin(), descriptorBindings.size()),
          bufferCount)
{}

void svk::ComputeRoutine::registerBuffers(uint32_t setIndex,
                                          std::initializer_list<svk::BufferBinding> descriptorBindings,
                                          std::span<const BufferClear> clearBuffers)
{
	registerBuffers(setIndex,
	                std::span<const svk::BufferBinding>(descriptorBindings.begin(), descriptorBindings.size()),
	                clearBuffers);
}

void svk::ComputeRoutine::registerBuffers(uint32_t setIndex,
                                          std::span<const svk::BufferBinding> descriptorBindings,
                                          std::initializer_list<BufferClear> clearBuffers)
{
	registerBuffers(
	    setIndex, descriptorBindings, std::span<const BufferClear>(clearBuffers.begin(), clearBuffers.size()));
}

void svk::ComputeRoutine::registerBuffers(uint32_t setIndex,
                                          std::initializer_list<svk::BufferBinding> descriptorBindings,
                                          std::initializer_list<BufferClear> clearBuffers)
{
	registerBuffers(setIndex,
	                std::span<const svk::BufferBinding>(descriptorBindings.begin(), descriptorBindings.size()),
	                std::span<const BufferClear>(clearBuffers.begin(), clearBuffers.size()));
}