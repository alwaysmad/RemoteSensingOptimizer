// src/engine/ComputeRoutine.hpp

#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Device.hpp"
#include "engine/Queue.hpp"
#include "engine/Command.hpp"
#include "engine/ComputePipeline.hpp"
#include "engine/Buffer.hpp" // Assuming svk::BufferBinding lives here or in BufferStructs

namespace svk 
{

// =========================================================================
//  "The Compute Wizard"
//  Commands pure mathematical dispatch operations on the Compute Queue.
// =========================================================================
class ComputeRoutine 
{
public:
    // Ironclad Constraints
    ComputeRoutine(const ComputeRoutine&) = delete;
    ComputeRoutine& operator=(const ComputeRoutine&) = delete;
    ComputeRoutine(ComputeRoutine&&) = default;
    ComputeRoutine& operator=(ComputeRoutine&&) = default;

    ComputeRoutine(
        const svk::Device& device,
        const vk::PipelineShaderStageCreateInfo& shaderStage,
        const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings,
        uint32_t bufferCount
    );

    // Binds actual buffer memory to the specified Descriptor Set index
    void registerBuffers(uint32_t setIndex, const std::vector<svk::BufferBinding>& descriptorBindings);

    // Prepare the spell
    void bakeCommands(
        uint32_t cmdIndex, 
        vk::CommandBufferUsageFlags usage, 
        uint32_t groupCountX, 
        uint32_t groupCountY = 1, 
        uint32_t groupCountZ = 1
    ) const;

    // Cast the spell
    void submitCommands(
        uint32_t cmdIndex, 
        vk::Semaphore waitSemaphore = nullptr, 
        vk::Semaphore signalSemaphore = nullptr, 
        vk::Fence fence = nullptr
    ) const;

private:
    const svk::Device* m_device = nullptr;
    const svk::Queue* m_computeQueue = nullptr;

    // The Contract and Memory
    svk::ComputePipeline m_pipeline;
    vk::raii::DescriptorPool m_descriptorPool = nullptr;
    vk::raii::DescriptorSets m_descriptorSets = nullptr;
    
    // The Spellbook
    svk::Command m_command;
};

} // namespace svk