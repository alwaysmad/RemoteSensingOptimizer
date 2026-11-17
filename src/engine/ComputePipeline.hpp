// src/engine/ComputePipeline.hpp
#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace svk { class ComputePipeline; }

// =========================================================================
//  "A Contract with the Compute Demon on the GPU"
//  RAII wrapper for the Compute Pipeline, its Layout, and Descriptor bindings.
// =========================================================================
class svk::ComputePipeline
{
private:
	// RAII handles MUST be declared in this exact order so they are destroyed in reverse:
	// Pipeline dies -> Layout dies -> Descriptor Set Layout dies.
	vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout m_pipelineLayout = nullptr;
	vk::raii::Pipeline m_pipeline = nullptr;

public:
	// Ironclad Constraints: Non-Copyable
	ComputePipeline(const ComputePipeline&) = delete;
	ComputePipeline& operator=(const ComputePipeline&) = delete;

	// Default Movable: The RAII handles manage themselves safely
	ComputePipeline(ComputePipeline&&) noexcept = default;
	ComputePipeline& operator=(ComputePipeline&&) noexcept = default;

	ComputePipeline(const vk::raii::Device& device,
	                const vk::PipelineShaderStageCreateInfo& computeShader,
	                std::span<const vk::DescriptorSetLayoutBinding> descriptorBindings)
	{
		// Check if we need descripotor sets
		const bool hasDescriptors = !descriptorBindings.empty();

		if (hasDescriptors)
		{ // Creating an empty descriptor set layout is not valid
			const vk::DescriptorSetLayoutCreateInfo layoutInfo{
			    .bindingCount = static_cast<uint32_t>(descriptorBindings.size()),
			    .pBindings = descriptorBindings.data(),
			};
			m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
		}

		// PipelineLayout (no push constants; 0 or 1 descriptor set)
		const vk::DescriptorSetLayout descriptorSetLayout = hasDescriptors ? *m_descriptorSetLayout : nullptr;
		const vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		    .setLayoutCount = hasDescriptors ? 1U : 0U,
		    .pSetLayouts = hasDescriptors ? &descriptorSetLayout : nullptr,
		    .pushConstantRangeCount = 0U,
		    .pPushConstantRanges = nullptr,
		};
		m_pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		// Compute Pipeline
		const vk::ComputePipelineCreateInfo pipelineInfo{
		    .stage = computeShader,
		    .layout = *m_pipelineLayout,
		};

		m_pipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
	}

	ComputePipeline(const vk::raii::Device& device,
	                const vk::PipelineShaderStageCreateInfo& computeShader,
	                std::initializer_list<vk::DescriptorSetLayoutBinding> descriptorBindings)
	    : ComputePipeline(
	          device,
	          computeShader,
	          std::span<const vk::DescriptorSetLayoutBinding>(descriptorBindings.begin(), descriptorBindings.size()))
	{}

	~ComputePipeline() = default;

	// Accessors
	[[nodiscard]] const vk::raii::Pipeline& getPipeline() const { return m_pipeline; }
	[[nodiscard]] const vk::raii::PipelineLayout& getPipelineLayout() const { return m_pipelineLayout; }
	[[nodiscard]] const vk::raii::DescriptorSetLayout& getDescriptorSetLayout() const { return m_descriptorSetLayout; }
};