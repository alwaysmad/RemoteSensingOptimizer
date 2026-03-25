#pragma once

#include <cstddef>
#include <vulkan/vulkan_raii.hpp>

namespace svk
{

class Device;

class Command
{
public:
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;

    Command(Command&&) noexcept = default;
    Command& operator=(Command&&) noexcept = default;

    ~Command() = default;

    [[nodiscard]] inline const vk::raii::CommandBuffers& getBuffers() const { return m_buffers; }
    [[nodiscard]] inline const vk::raii::CommandBuffer& operator[](size_t index) const { return m_buffers[index]; }

private:
    friend class Device;

    Command(const vk::raii::Device& device,
            uint32_t queueFamilyIndex,
            uint32_t count,
            vk::CommandPoolCreateFlags flags)
        : m_pool(
            device,
            vk::CommandPoolCreateInfo {
                .flags = flags,
                .queueFamilyIndex = queueFamilyIndex,
            }),
          m_buffers(
            device,
            vk::CommandBufferAllocateInfo {
                .commandPool = *m_pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = count,
            })
    {}

    vk::raii::CommandPool m_pool = nullptr;
    vk::raii::CommandBuffers m_buffers = nullptr;
};

} // namespace svk