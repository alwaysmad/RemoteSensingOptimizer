// src/engine/GraphicsPipeline.hpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace svk { class GraphicsPipeline; }
// =========================================================================
//  "A Contract with the GPU Shader-Demon"
//  RAII wrapper for the Graphics Pipeline, its Layout, and Descriptor bindings.
// =========================================================================
class svk::GraphicsPipeline 
{
private:
    // RAII handles MUST be declared in this exact order so they are destroyed in reverse:
    // Pipeline dies -> Layout dies -> Descriptor Set Layout dies.
    vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout m_pipelineLayout = nullptr;
    vk::raii::Pipeline m_pipeline = nullptr;

public:
    // Ironclad Constraints: Non-Copyable
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    // Default Movable: The RAII handles manage themselves safely
    GraphicsPipeline(GraphicsPipeline&&) noexcept = default;
    GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept = default;

    GraphicsPipeline(
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

    ~GraphicsPipeline() = default;

    // Accessors
    [[nodiscard]] const vk::raii::Pipeline& getPipeline() const { return m_pipeline; }
    [[nodiscard]] const vk::raii::PipelineLayout& getPipelineLayout() const { return m_pipelineLayout; }
    [[nodiscard]] const vk::raii::DescriptorSetLayout& getDescriptorSetLayout() const { return m_descriptorSetLayout; }
};