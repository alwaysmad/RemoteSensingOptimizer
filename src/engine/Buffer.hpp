// src/engine/Buffer.hpp
#pragma once

#include <atomic>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace svk
{
class Device;
class Buffer;
class BufferMap;
struct BufferBinding;
} // namespace svk

// =========================================================================
//  "Reading Session"
//  RAII wrapper for mapped device memory. Unmaps automatically on destruction.
// =========================================================================
class svk::BufferMap
{
private:
	const vk::raii::DeviceMemory* m_memory = nullptr;
	void* m_mapped = nullptr;

	// Only the Buffer class can create a BufferMap
	friend class Buffer;
	BufferMap(const vk::raii::DeviceMemory& memory, vk::DeviceSize offset, vk::DeviceSize size)
	    : m_memory(&memory), m_mapped(memory.mapMemory(offset, size))
	{}

public:
	BufferMap(const BufferMap&) = delete;
	BufferMap& operator=(const BufferMap&) = delete;

	BufferMap(BufferMap&& other) noexcept
	    : m_memory(std::exchange(other.m_memory, nullptr)), m_mapped(std::exchange(other.m_mapped, nullptr))
	{}

	BufferMap& operator=(BufferMap&& other) noexcept
	{
		if (this != &other)
		{
			if (m_memory != nullptr && m_mapped != nullptr) { m_memory->unmapMemory(); }
			m_memory = std::exchange(other.m_memory, nullptr);
			m_mapped = std::exchange(other.m_mapped, nullptr);
		}
		return *this;
	}

	~BufferMap()
	{
		if (m_memory != nullptr && m_mapped != nullptr) { m_memory->unmapMemory(); }
	}

	[[nodiscard]] void* get() const { return m_mapped; }
};

// =========================================================================
//  "A book in the institute"
//  RAII wrapper for a Vulkan Buffer and its backing Device Memory.
// =========================================================================
class svk::Buffer
{
private:
	std::atomic<uint32_t>* m_allocCounter = nullptr; // points to the Device's allocation counter
	vk::DeviceSize m_size = 0;
	vk::BufferUsageFlags m_usage;

	vk::raii::Buffer m_buffer = nullptr;
	vk::raii::DeviceMemory m_memory = nullptr;

	// Only the Device class can create a Buffer
	friend class Device;
	// The handles are pre-allocated by the Device and sunk into this wrapper.
	Buffer(vk::raii::Buffer&& buffer,
	       vk::raii::DeviceMemory&& memory,
	       vk::DeviceSize size,
	       vk::BufferUsageFlags usage,
	       std::atomic<uint32_t>& allocCounter)
	    : m_allocCounter(&allocCounter), m_size(size), m_usage(usage), m_buffer(std::move(buffer)),
	      m_memory(std::move(memory))
	{
		m_buffer.bindMemory(*m_memory, 0);
		m_allocCounter->fetch_add(1, std::memory_order_relaxed);
	}

public:
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

	Buffer(Buffer&& other) noexcept
	    : m_allocCounter(std::exchange(other.m_allocCounter, nullptr)), m_size(std::exchange(other.m_size, 0)),
	      m_usage(std::exchange(other.m_usage, vk::BufferUsageFlags{})),
	      m_buffer(std::exchange(other.m_buffer, nullptr)), m_memory(std::exchange(other.m_memory, nullptr))
	{}

	Buffer& operator=(Buffer&& other) noexcept
	{
		if (this != &other)
		{
			if (m_allocCounter != nullptr) { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }

			m_allocCounter = std::exchange(other.m_allocCounter, nullptr);
			m_size = std::exchange(other.m_size, 0);
			m_usage = std::exchange(other.m_usage, vk::BufferUsageFlags{});
			m_buffer = std::exchange(other.m_buffer, nullptr);
			m_memory = std::exchange(other.m_memory, nullptr);
		}
		return *this;
	}

	~Buffer()
	{
		if (m_allocCounter != nullptr) { m_allocCounter->fetch_sub(1, std::memory_order_relaxed); }
	}

	[[nodiscard]] BufferMap map(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) const
	{
		return BufferMap(m_memory, offset, size);
	}

	[[nodiscard]] vk::BufferUsageFlags getUsage() const { return m_usage; }
	[[nodiscard]] vk::DeviceSize getSize() const { return m_size; }
	[[nodiscard]] const vk::raii::Buffer& getBuffer() const { return m_buffer; }
};

// =========================================================================
//  "The Reading Desk Assignment"
//  Lightweight wrapper containing the raw Vulkan handles needed for binding.
// =========================================================================
struct svk::BufferBinding
{
	vk::Buffer buffer = nullptr;
	uint32_t binding = 0;
	vk::BufferUsageFlags usage; // Required to deduce DescriptorType dynamically

	BufferBinding() = default;

	// Instantly extracts the raw handle and flags from the RAII wrapper
	BufferBinding(const svk::Buffer& b, uint32_t bindingSlot)
	    : buffer(*b.getBuffer()), binding(bindingSlot), usage(b.getUsage())
	{}

	vk::DescriptorType deduceDescriptorType() const
	{
		if (usage & vk::BufferUsageFlagBits::eUniformBuffer) { return vk::DescriptorType::eUniformBuffer; }

		if (usage & vk::BufferUsageFlagBits::eStorageBuffer) { return vk::DescriptorType::eStorageBuffer; }

		throw std::runtime_error("Buffer usage cannot be mapped to a DescriptorType");
	}
};