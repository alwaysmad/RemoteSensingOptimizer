// src/GraphicsPipeline.cpp
#include "GraphicsPipeline.hpp"
#include "VulkanDevice.hpp"
#include "PushConstants.hpp"

// Subclasses include their specific data headers
#include "Mesh.hpp"
#include "Satellite.hpp"

// Generated Shaders
#include "mesh.hpp"      
#include "satellite.hpp"

// =================================================================================================
// BASE CLASS IMPLEMENTATION
// =================================================================================================
GraphicsPipeline::GraphicsPipeline(
		const VulkanDevice& device,
		const Config& config,
		vk::Format colorFormat,
		vk::Format depthFormat )
{
	// 1. Shader Stages
	const vk::raii::ShaderModule shaderModule(device.device(), config.shaderInfo);
	const vk::PipelineShaderStageCreateInfo shaderStages[] = {
		{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = shaderModule,
			.pName = "vertMain" // Entry point from Slang shader
		},
		{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = shaderModule,
			.pName = "fragMain" // Entry point from Slang shader
		}
	};
	// 2. Vertex Input (Directly from Config)
	const vk::PipelineVertexInputStateCreateInfo& vertexInputInfo = config.vertexInputState;
	// 3. Input assembly
	const vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
		.topology = config.topology,
		.primitiveRestartEnable = vk::False
	};	
	// 4. Viewport/Scissor (Dynamic)
	constexpr vk::PipelineViewportStateCreateInfo viewportState {
		.viewportCount = 1,
		.scissorCount = 1
	};
	// 5. Rasterizer
	const vk::PipelineRasterizationStateCreateInfo rasterizer {
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = config.cullMode,
		.frontFace = vk::FrontFace::eClockwise,
		.lineWidth = 1.0f
	};
	// 6. Multisampling
	constexpr vk::PipelineMultisampleStateCreateInfo multisampling {
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
	};
	// 7. Depth Stencil State
	constexpr vk::PipelineDepthStencilStateCreateInfo depthStencil {
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess, // Closer objects overwrite further ones
	};
	// 8. Color blending
	static constexpr vk::PipelineColorBlendAttachmentState colorBlendAttachment {
		.blendEnable = vk::False,
		.colorWriteMask
			= vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
	};
	constexpr vk::PipelineColorBlendStateCreateInfo colorBlending {
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};
	// 9. Dynamic States allow us to resize window without recreating pipeline
	static constexpr std::array<vk::DynamicState, 2> dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
	constexpr vk::PipelineDynamicStateCreateInfo dynamicStateInfo {
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};
	// 10. Pipeline Layout (Uniforms/Push Constants go here)
	const vk::DescriptorSetLayoutBinding binding0 {
		.binding = 0,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eVertex 
	};
	const vk::DescriptorSetLayoutCreateInfo dslInfo { 
		.bindingCount = 1,
		.pBindings = &binding0
	};
	m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device.device(), dslInfo);

	static constexpr vk::PushConstantRange pcRange {
		.stageFlags = vk::ShaderStageFlagBits::eVertex,
		.offset = 0,
		.size = sizeof(CameraPushConstants)
	};

	const vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
		.setLayoutCount = 1,
		.pSetLayouts = &*m_descriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pcRange
	};
	m_pipelineLayout = vk::raii::PipelineLayout(device.device(), pipelineLayoutInfo);
	// 11. Dynamic Rendering Info (Vulkan 1.3)
	const vk::PipelineRenderingCreateInfo pipelineRenderingInfo {
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &colorFormat,
		.depthAttachmentFormat = depthFormat
	};
	// 12. Create Pipeline
	const vk::GraphicsPipelineCreateInfo pipelineInfo {
		.pNext = &pipelineRenderingInfo, // 11
		.stageCount = 2,
		.pStages = shaderStages, // 1
		.pVertexInputState = &vertexInputInfo, // 2
		.pInputAssemblyState = &inputAssembly, // 3
		.pViewportState = &viewportState, // 4
		.pRasterizationState = &rasterizer, // 5
		.pMultisampleState = &multisampling, // 6
		.pDepthStencilState = &depthStencil, // 7
		.pColorBlendState = &colorBlending, // 8
		.pDynamicState = &dynamicStateInfo, // 9
		.layout = m_pipelineLayout, // 10
		.renderPass = nullptr, // Must be null for dynamic rendering
		.subpass = 0
	};

	m_pipeline = device.device().createGraphicsPipeline(nullptr, pipelineInfo);
	LOG_DEBUG("Graphics Pipeline created");
}

// =================================================================================================
// SUBCLASS: MESH (EARTH)
// =================================================================================================
MeshPipeline::MeshPipeline(const VulkanDevice& device, vk::Format cFmt, vk::Format dFmt)
	: GraphicsPipeline(device, 
		{
			.shaderInfo = mesh::smci,
			.vertexInputState = {
				.vertexBindingDescriptionCount = 1,
				.pVertexBindingDescriptions = Mesh::bindingDescriptions.data(),
				.vertexAttributeDescriptionCount = static_cast<uint32_t>(Mesh::attributeDescriptions.size()),
				.pVertexAttributeDescriptions = Mesh::attributeDescriptions.data()
			},
			.topology = vk::PrimitiveTopology::eTriangleList,
			.cullMode = vk::CullModeFlagBits::eBack
		}, cFmt, dFmt) { LOG_DEBUG("Mesh Pipeline Created"); }

MeshPipeline::~MeshPipeline() { LOG_DEBUG("Mesh Pipeline destroyed"); }

// =================================================================================================
// SUBCLASS: SATELLITE (WIREFRAME)
// =================================================================================================
SatellitePipeline::SatellitePipeline(const VulkanDevice& device, vk::Format cFmt, vk::Format dFmt)
	: GraphicsPipeline(device, 
		{
			.shaderInfo = satellite::smci,
			// Empty Vertex Input (Vertex Pulling)
			.vertexInputState = {
				.vertexBindingDescriptionCount = 0,
				.vertexAttributeDescriptionCount = 0
			},
			.topology = vk::PrimitiveTopology::eLineList,
			.cullMode = vk::CullModeFlagBits::eNone
		}, cFmt, dFmt) { LOG_DEBUG("Satellite Pipeline Created"); }

SatellitePipeline::~SatellitePipeline() { LOG_DEBUG("Satellite Pipeline destroyed"); }
