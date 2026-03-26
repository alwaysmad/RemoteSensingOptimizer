// src/engine/GraphicsPipeline.hpp
#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace svk 
{

// =========================================================================
//  "A Contract with the GPU Shader-Demon"
//  RAII wrapper for the Graphics Pipeline, its Layout, and Descriptor bindings.
// =========================================================================
class GraphicsPipeline 
{
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
        vk::Format depthFormat);

    ~GraphicsPipeline() = default;

    // Accessors
    [[nodiscard]] inline const vk::raii::Pipeline& getPipeline() const { return m_pipeline; }
    [[nodiscard]] inline const vk::raii::PipelineLayout& getPipelineLayout() const { return m_pipelineLayout; }
    [[nodiscard]] inline const vk::raii::DescriptorSetLayout& getDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    // RAII handles MUST be declared in this exact order so they are destroyed in reverse:
    // Pipeline dies -> Layout dies -> Descriptor Set Layout dies.
    vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout m_pipelineLayout = nullptr;
    vk::raii::Pipeline m_pipeline = nullptr;
};

} // namespace svk