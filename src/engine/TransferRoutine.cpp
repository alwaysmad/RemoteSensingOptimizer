#include "engine/TransferRoutine.hpp"

namespace svk
{

TransferRoutine::TransferRoutine(
    const svk::Device& device,
    uint32_t bufferCount,
    vk::CommandPoolCreateFlags flags)
    : m_device(&device),
      m_transferQueue(&device.transferQueue()),
      m_command(device.createCommand(svk::Device::TRANSFER, bufferCount, flags))
{}

void TransferRoutine::bakeCommands(
    uint32_t cmdIndex,
    vk::CommandBufferUsageFlags usage,
    const svk::Buffer& srcBuffer,
    const svk::Buffer& dstBuffer,
    vk::DeviceSize srcOffset,
    vk::DeviceSize dstOffset,
    vk::DeviceSize size) const
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

void TransferRoutine::submitCommands(
    uint32_t cmdIndex,
    vk::Semaphore waitSemaphore,
    vk::Semaphore signalSemaphore,
    vk::Fence fence) const
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
        .waitSemaphoreInfoCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphoreInfos = waitSemaphore ? &waitInfo : nullptr,
        .commandBufferInfoCount = 1u,
        .pCommandBufferInfos = &cmdInfo,
        .signalSemaphoreInfoCount = signalSemaphore ? 1u : 0u,
        .pSignalSemaphoreInfos = signalSemaphore ? &signalInfo : nullptr,
    };

    m_transferQueue->submit(submitInfo, fence);
}

} // namespace svk
