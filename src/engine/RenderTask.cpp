#include "engine/RenderTask.hpp"

#include "engine/VulkanHelpers.hpp"

namespace svk
{

RenderTask::RenderTask(
    const svk::Device& device,
    const std::vector<vk::PipelineShaderStageCreateInfo>& shaderStages,
    const vk::PipelineVertexInputStateCreateInfo& vertexInput,
    vk::PrimitiveTopology topology,
    vk::CullModeFlags cullMode,
    const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings,
    vk::Format colorFormat,
    vk::Format depthFormat)
    : m_device(&device),
      m_pipeline(
            device.device(),
            shaderStages,
            vertexInput,
            topology,
            cullMode,
            descriptorBindings,
            colorFormat,
            depthFormat)
{
    std::vector<vk::DescriptorPoolSize> poolSizes = svk::deduceDescriptorPoolSizes(descriptorBindings);
    if (poolSizes.empty())
    {
        poolSizes.push_back(vk::DescriptorPoolSize {
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
        });
    }

    vk::DescriptorPoolCreateInfo poolInfo {
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    m_descriptorPool = vk::raii::DescriptorPool(device.device(), poolInfo);

    const vk::DescriptorSetLayout layout = *m_pipeline.getDescriptorSetLayout();
    const vk::DescriptorSetAllocateInfo allocInfo {
        .descriptorPool = *m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    m_descriptorSets = vk::raii::DescriptorSets(device.device(), allocInfo);
}

void RenderTask::updateDescriptors(const std::vector<svk::BufferBinding>& descriptorBindings)
{
    if (descriptorBindings.empty())
        { return; }

    std::vector<vk::DescriptorBufferInfo> bufferInfos(descriptorBindings.size());
    std::vector<vk::WriteDescriptorSet> writes(descriptorBindings.size());

    for (size_t i = 0; i < descriptorBindings.size(); ++i)
    {
        bufferInfos[i] = vk::DescriptorBufferInfo {
            .buffer = descriptorBindings[i].buffer,
            .offset = 0,
            .range = vk::WholeSize,
        };

        writes[i] = vk::WriteDescriptorSet {
            .dstSet = *m_descriptorSets[0],
            .dstBinding = descriptorBindings[i].binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = descriptorBindings[i].deduceDescriptorType(),
            .pBufferInfo = &bufferInfos[i],
        };
    }

    m_device->device().updateDescriptorSets(writes, nullptr);
}

void RenderTask::recordCommands(const vk::raii::CommandBuffer& cmd) const
{
    if (!m_active)
        { return; }

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline.getPipeline());

    if (!m_vertexBufferBindings.empty())
    {
        std::vector<vk::Buffer> vertexBuffers;
        std::vector<vk::DeviceSize> offsets;
        vertexBuffers.reserve(m_vertexBufferBindings.size());
        offsets.reserve(m_vertexBufferBindings.size());

        for (const svk::BufferBinding& binding : m_vertexBufferBindings)
        {
            vertexBuffers.push_back(binding.buffer);
            offsets.push_back(0);
        }

        cmd.bindVertexBuffers(0, vertexBuffers, offsets);
    }

    if (m_indexBufferBinding.has_value())
        { cmd.bindIndexBuffer(m_indexBufferBinding->buffer, 0, vk::IndexType::eUint32); }

    if (!m_descriptorSets.empty())
    {
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *m_pipeline.getPipelineLayout(),
            0,
            { *m_descriptorSets[0] },
            nullptr);
    }

    if (m_indexBufferBinding.has_value())
        { cmd.drawIndexed(m_count, m_instanceCount, 0, 0, 0); }
    else
        { cmd.draw(m_count, m_instanceCount, 0, 0); }
}

} // namespace svk
