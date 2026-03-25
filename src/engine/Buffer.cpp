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
               const vk::raii::PhysicalDevice& physicalDevice,
               vk::DeviceSize size,
               vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags properties,
               const std::vector<uint32_t>& uniqueQueueFamilies,
               std::atomic<uint32_t>& allocCounter)
    : m_allocCounter(&allocCounter),
      m_size(size),
      m_usage(usage)
{
    vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* queueFamilyIndices = nullptr;

    if (uniqueQueueFamilies.size() > 1)
    {
        sharingMode = vk::SharingMode::eConcurrent;
        queueFamilyIndexCount = static_cast<uint32_t>(uniqueQueueFamilies.size());
        queueFamilyIndices = uniqueQueueFamilies.data();
    }

    const vk::BufferCreateInfo bufferInfo {
        .size = m_size,
        .usage = m_usage,
        .sharingMode = sharingMode,
        .queueFamilyIndexCount = queueFamilyIndexCount,
        .pQueueFamilyIndices = queueFamilyIndices,
    };

    m_buffer = vk::raii::Buffer(device, bufferInfo);

    const vk::MemoryRequirements memoryRequirements = m_buffer.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const bool typeMatches = (memoryRequirements.memoryTypeBits & (1u << i)) != 0u;
        const bool propertyMatches = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeMatches && propertyMatches)
            { memoryTypeIndex = i; break; }
    }

    if (memoryTypeIndex == UINT32_MAX)
        { throw std::runtime_error("Failed to find suitable memory type for buffer"); }

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
