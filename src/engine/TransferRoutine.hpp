// src/engine/TransferRoutine.hpp
#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "engine/Device.hpp"
#include "engine/Queue.hpp"
#include "engine/Command.hpp"
#include "engine/Buffer.hpp"

namespace svk 
{

// =========================================================================
//  
//  Manages the baking and submission of memory transfer operations
//  across the PCIe bus using a dedicated Transfer Queue.
// =========================================================================
class TransferRoutine 
{
public:
    // Ironclad Constraints
    TransferRoutine(const TransferRoutine&) = delete;
    TransferRoutine& operator=(const TransferRoutine&) = delete;
    TransferRoutine(TransferRoutine&&) = default;
    TransferRoutine& operator=(TransferRoutine&&) = default;

    // We default the flags to eResetCommandBuffer for flexibility, as requested.
    TransferRoutine(const svk::Device& device, 
                    uint32_t bufferCount, 
                    vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

    // Bakes the memory copy commands into the specified command buffer
    void bakeCommands(
        uint32_t cmdIndex, 
        vk::CommandBufferUsageFlags usage, 
        const svk::Buffer& srcBuffer, 
        const svk::Buffer& dstBuffer, 
        vk::DeviceSize srcOffset = 0, 
        vk::DeviceSize dstOffset = 0, 
        vk::DeviceSize size = vk::WholeSize) const;

    // Submits the baked command buffer to the Transfer Queue
    void submitCommands(
        uint32_t cmdIndex, 
        vk::Semaphore waitSemaphore = nullptr, 
        vk::Semaphore signalSemaphore = nullptr,
        vk::Fence fence = nullptr) const;

private:
    const svk::Device* m_device = nullptr;
    const svk::Queue* m_transferQueue = nullptr;
    svk::Command m_command;
};

} // namespace svk