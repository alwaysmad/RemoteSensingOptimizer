// src/engine/Device.hpp
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Queue.hpp"

namespace svk
{

class Instance;

class Device
{
public:
    // 1. Headless Mode (Compute & Transfer Only)
    Device(const svk::Instance& instance, const std::string& deviceName);

    // 2. Surfaced Mode (Graphics & Present Enabled)
    Device(const svk::Instance& instance, const vk::raii::SurfaceKHR& surface, const std::string& deviceName);

    // Ironclad Constraints
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    ~Device() = default;

    // Accessors
    [[nodiscard]] inline const vk::raii::PhysicalDevice& physicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] inline const vk::raii::Device& device() const { return m_device; }

    // The Laboratories
    [[nodiscard]] inline const svk::Queue& transferQueue() const { return m_queues[m_queueMapping[TRANSFER]]; }
    [[nodiscard]] inline const svk::Queue& computeQueue() const { return m_queues[m_queueMapping[COMPUTE]]; }
    [[nodiscard]] inline const svk::Queue& graphicsQueue() const { return m_queues[m_queueMapping[GRAPHICS]]; }
    [[nodiscard]] inline const svk::Queue& presentQueue() const { return m_queues[m_queueMapping[PRESENT]]; }

    // Global Sync & State
    std::atomic<uint32_t> allocationCount { 0 };
    inline void waitIdle() const
    {
        // Unconditionally lock all 4 laboratories instantly to prevent deadlocks. 
        // This ensures no thread is mid-submission before we halt the GPU.
        std::scoped_lock lock(
            m_queues[0].m_mutex, 
            m_queues[1].m_mutex, 
            m_queues[2].m_mutex, 
            m_queues[3].m_mutex
        );
        m_device.waitIdle();
    }

private:
    void initialize(const svk::Instance& instance, const vk::raii::SurfaceKHR* surface, const std::string& deviceName);

    vk::raii::PhysicalDevice m_physicalDevice = nullptr;
    vk::raii::Device m_device = nullptr;

    enum QueueType : uint32_t {
        TRANSFER = 0,
        COMPUTE = 1,
        GRAPHICS = 2,
        PRESENT = 3
    };

    std::array<uint32_t, 4> m_queueMapping { 0, 0, 0, 0 };
    std::array<svk::Queue, 4> m_queues;
};

} // namespace svk
