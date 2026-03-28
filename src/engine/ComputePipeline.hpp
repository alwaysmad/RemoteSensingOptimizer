// src/engine/ComputePipeline.hpp
#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace svk 
{

// =========================================================================
//  "A Contract with the Compute Demon on the GPU"
//  RAII wrapper for the Compute Pipeline, its Layout, and Descriptor bindings.
// =========================================================================
class ComputePipeline 
{
public:
    // Ironclad Constraints: Non-Copyable
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    // Default Movable: The RAII handles manage themselves safely
    ComputePipeline(ComputePipeline&&) noexcept = default;
    ComputePipeline& operator=(ComputePipeline&&) noexcept = default;

    ComputePipeline(
        const vk::raii::Device& device,
        const vk::PipelineShaderStageCreateInfo& computeShader,
        const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings);

    ~ComputePipeline() = default;

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
