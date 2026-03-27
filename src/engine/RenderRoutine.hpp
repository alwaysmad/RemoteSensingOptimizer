// src/engine/RenderRoutine.hpp
#pragma once

#include <vector>
#include <array>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Device.hpp"
#include "engine/Swapchain.hpp"
#include "engine/Queue.hpp"
#include "engine/Command.hpp"
#include "engine/DepthResources.hpp"
#include "engine/RenderTask.hpp"

namespace svk 
{

// =========================================================================
//  "The ArchWizard"
//  Commands the execution of RenderTasks and manages the Vulkan timeline.
// =========================================================================
class RenderRoutine 
{
public:
    // Ironclad Constraints
    RenderRoutine(const RenderRoutine&) = delete;
    RenderRoutine& operator=(const RenderRoutine&) = delete;
    RenderRoutine(RenderRoutine&&) = default;
    RenderRoutine& operator=(RenderRoutine&&) = default;

    RenderRoutine(const svk::Device& device, svk::Swapchain& swapchain, uint32_t bufferCount);

    // Temporary Public Access
    std::vector<svk::RenderTask> m_tasks;

    [[nodiscard]] inline vk::Format getDepthFormat() const { return m_depthResources.getFormat(); }

    // The Master Spell: Executes the frame
    void draw(uint32_t currentFrame, vk::Fence fence, vk::Semaphore waitSemaphore = nullptr);

private:
    svk::Swapchain* m_swapchain;
    const svk::Queue* m_graphicsQueue;

    // The ArchWizard's Arsenal
    svk::DepthResources m_depthResources;
    svk::Command m_command;

    // Per-frame image-acquire semaphores owned by the render routine
    std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;

    // The Void
    static constexpr std::array<float, 4> m_backgroundColor = {0.01f, 0.01f, 0.01f, 1.0f};
};

} // namespace svk