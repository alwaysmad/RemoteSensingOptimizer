// src/engine/TransferRoutine.hpp
#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "engine/Device.hpp"
#include "engine/Queue.hpp"
#include "engine/Command.hpp"
#include "engine/Buffer.hpp"

namespace svk { class TransferRoutine; }

// =========================================================================
//  
//  Manages the baking and submission of memory transfer operations
//  across the PCIe bus using a dedicated Transfer Queue.
// =========================================================================
class svk::TransferRoutine 
{
private:
    const svk::Device* m_device = nullptr;
    const svk::Queue* m_transferQueue = nullptr;
    svk::Command m_command;
    
public:
    // Ironclad Constraints
    TransferRoutine(const TransferRoutine&) = delete;
    TransferRoutine& operator=(const TransferRoutine&) = delete;
    TransferRoutine(TransferRoutine&&) noexcept = default;
    TransferRoutine& operator=(TransferRoutine&&) noexcept = default;
    ~TransferRoutine() = default;

    // We default the flags to eResetCommandBuffer for flexibility, as requested.
    TransferRoutine(const svk::Device& device, 
                    uint32_t bufferCount, 
                    vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        : m_device(&device),
          m_transferQueue(&device.transferQueue()),
          m_command(device.createCommand(svk::Device::TRANSFER, bufferCount, flags))
    {}

    // Bakes the memory copy commands into the specified command buffer
    void bakeCommands(
        uint32_t cmdIndex, 
        vk::CommandBufferUsageFlags usage, 
        const svk::Buffer& srcBuffer, 
        const svk::Buffer& dstBuffer, 
        vk::DeviceSize srcOffset = 0, 
        vk::DeviceSize dstOffset = 0, 
        vk::DeviceSize size = vk::WholeSize) const
    {
        const vk::raii::CommandBuffer& cmd = m_command[cmdIndex];
        cmd.begin(vk::CommandBufferBeginInfo {.flags = usage});

        const vk::BufferCopy copyRegion {
            .srcOffset = srcOffset,
            .dstOffset = dstOffset,
            .size = size,
        };
        cmd.copyBuffer(*srcBuffer.getBuffer(), *dstBuffer.getBuffer(), {copyRegion});

        cmd.end();
    }

    // Submits the baked command buffer to the Transfer Queue
    void submitCommands(
        uint32_t cmdIndex, 
        vk::Semaphore waitSemaphore = nullptr, 
        vk::Semaphore signalSemaphore = nullptr,
        vk::Fence fence = nullptr) const
    {
        const vk::raii::CommandBuffer& cmd = m_command[cmdIndex];

        const vk::CommandBufferSubmitInfo cmdInfo {
            .commandBuffer = *cmd,
        };

        const vk::SemaphoreSubmitInfo waitInfo {
            .semaphore = waitSemaphore,
            .stageMask = vk::PipelineStageFlagBits2::eAllTransfer,
        };

        const vk::SemaphoreSubmitInfo signalInfo {
            .semaphore = signalSemaphore,
            .stageMask = vk::PipelineStageFlagBits2::eAllTransfer,
        };

        const vk::SubmitInfo2 submitInfo {
            .waitSemaphoreInfoCount = static_cast<bool>(waitSemaphore) ? 1u : 0u,
            .pWaitSemaphoreInfos = static_cast<bool>(waitSemaphore) ? &waitInfo : nullptr,
            .commandBufferInfoCount = 1u,
            .pCommandBufferInfos = &cmdInfo,
            .signalSemaphoreInfoCount = static_cast<bool>(signalSemaphore) ? 1u : 0u,
            .pSignalSemaphoreInfos = static_cast<bool>(signalSemaphore) ? &signalInfo : nullptr,
        };

        m_transferQueue->submit(submitInfo, fence);
    }
};