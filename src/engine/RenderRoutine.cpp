#include "engine/RenderRoutine.hpp"

namespace svk
{

RenderRoutine::RenderRoutine(
    const svk::Device& device,
    svk::Swapchain& swapchain,
    uint32_t bufferCount)
    : m_swapchain(&swapchain),
      m_graphicsQueue(&device.graphicsQueue()),
      m_depthResources(device, swapchain.getExtent(), bufferCount),
      m_command(device.createCommand(svk::Device::GRAPHICS, bufferCount, vk::CommandPoolCreateFlagBits::eResetCommandBuffer))
{
    // Create image-available semaphores (one per frame)
    for (uint32_t i = 0; i < bufferCount; ++i)
        { m_imageAvailableSemaphores.emplace_back(device.device(), vk::SemaphoreCreateInfo{}); }
}

void RenderRoutine::draw(uint32_t currentFrame, vk::Fence fence, vk::Semaphore waitSemaphore)
{
    // 0. Wait on fence and reset it
    // external sync logic belogs to the caller!!!
    // [[maybe_unused]] auto waitResult = m_device->device().waitForFences({fence}, vk::True, UINT64_MAX);
    // m_device->device().resetFences({fence});

    // 1. Minimization Check
    const vk::Extent2D extent = m_swapchain->getExtent();
    if (extent.width <= 1 || extent.height <= 1)
        { m_graphicsQueue->dummySubmit(fence, waitSemaphore); return; }

    // 2. Acquire Image
    const auto frameOpt = m_swapchain->acquireNextImage(*m_imageAvailableSemaphores[currentFrame]);
    if (!frameOpt.has_value())
    {
        m_graphicsQueue->dummySubmit(fence, waitSemaphore);
        m_swapchain->recreate();
        return;
    }
    const svk::SwapchainFrame frame = frameOpt.value();

    // 3. Depth Extent Check
    const auto& depthImage = m_depthResources[currentFrame];
    if (extent != m_depthResources.getExtent())
    {
        const std::array<vk::SemaphoreSubmitInfo, 2> waitInfos = {
            vk::SemaphoreSubmitInfo{
                .semaphore = *m_imageAvailableSemaphores[currentFrame],
                .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
            },
            vk::SemaphoreSubmitInfo{
                .semaphore = waitSemaphore,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands
            }
        };
        const vk::SubmitInfo2 dummyInfo{
            .waitSemaphoreInfoCount = waitSemaphore ? 2u : 1u,
            .pWaitSemaphoreInfos = waitInfos.data()
        };
        // Submit empty command, consume all semaphores, and signal the fence
        m_graphicsQueue->submit(dummyInfo, fence);
        m_graphicsQueue->waitIdle();
        m_depthResources.recreate(extent);
        return;
    }

    // 6. Get Command Buffer
    const vk::raii::CommandBuffer& cmd = m_command[currentFrame];

    // 5. Begin Recording
    cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // 6. Memory Barriers (Enter)
    const vk::ImageMemoryBarrier2 entryBarriers[] = {
        // Color Attachment: Undefined -> ColorAttachmentOptimal
        vk::ImageMemoryBarrier2{
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = frame.image,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        },
        // Depth Attachment: Undefined -> DepthAttachmentOptimal
        vk::ImageMemoryBarrier2{
            .srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = *depthImage.getImage(),
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eDepth,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = entryBarriers
    });

    // 7. Attachments & Rendering Info
    constexpr vk::ClearValue clearColor{.color = m_backgroundColor};
    const vk::RenderingAttachmentInfo colorAttachment{
        .imageView = frame.imageView,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor
    };
    const vk::RenderingAttachmentInfo depthAttachment{
        .imageView = *depthImage.getImageView(),
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearValue{.depthStencil = vk::ClearDepthStencilValue{1.0f, 0}}
    };

    const vk::RenderingInfo renderInfo{
        .renderArea = { .extent = extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment
    };

    cmd.beginRendering(renderInfo);

    // 8. Dynamic States
    cmd.setViewport(0, vk::Viewport{
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    });
    cmd.setScissor(0, vk::Rect2D{ .extent = extent });

    // 9. Record inline commands for each task
    for (const auto& task : m_tasks)
        { task.recordCommands(cmd); }

    // 10. End Rendering
    cmd.endRendering();

    // 11. Memory Barrier (Exit)
    const vk::ImageMemoryBarrier2 exitBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .dstAccessMask = vk::AccessFlagBits2::eNone,
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = frame.image,
        .subresourceRange = vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    cmd.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &exitBarrier
    });

    // 12. End Command Buffer
    cmd.end();

    // 13. Submit to Queue
    const std::array<vk::SemaphoreSubmitInfo, 2> waitInfos = {
        vk::SemaphoreSubmitInfo{
            .semaphore = *m_imageAvailableSemaphores[currentFrame],
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        },
        vk::SemaphoreSubmitInfo{
            .semaphore = waitSemaphore,
            .stageMask = vk::PipelineStageFlagBits2::eVertexInput
        }
    };

    const vk::CommandBufferSubmitInfo finalCmdInfo{.commandBuffer = *cmd};
    const vk::SemaphoreSubmitInfo finalSignalInfo{
        .semaphore = frame.renderFinishedSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eAllGraphics
    };
    
    const vk::SubmitInfo2 finalSubmitInfo{
        .waitSemaphoreInfoCount = waitSemaphore ? 2u : 1u,
        .pWaitSemaphoreInfos = waitInfos.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &finalCmdInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &finalSignalInfo
    };
    
    m_graphicsQueue->submit(finalSubmitInfo, fence);

    // 14. Present
    if (!m_swapchain->present(frame)) // if not optimal or out of date
        { m_swapchain->recreate(); }
}

} // namespace svk
