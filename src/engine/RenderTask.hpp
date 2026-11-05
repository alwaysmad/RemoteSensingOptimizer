// src/engine/RenderTask.hpp
#pragma once

#include <optional>
#include <vector>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Buffer.hpp"
#include "engine/GraphicsPipeline.hpp"

namespace svk 
{
// =========================================================================
//  "A Standing Order for the GPU"
//  Encapsulates the Pipeline, Descriptor Memory, and Binding logic.
// =========================================================================
class RenderTask 
{
public:
    // Takes ownership of the pipeline and allocates the required Descriptor Pools
    RenderTask(
        const vk::raii::Device& device,
        const std::vector<vk::PipelineShaderStageCreateInfo>& shaderStages,
        const vk::PipelineVertexInputStateCreateInfo& vertexInput,
        vk::PrimitiveTopology topology,
        vk::CullModeFlags cullMode,
        const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings,
        vk::Format colorFormat,
        vk::Format depthFormat);

    // Ironclad Constraints: Non-Copyable
    RenderTask(const RenderTask&) = delete;
    RenderTask& operator=(const RenderTask&) = delete;
    // Default Movable
    RenderTask(RenderTask&&) noexcept = default;
    RenderTask& operator=(RenderTask&&) noexcept = default;

    ~RenderTask() = default;

    // --- State Management ---
    bool m_active = false;

    // --- Buffer Registration (Perfect Forwarding) ---
    // MUST NO NOT BE EXECUTED DURING RENDERING
    // In-Flight Frame Synchronization!!!
    template <typename V, typename I>
    void registerBuffers(
        const std::vector<svk::BufferBinding>& descriptorBindings,
        V&& vertexBindings,
        I&& indexBinding,
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
            m_vertexOffsets.emplace_back(0); // Offset is explicitly cached
        }

        // 2. Save the index binding and draw counts
        m_indexBufferBinding = std::forward<I>(indexBinding);
        m_count = count;
        m_instanceCount = instanceCount;

        // 3. Delegate Descriptor logic to .cpp
        updateDescriptors(descriptorBindings);
    }

    // Executes the draw calls into the provided command buffer
    void recordCommands(const vk::raii::CommandBuffer& cmd) const;

private:

    void updateDescriptors(const std::vector<svk::BufferBinding>& descriptorBindings);
    const vk::raii::Device* m_device = nullptr; // Retained to update descriptor sets later
    
    // Contract and GPU Memory
    svk::GraphicsPipeline m_pipeline;
    vk::raii::DescriptorPool m_descriptorPool = nullptr;
    vk::raii::DescriptorSets m_descriptorSets = nullptr; // Vector-like RAII container

    // Binding State
    std::vector<vk::Buffer> m_vertexBuffers;
    std::vector<vk::DeviceSize> m_vertexOffsets;
    std::optional<svk::BufferBinding> m_indexBufferBinding;
    
    uint32_t m_count = 0;
    uint32_t m_instanceCount = 1;
};

} // namespace svk