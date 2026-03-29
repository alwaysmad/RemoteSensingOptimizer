#include "engine/ComputeRoutine.hpp"

#include "engine/VulkanHelpers.hpp"

namespace svk
{

ComputeRoutine::ComputeRoutine(
    const svk::Device& device,
    const vk::PipelineShaderStageCreateInfo& shaderStage,
    const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings,
    uint32_t bufferCount)
    : m_device(&device),
      m_computeQueue(&device.computeQueue()),
      m_pipeline(device.device(), shaderStage, descriptorBindings),
      m_command(device.createCommand(svk::Device::COMPUTE, bufferCount, vk::CommandPoolCreateFlagBits::eResetCommandBuffer))
{
    std::vector<vk::DescriptorPoolSize> poolSizes = svk::deduceDescriptorPoolSizes(descriptorBindings);
    if (poolSizes.empty())
    {
        poolSizes.emplace_back(vk::DescriptorPoolSize {
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
        });
    }

    for (vk::DescriptorPoolSize& poolSize : poolSizes)
        { poolSize.descriptorCount *= bufferCount; }

    const vk::DescriptorPoolCreateInfo poolInfo {
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = bufferCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    m_descriptorPool = vk::raii::DescriptorPool(device.device(), poolInfo);

    std::vector<vk::DescriptorSetLayout> layouts(bufferCount, *m_pipeline.getDescriptorSetLayout());
    const vk::DescriptorSetAllocateInfo allocInfo {
        .descriptorPool = *m_descriptorPool,
        .descriptorSetCount = bufferCount,
        .pSetLayouts = layouts.data(),
    };
    m_descriptorSets = vk::raii::DescriptorSets(device.device(), allocInfo);
}

void ComputeRoutine::updateDescriptors(uint32_t setIndex, const std::vector<svk::BufferBinding>& descriptorBindings)
{
    if (descriptorBindings.empty())
        { return; }

    std::vector<vk::DescriptorBufferInfo> bufferInfos(descriptorBindings.size());
    std::vector<vk::WriteDescriptorSet> writes(descriptorBindings.size());

    for (size_t i = 0; i < descriptorBindings.size(); ++i)
    {
        const svk::BufferBinding& binding = descriptorBindings[i];

        bufferInfos[i] = vk::DescriptorBufferInfo {
            .buffer = binding.buffer,
            .offset = 0,
            .range = vk::WholeSize,
        };

        writes[i] = vk::WriteDescriptorSet {
            .dstSet = *m_descriptorSets[setIndex],
            .dstBinding = binding.binding,
            .descriptorCount = 1,
            .descriptorType = binding.deduceDescriptorType(),
            .pBufferInfo = &bufferInfos[i],
        };
    }

    m_device->device().updateDescriptorSets(writes, nullptr);
}

void ComputeRoutine::bakeCommands(
    uint32_t cmdIndex,
    vk::CommandBufferUsageFlags usage,
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ) const
{
    const vk::raii::CommandBuffer& cmd = m_command[cmdIndex];
    cmd.begin(vk::CommandBufferBeginInfo {.flags = usage});

    if (m_clearBuffer)
    {
        cmd.fillBuffer(m_clearBuffer->buffer, 0, VK_WHOLE_SIZE, 0);

        const vk::MemoryBarrier2 barrier {
            .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
        };
        cmd.pipelineBarrier2(vk::DependencyInfo {
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &barrier,
        });
    }

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *m_pipeline.getPipeline());

    if (!m_descriptorSets.empty())
    {
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            *m_pipeline.getPipelineLayout(),
            0,
            {*m_descriptorSets[cmdIndex]},
            nullptr);
    }

    cmd.dispatch(groupCountX, groupCountY, groupCountZ);
    cmd.end();
}

void ComputeRoutine::submitCommands(
    uint32_t cmdIndex,
    vk::Semaphore waitSemaphore,
    vk::Semaphore signalSemaphore,
    vk::Fence fence) const
{
    const vk::raii::CommandBuffer& cmd = m_command[cmdIndex];

    const vk::CommandBufferSubmitInfo cmdInfo {
        .commandBuffer = *cmd,
    };

    const vk::SemaphoreSubmitInfo waitInfo {
        .semaphore = waitSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eComputeShader,
    };

    const vk::SemaphoreSubmitInfo signalInfo {
        .semaphore = signalSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eComputeShader,
    };

    const vk::SubmitInfo2 submitInfo {
        .waitSemaphoreInfoCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphoreInfos = waitSemaphore ? &waitInfo : nullptr,
        .commandBufferInfoCount = 1u,
        .pCommandBufferInfos = &cmdInfo,
        .signalSemaphoreInfoCount = signalSemaphore ? 1u : 0u,
        .pSignalSemaphoreInfos = signalSemaphore ? &signalInfo : nullptr,
    };

    m_computeQueue->submit(submitInfo, fence);
}

} // namespace svk