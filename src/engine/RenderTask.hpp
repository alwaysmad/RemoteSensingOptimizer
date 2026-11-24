// src/engine/RenderTask.hpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Buffer.hpp"
#include "engine/GraphicsPipeline.hpp"
#include "engine/VulkanHelpers.hpp"

namespace svk
{
// =========================================================================
//  "A Standing Order for the GPU"
//  Encapsulates the Pipeline, Descriptor Memory, and Binding logic.
// =========================================================================
class RenderTask
{
private:
	const vk::raii::Device* m_device = nullptr; // Retained to update descriptor sets later

	// Contract and GPU Memory
	svk::GraphicsPipeline m_pipeline;
	vk::raii::DescriptorPool m_descriptorPool = nullptr;
	vk::raii::DescriptorSets m_descriptorSets = nullptr; // Vector-like RAII container

	// Binding State
	std::vector<vk::Buffer> m_vertexBuffers;
	std::vector<vk::DeviceSize> m_vertexOffsets;
	std::optional<svk::BufferBinding> m_indexBufferBinding;

	uint32_t m_vertexCount = 0;
	uint32_t m_instanceCount = 1;

public:
	// Takes ownership of the pipeline and allocates the required Descriptor Pools
	RenderTask(const vk::raii::Device& device,
	           std::span<const vk::PipelineShaderStageCreateInfo> shaderStages,
	           const vk::PipelineVertexInputStateCreateInfo& vertexInput,
	           vk::PrimitiveTopology topology,
	           vk::CullModeFlags cullMode,
	           std::span<const vk::DescriptorSetLayoutBinding> descriptorBindings,
	           vk::Format colorFormat,
	           vk::Format depthFormat,
	           uint32_t descriptorSetCount = 1U)
	    : m_device(&device),
	      m_pipeline(
	          device, shaderStages, vertexInput, topology, cullMode, descriptorBindings, colorFormat, depthFormat)
	{
		if (descriptorSetCount == 0U) { throw std::invalid_argument("RenderTask descriptorSetCount must be >= 1"); }

		std::vector<vk::DescriptorPoolSize> scaledPoolSizes = svk::deduceDescriptorPoolSizes(descriptorBindings);
		for (vk::DescriptorPoolSize& poolSize : scaledPoolSizes) { poolSize.descriptorCount *= descriptorSetCount; }

		// Per Vulkan spec §14.2.2 & vk.xml, poolSizeCount can be 0 when no descriptors are required.
		// maxSets must be > 0. Allocating an empty descriptor set layout consumes 0 descriptors.
		const vk::DescriptorPoolCreateInfo poolInfo{
		    .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		    .maxSets = descriptorSetCount,
		    .poolSizeCount = static_cast<uint32_t>(scaledPoolSizes.size()),
		    .pPoolSizes = scaledPoolSizes.data(),
		};
		m_descriptorPool = vk::raii::DescriptorPool(device, poolInfo);

		const std::vector<vk::DescriptorSetLayout> layouts(descriptorSetCount, *m_pipeline.getDescriptorSetLayout());
		const vk::DescriptorSetAllocateInfo allocInfo{
		    .descriptorPool = *m_descriptorPool,
		    .descriptorSetCount = descriptorSetCount,
		    .pSetLayouts = layouts.data(),
		};
		m_descriptorSets = vk::raii::DescriptorSets(device, allocInfo);
	}

	RenderTask(const vk::raii::Device& device,
	           std::initializer_list<vk::PipelineShaderStageCreateInfo> shaderStages,
	           const vk::PipelineVertexInputStateCreateInfo& vertexInput,
	           vk::PrimitiveTopology topology,
	           vk::CullModeFlags cullMode,
	           std::span<const vk::DescriptorSetLayoutBinding> descriptorBindings,
	           vk::Format colorFormat,
	           vk::Format depthFormat,
	           uint32_t descriptorSetCount = 1U)
	    : RenderTask(device,
	                 std::span<const vk::PipelineShaderStageCreateInfo>(shaderStages.begin(), shaderStages.size()),
	                 vertexInput,
	                 topology,
	                 cullMode,
	                 descriptorBindings,
	                 colorFormat,
	                 depthFormat,
	                 descriptorSetCount)
	{}

	RenderTask(const vk::raii::Device& device,
	           std::span<const vk::PipelineShaderStageCreateInfo> shaderStages,
	           const vk::PipelineVertexInputStateCreateInfo& vertexInput,
	           vk::PrimitiveTopology topology,
	           vk::CullModeFlags cullMode,
	           std::initializer_list<vk::DescriptorSetLayoutBinding> descriptorBindings,
	           vk::Format colorFormat,
	           vk::Format depthFormat,
	           uint32_t descriptorSetCount = 1U)
	    : RenderTask(
	          device,
	          shaderStages,
	          vertexInput,
	          topology,
	          cullMode,
	          std::span<const vk::DescriptorSetLayoutBinding>(descriptorBindings.begin(), descriptorBindings.size()),
	          colorFormat,
	          depthFormat,
	          descriptorSetCount)
	{}

	RenderTask(const vk::raii::Device& device,
	           std::initializer_list<vk::PipelineShaderStageCreateInfo> shaderStages,
	           const vk::PipelineVertexInputStateCreateInfo& vertexInput,
	           vk::PrimitiveTopology topology,
	           vk::CullModeFlags cullMode,
	           std::initializer_list<vk::DescriptorSetLayoutBinding> descriptorBindings,
	           vk::Format colorFormat,
	           vk::Format depthFormat,
	           uint32_t descriptorSetCount = 1U)
	    : RenderTask(
	          device,
	          std::span<const vk::PipelineShaderStageCreateInfo>(shaderStages.begin(), shaderStages.size()),
	          vertexInput,
	          topology,
	          cullMode,
	          std::span<const vk::DescriptorSetLayoutBinding>(descriptorBindings.begin(), descriptorBindings.size()),
	          colorFormat,
	          depthFormat,
	          descriptorSetCount)
	{}

	// Ironclad Constraints: Non-Copyable
	RenderTask(const RenderTask&) = delete;
	RenderTask& operator=(const RenderTask&) = delete;
	// Default Movable
	RenderTask(RenderTask&&) noexcept = default;
	RenderTask& operator=(RenderTask&&) noexcept = default;

	~RenderTask() = default;

	void updateDescriptors(uint32_t setIndex, std::span<const svk::BufferBinding> descriptorBindings)
	{
		if (static_cast<std::size_t>(setIndex) >= m_descriptorSets.size())
		{
			throw std::out_of_range("RenderTask descriptor set index out of range");
		}

		if (descriptorBindings.empty()) { return; }

		std::vector<vk::DescriptorBufferInfo> bufferInfos(descriptorBindings.size());
		std::vector<vk::WriteDescriptorSet> writes(descriptorBindings.size());

		for (std::size_t i = 0; i < descriptorBindings.size(); ++i)
		{
			bufferInfos[i] = vk::DescriptorBufferInfo{
			    .buffer = descriptorBindings[i].buffer,
			    .offset = descriptorBindings[i].offset,
			    .range = descriptorBindings[i].range,
			};

			writes[i] = vk::WriteDescriptorSet{
			    .dstSet = *m_descriptorSets[static_cast<std::size_t>(setIndex)],
			    .dstBinding = descriptorBindings[i].binding,
			    .dstArrayElement = 0U,
			    .descriptorCount = 1U,
			    .descriptorType = descriptorBindings[i].deduceDescriptorType(),
			    .pBufferInfo = &bufferInfos[i],
			};
		}

		m_device->updateDescriptorSets(writes, nullptr);
	}

	void updateDescriptors(uint32_t setIndex, std::initializer_list<svk::BufferBinding> descriptorBindings)
	{
		updateDescriptors(setIndex,
		                  std::span<const svk::BufferBinding>(descriptorBindings.begin(), descriptorBindings.size()));
	}

	// --- Buffer Registration ---
	// MUST NOT BE EXECUTED DURING RENDERING
	// In-Flight Frame Synchronization
	void registerBuffers(std::span<const svk::BufferBinding> descriptorBindings,
	                     std::span<const svk::BufferBinding> vertexBindings,
	                     std::optional<svk::BufferBinding> indexBinding,
	                     uint32_t count,
	                     uint32_t instanceCount = 1)
	{
		// 1. Unpack and cache the Vulkan handles and offsets ONCE.
		m_vertexBuffers.clear();
		m_vertexOffsets.clear();
		m_vertexBuffers.reserve(vertexBindings.size());
		m_vertexOffsets.reserve(vertexBindings.size());

		for (const auto& binding : vertexBindings)
		{
			m_vertexBuffers.emplace_back(binding.buffer);
			m_vertexOffsets.emplace_back(binding.offset); // Offset is explicitly cached
		}

		// 2. Save the index binding and draw counts
		m_indexBufferBinding = indexBinding;
		m_vertexCount = count;
		m_instanceCount = instanceCount;

		// 3. Delegate Descriptor logic to updateDescriptors
		updateDescriptors(0U, descriptorBindings);
	}

	void registerBuffers(std::initializer_list<svk::BufferBinding> descriptorBindings,
	                     std::span<const svk::BufferBinding> vertexBindings,
	                     std::optional<svk::BufferBinding> indexBinding,
	                     uint32_t count,
	                     uint32_t instanceCount = 1)
	{
		registerBuffers(std::span<const svk::BufferBinding>(descriptorBindings.begin(), descriptorBindings.size()),
		                vertexBindings,
		                indexBinding,
		                count,
		                instanceCount);
	}

	void registerBuffers(std::span<const svk::BufferBinding> descriptorBindings,
	                     std::initializer_list<svk::BufferBinding> vertexBindings,
	                     std::optional<svk::BufferBinding> indexBinding,
	                     uint32_t count,
	                     uint32_t instanceCount = 1)
	{
		registerBuffers(descriptorBindings,
		                std::span<const svk::BufferBinding>(vertexBindings.begin(), vertexBindings.size()),
		                indexBinding,
		                count,
		                instanceCount);
	}

	void registerBuffers(std::initializer_list<svk::BufferBinding> descriptorBindings,
	                     std::initializer_list<svk::BufferBinding> vertexBindings,
	                     std::optional<svk::BufferBinding> indexBinding,
	                     uint32_t count,
	                     uint32_t instanceCount = 1)
	{
		registerBuffers(std::span<const svk::BufferBinding>(descriptorBindings.begin(), descriptorBindings.size()),
		                std::span<const svk::BufferBinding>(vertexBindings.begin(), vertexBindings.size()),
		                indexBinding,
		                count,
		                instanceCount);
	}

	// Executes the draw calls into the provided command buffer
	void recordCommands(const vk::raii::CommandBuffer& cmd, uint32_t setIndex = 0U) const
	{
		const std::size_t activeSetIndex = (m_descriptorSets.size() > 1U) ? static_cast<std::size_t>(setIndex) : 0U;
		if (activeSetIndex >= m_descriptorSets.size())
		{
			throw std::out_of_range("RenderTask descriptor set index out of range");
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline.getPipeline());

		if (!m_vertexBuffers.empty()) { cmd.bindVertexBuffers(0, m_vertexBuffers, m_vertexOffsets); }

		if (m_indexBufferBinding.has_value())
		{
			cmd.bindIndexBuffer(m_indexBufferBinding->buffer, m_indexBufferBinding->offset, vk::IndexType::eUint32);
		}

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
		                       *m_pipeline.getPipelineLayout(),
		                       0,
		                       {*m_descriptorSets[activeSetIndex]},
		                       nullptr);

		if (m_indexBufferBinding.has_value()) { cmd.drawIndexed(m_vertexCount, m_instanceCount, 0, 0, 0); }
		else
		{
			cmd.draw(m_vertexCount, m_instanceCount, 0, 0);
		}
	}
};

} // namespace svk
