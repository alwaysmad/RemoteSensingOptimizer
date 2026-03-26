#pragma once

#include <atomic>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace svk
{

class Device;
class Buffer;

// =========================================================================
//  "Reading Session"
//  RAII wrapper for mapped device memory. Unmaps automatically on destruction.
// =========================================================================
class BufferMap
{
public:
    BufferMap(const BufferMap&) = delete;
    BufferMap& operator=(const BufferMap&) = delete;

    inline BufferMap(BufferMap&& other) noexcept
        : m_memory(std::exchange(other.m_memory, nullptr)), m_mapped(std::exchange(other.m_mapped, nullptr)) {}

    inline BufferMap& operator=(BufferMap&& other) noexcept
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

    inline ~BufferMap()
    {
        if (m_memory != nullptr && m_mapped != nullptr)
            { m_memory->unmapMemory(); }
    }

    [[nodiscard]] inline void* get() const { return m_mapped; }

private:
    friend class Buffer;

    inline BufferMap(const vk::raii::DeviceMemory& memory, vk::DeviceSize offset, vk::DeviceSize size)
        : m_memory(&memory), m_mapped(memory.mapMemory(offset, size)) {}

    const vk::raii::DeviceMemory* m_memory = nullptr;
    void* m_mapped = nullptr;
};

// =========================================================================
//  "A book in the institute"
//  RAII wrapper for a Vulkan Buffer and its backing Device Memory.
// =========================================================================
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

    // The handles are pre-allocated by the Device and sunk into this wrapper.
    Buffer(vk::raii::Buffer&& buffer,
           vk::raii::DeviceMemory&& memory,
           vk::DeviceSize size,
           vk::BufferUsageFlags usage,
           std::atomic<uint32_t>& allocCounter);

    std::atomic<uint32_t>* m_allocCounter = nullptr;
    vk::DeviceSize m_size = 0;
    vk::BufferUsageFlags m_usage {};

    vk::raii::Buffer m_buffer = nullptr;
    vk::raii::DeviceMemory m_memory = nullptr;
};

inline Buffer::Buffer(vk::raii::Buffer&& buffer,
                      vk::raii::DeviceMemory&& memory,
                      vk::DeviceSize size,
                      vk::BufferUsageFlags usage,
                      std::atomic<uint32_t>& allocCounter)
    : m_allocCounter(&allocCounter),
      m_size(size),
      m_usage(usage),
      m_buffer(std::move(buffer)),
      m_memory(std::move(memory))
{
    m_buffer.bindMemory(*m_memory, 0);
    m_allocCounter->fetch_add(1, std::memory_order_relaxed);
}

inline Buffer::Buffer(Buffer&& other) noexcept
    : m_allocCounter(std::exchange(other.m_allocCounter, nullptr)),
      m_size(std::exchange(other.m_size, 0)),
      m_usage(std::exchange(other.m_usage, vk::BufferUsageFlags {})),
      m_buffer(std::exchange(other.m_buffer, nullptr)),
      m_memory(std::exchange(other.m_memory, nullptr))
{}

inline Buffer& Buffer::operator=(Buffer&& other) noexcept
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

inline Buffer::~Buffer()
{
    if (m_allocCounter != nullptr)
        { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }
}

inline svk::BufferMap Buffer::map(vk::DeviceSize offset, vk::DeviceSize size) const
    { return svk::BufferMap(m_memory, offset, size); }

// =========================================================================
//  "The Reading Desk Assignment"
//  Lightweight wrapper containing the raw Vulkan handles needed for binding.
// =========================================================================
struct BufferBinding 
{
    vk::Buffer buffer = nullptr;
    uint32_t binding = 0;
    vk::BufferUsageFlags usage; // Required to deduce DescriptorType dynamically

    BufferBinding() = default;

    // Instantly extracts the raw handle and flags from the RAII wrapper
    inline BufferBinding(const svk::Buffer& b, uint32_t bindingSlot)
    : buffer(*b.getBuffer()), binding(bindingSlot), usage(b.getUsage()) {}

    inline vk::DescriptorType deduceDescriptorType() const
    {
        if (usage & vk::BufferUsageFlagBits::eUniformBuffer)
            { return vk::DescriptorType::eUniformBuffer; }

        if (usage & vk::BufferUsageFlagBits::eStorageBuffer)
            { return vk::DescriptorType::eStorageBuffer; }

        throw std::runtime_error("Buffer usage cannot be mapped to a DescriptorType");
    }
};

} // namespace svk