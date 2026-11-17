// src/engine/RenderTask.cpp
#include "engine/RenderTask.hpp"

#include "engine/VulkanHelpers.hpp"

namespace svk
{

RenderTask::RenderTask(const vk::raii::Device& device,
                       const std::vector<vk::PipelineShaderStageCreateInfo>& shaderStages,
                       const vk::PipelineVertexInputStateCreateInfo& vertexInput,
                       vk::PrimitiveTopology topology,
                       vk::CullModeFlags cullMode,
                       const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings,
                       vk::Format colorFormat,
                       vk::Format depthFormat)
    : m_device(&device),
      m_pipeline(device, shaderStages, vertexInput, topology, cullMode, descriptorBindings, colorFormat, depthFormat)
{
	const std::vector<vk::DescriptorPoolSize> poolSizes = svk::deduceDescriptorPoolSizes(descriptorBindings);

	// Per Vulkan spec §14.2.2 & vk.xml, poolSizeCount can be 0 when no descriptors are required.
	// maxSets must be > 0. Allocating an empty descriptor set layout consumes 0 descriptors.
	const vk::DescriptorPoolCreateInfo poolInfo{
	    .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
	    .maxSets = 1U,
	    .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
	    .pPoolSizes = poolSizes.data(),
	};
	m_descriptorPool = vk::raii::DescriptorPool(device, poolInfo);

	const vk::DescriptorSetLayout layout = *m_pipeline.getDescriptorSetLayout();
	const vk::DescriptorSetAllocateInfo allocInfo{
	    .descriptorPool = *m_descriptorPool,
	    .descriptorSetCount = 1U,
	    .pSetLayouts = &layout,
	};
	m_descriptorSets = vk::raii::DescriptorSets(device, allocInfo);
}

void RenderTask::updateDescriptors(const std::vector<svk::BufferBinding>& descriptorBindings)
{
	if (descriptorBindings.empty()) { return; }

	std::vector<vk::DescriptorBufferInfo> bufferInfos(descriptorBindings.size());
	std::vector<vk::WriteDescriptorSet> writes(descriptorBindings.size());

	for (size_t i = 0; i < descriptorBindings.size(); ++i)
	{
		bufferInfos[i] = vk::DescriptorBufferInfo{
		    .buffer = descriptorBindings[i].buffer,
		    .offset = 0,
		    .range = vk::WholeSize,
		};

		writes[i] = vk::WriteDescriptorSet{
		    .dstSet = *m_descriptorSets[0],
		    .dstBinding = descriptorBindings[i].binding,
		    .dstArrayElement = 0,
		    .descriptorCount = 1U,
		    .descriptorType = descriptorBindings[i].deduceDescriptorType(),
		    .pBufferInfo = &bufferInfos[i],
		};
	}

	m_device->updateDescriptorSets(writes, nullptr);
}

void RenderTask::recordCommands(const vk::raii::CommandBuffer& cmd) const
{
	if (!m_active) { return; }

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline.getPipeline());

	if (!m_vertexBuffers.empty()) { cmd.bindVertexBuffers(0, m_vertexBuffers, m_vertexOffsets); }

	if (m_indexBufferBinding.has_value())
	{
		cmd.bindIndexBuffer(m_indexBufferBinding->buffer, 0, vk::IndexType::eUint32);
	}

	cmd.bindDescriptorSets(
	    vk::PipelineBindPoint::eGraphics, *m_pipeline.getPipelineLayout(), 0, {*m_descriptorSets[0]}, nullptr);

	if (m_indexBufferBinding.has_value()) { cmd.drawIndexed(m_count, m_instanceCount, 0, 0, 0); }
	else
	{
		cmd.draw(m_count, m_instanceCount, 0, 0);
	}
}

} // namespace svk
