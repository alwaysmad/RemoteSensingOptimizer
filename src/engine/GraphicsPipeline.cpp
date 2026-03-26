#include <array>

#include "engine/GraphicsPipeline.hpp"

namespace svk
{

GraphicsPipeline::GraphicsPipeline(
    const vk::raii::Device& device,
    const std::vector<vk::PipelineShaderStageCreateInfo>& shaderStages,
    const vk::PipelineVertexInputStateCreateInfo& vertexInput,
    vk::PrimitiveTopology topology,
    vk::CullModeFlags cullMode,
    const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings,
    vk::Format colorFormat,
    vk::Format depthFormat)
{
    // DescriptorSetLayout and PipelineLayout (arg)
    const vk::DescriptorSetLayoutCreateInfo layoutInfo {
        .bindingCount = static_cast<uint32_t>(descriptorBindings.size()),
        .pBindings = descriptorBindings.data(),
    };
    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);

    const vk::DescriptorSetLayout descriptorSetLayout = *m_descriptorSetLayout;
    const vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    m_pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    // Input Assembly (arg)
    const vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
        .topology = topology,
        .primitiveRestartEnable = vk::False,
    };

    // Viewport/Scissor (Dynamic states) (hardcoded)
    constexpr vk::PipelineViewportStateCreateInfo viewportState {
        .viewportCount = 1, .pViewports = nullptr,
        .scissorCount = 1, .pScissors = nullptr,
    };

    // Rasterizer (arg)
    const vk::PipelineRasterizationStateCreateInfo rasterizer {
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = cullMode,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f,
    };

    // Multisampling (hardcoded)
    constexpr vk::PipelineMultisampleStateCreateInfo multisampling {
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False,
    };

    // Depth Stencil (hardcoded)
    constexpr vk::PipelineDepthStencilStateCreateInfo depthStencil {
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess,
    };

    // Color Blending (hardcoded)
    static constexpr vk::PipelineColorBlendAttachmentState colorBlendAttachment {
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA,
    };
    constexpr vk::PipelineColorBlendStateCreateInfo colorBlending {
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    // Dynamic States (hardcoded)
    static constexpr std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    constexpr vk::PipelineDynamicStateCreateInfo dynamicStateInfo {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    // Dynamic Rendering Info (Vulkan 1.3)
    const vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
    };

    const vk::GraphicsPipelineCreateInfo pipelineInfo {
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicStateInfo,
        .layout = *m_pipelineLayout,
    };

    m_pipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

} // namespace svk
