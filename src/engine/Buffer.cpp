#include <atomic>
#include <stdexcept>
#include <utility>

#include "engine/Buffer.hpp"

namespace svk
{

BufferMap::BufferMap(const vk::raii::DeviceMemory& memory, vk::DeviceSize offset, vk::DeviceSize size)
    : m_memory(&memory), m_mapped(memory.mapMemory(offset, size))
{}

BufferMap::BufferMap(BufferMap&& other) noexcept
    : m_memory(std::exchange(other.m_memory, nullptr)),
      m_mapped(std::exchange(other.m_mapped, nullptr))
{}

BufferMap& BufferMap::operator=(BufferMap&& other) noexcept
{
    if (this != &other)
    {
        if (m_memory != nullptr && m_mapped != nullptr)
            { m_memory->unmapMemory(); }
        m_memory = std::exchange(other.m_memory, nullptr);
        m_mapped = std::exchange(other.m_mapped, nullptr);
    }
    return *this;
}

BufferMap::~BufferMap()
{
    if (m_memory != nullptr && m_mapped != nullptr)
        { m_memory->unmapMemory(); }
}

Buffer::Buffer(const vk::raii::Device& device,
                vk::raii::Buffer&& buffer,
                const vk::MemoryRequirements& memoryRequirements,
                uint32_t memoryTypeIndex,
                vk::DeviceSize size,
                vk::BufferUsageFlags usage,
                std::atomic<uint32_t>& allocCounter)
    : m_allocCounter(&allocCounter),
      m_size(size),
      m_usage(usage),
      m_buffer(std::move(buffer))
{
    const vk::MemoryAllocateInfo allocInfo {
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };

    m_memory = vk::raii::DeviceMemory(device, allocInfo);
    m_buffer.bindMemory(*m_memory, 0);

    m_allocCounter->fetch_add(1, std::memory_order_relaxed);
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_allocCounter(std::exchange(other.m_allocCounter, nullptr)),
      m_size(std::exchange(other.m_size, 0)),
      m_usage(std::exchange(other.m_usage, vk::BufferUsageFlags {})),
      m_buffer(std::exchange(other.m_buffer, nullptr)),
      m_memory(std::exchange(other.m_memory, nullptr))
{}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (this != &other)
    {
        if (m_allocCounter != nullptr)
            { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }

        m_allocCounter = std::exchange(other.m_allocCounter, nullptr);
        m_size = std::exchange(other.m_size, 0);
        m_usage = std::exchange(other.m_usage, vk::BufferUsageFlags {});
        m_buffer = std::exchange(other.m_buffer, nullptr);
        m_memory = std::exchange(other.m_memory, nullptr);
    }
    return *this;
}

Buffer::~Buffer()
{
    if (m_allocCounter != nullptr)
        { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }
}

svk::BufferMap Buffer::map(vk::DeviceSize offset, vk::DeviceSize size) const
    { return svk::BufferMap(m_memory, offset, size); }

} // namespace svk
