#include "ComputePipeline.hpp"
#include "VulkanDevice.hpp"
#include "PushConstants.hpp"
#include "solver.hpp" // Generated from shaders/solver.slang

ComputePipeline::ComputePipeline(const VulkanDevice& device)
{
	// 1. Descriptor Set Layout
	static constexpr std::array<vk::DescriptorSetLayoutBinding, 2> bindings = 
	{{
		// Binding 0: Satellites (UBO)
		{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eCompute
		},
		// Binding 1: Earth Vertices (SSBO)
		{
			.binding = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eCompute
		}
	}};

	constexpr vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};
	m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device.device(), layoutInfo);

	// 2. Push Constant Range
	static constexpr vk::PushConstantRange pushConstantRange {
		.stageFlags = vk::ShaderStageFlagBits::eCompute,
		.offset = 0,
		.size = sizeof(ComputePushConstants) 
	};

	// 3. Pipeline Layout
	const vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
		.setLayoutCount = 1,
		.pSetLayouts = &*m_descriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	m_pipelineLayout = vk::raii::PipelineLayout(device.device(), pipelineLayoutInfo);

	// 4. Load Shader
	const vk::raii::ShaderModule computeShader(device.device(), solver::smci);

	const vk::PipelineShaderStageCreateInfo shaderStage {
		.stage = vk::ShaderStageFlagBits::eCompute,
		.module = *computeShader,
		.pName = "computeMain" // Must match Slang entry point
	};

	// 5. Create Compute Pipeline
	const vk::ComputePipelineCreateInfo pipelineInfo {
		.stage = shaderStage,
		.layout = *m_pipelineLayout
	};

	m_pipeline = device.device().createComputePipeline(nullptr, pipelineInfo);

	LOG_DEBUG("Compute Pipeline created");
}

ComputePipeline::~ComputePipeline() { LOG_DEBUG("Compute Pipeline destroyed"); }
