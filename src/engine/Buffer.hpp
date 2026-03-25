#pragma once

#include <atomic>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace svk
{

class Device;

class BufferMap
{
public:
    BufferMap(const BufferMap&) = delete;
    BufferMap& operator=(const BufferMap&) = delete;

    BufferMap(BufferMap&& other) noexcept;
    BufferMap& operator=(BufferMap&& other) noexcept;

    ~BufferMap();

    [[nodiscard]] inline void* get() const { return m_mapped; }

private:
    friend class Buffer;

    BufferMap(const vk::raii::DeviceMemory& memory, vk::DeviceSize offset, vk::DeviceSize size);

    const vk::raii::DeviceMemory* m_memory = nullptr;
    void* m_mapped = nullptr;
};

class Buffer
{
public:
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    ~Buffer();

    [[nodiscard]] svk::BufferMap map(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) const;

    [[nodiscard]] inline vk::BufferUsageFlags getUsage() const { return m_usage; }
    [[nodiscard]] inline vk::DeviceSize getSize() const { return m_size; }
    [[nodiscard]] inline const vk::raii::Buffer& getBuffer() const { return m_buffer; }

private:
    friend class Device;

    Buffer(const vk::raii::Device& device,
           const vk::raii::PhysicalDevice& physicalDevice,
           vk::DeviceSize size,
           vk::BufferUsageFlags usage,
           vk::MemoryPropertyFlags properties,
           const std::vector<uint32_t>& uniqueQueueFamilies,
           std::atomic<uint32_t>& allocCounter);

    std::atomic<uint32_t>* m_allocCounter = nullptr;
    vk::DeviceSize m_size = 0;
    vk::BufferUsageFlags m_usage {};

    vk::raii::Buffer m_buffer = nullptr;
    vk::raii::DeviceMemory m_memory = nullptr;
};

} // namespace svk
