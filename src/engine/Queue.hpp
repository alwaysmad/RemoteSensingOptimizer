#pragma once

#include <cstdint>  // uint32_t, UINT32_MAX
#include <mutex>    // Thread-safe lock guards for submissions
#include <utility>  // std::move
#include <vulkan/vulkan_raii.hpp>

namespace svk 
{

/**
 * @brief "A laboratory in the institute"
 * Wraps a Vulkan Queue with a mutex to ensure thread-safe command submissions
 * and presentations across the entire engine architecture.
 * 
 * Uses two-stage initialization: constructed with a Device reference, then
 * initialized with the actual queue handle via init(). Non-movable, designed
 * for storage in std::array within svk::Device.
 */
class Queue 
{
    friend class Device;
public:
    // 1. Construct with the Reference
    explicit Queue(const vk::raii::Device& device)
        : m_device(device) 
    {}

    // Ironclad Constraints
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue&&) = delete;
    Queue& operator=(Queue&&) = delete;

    ~Queue() = default;

    // --- Attributes ---
    [[nodiscard]] inline uint32_t getFamilyIndex() const { return m_queueFamilyIndex; }

    // --- Thread-Safe Operations ---
    inline void submit(const vk::SubmitInfo2& submitInfo, const vk::Fence fence = nullptr) const
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.submit2(submitInfo, fence);
    }

    inline void dummySubmit(const vk::Fence fence, const vk::Semaphore waitSemaphore = nullptr) const
    {
        if (fence) // Safely using the reference!
            { m_device.resetFences({fence}); }

        const vk::SemaphoreSubmitInfo waitInfo {
            .semaphore = waitSemaphore,
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands
        };

        const vk::SubmitInfo2 submitInfo {
            .waitSemaphoreInfoCount = waitSemaphore ? 1u : 0u,
            .pWaitSemaphoreInfos = waitSemaphore ? &waitInfo : nullptr
        };

        const std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.submit2(submitInfo, fence);
    }

    inline vk::Result present(const vk::PresentInfoKHR& presentInfo) const
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.presentKHR(presentInfo);
    }

    inline void waitIdle() const
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.waitIdle();
    }

private:
    // 2. The Binder (Two-Stage)
    inline void init(vk::raii::Queue queue, uint32_t familyIndex)
    {
        m_queue = std::move(queue); 
        m_queueFamilyIndex = familyIndex;
    }
    const vk::raii::Device& m_device; // Locked permanently to the parent's member
    vk::raii::Queue m_queue = nullptr;
    uint32_t m_queueFamilyIndex = UINT32_MAX;
    mutable std::mutex m_mutex;
};

} // namespace svk
