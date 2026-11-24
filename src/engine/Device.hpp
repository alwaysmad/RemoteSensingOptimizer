// src/engine/Device.hpp
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Buffer.hpp"
#include "engine/Command.hpp"
#include "engine/Image.hpp"
#include "engine/Queue.hpp"

namespace svk
{
class ConsoleLogger;
class Instance;
class Device;
} // namespace svk

class svk::Device
{
private:
	// Default normalized priority [0.0, 1.0] assigned to every created device queue.
	static constexpr float queuePriority = 1.0f;
	// Vulkan spec guarantees at least 4096 maxMemoryAllocationCount on conforming implementations.
	static constexpr uint32_t kVulkanRecommendedMaxAllocations = 4096;
	// Thresholds for warning when device memory allocations approach the 4096 guaranteed minimum.
	static constexpr uint32_t kAllocationHardWarning = 4086;
	static constexpr uint32_t kAllocationNearWarning = 4000;
	// Conventional 64 KiB baseline UBO budget; warn if a UBO exceeds this portable size.
	static constexpr vk::DeviceSize kConventionalUboLimit = 64ull * 1024ull;
	// Sentinel indicating a QueueType has not been mapped to an initialized slot in m_queues.
	static constexpr std::size_t kUnassignedQueueSlot = std::numeric_limits<std::size_t>::max();
	// Sentinel indicating a Vulkan queue family index has not been selected yet.
	static constexpr uint32_t kUnassignedQueueFamily = std::numeric_limits<uint32_t>::max();

	void logAllocationPressure() const;

	mutable std::atomic<uint32_t> allocationCount{0};

	void selectPhysicalDevice(const vk::raii::Instance& instance, const std::string& requestedDeviceName);

	void pickComputeAndTransferQueues(uint32_t& outCompute, uint32_t& outTransfer) const;

	void pickGraphicsAndPresentQueues(const vk::raii::SurfaceKHR& surface,
	                                  uint32_t& outGraphics,
	                                  uint32_t& outPresent) const;

	void initialize(const svk::Instance& instance, const vk::raii::SurfaceKHR* surface, const std::string& deviceName);

	// private helper functions
	[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

	vk::raii::PhysicalDevice m_physicalDevice = nullptr;
	vk::raii::Device m_device = nullptr;
	const svk::ConsoleLogger& m_logger;

	std::array<std::size_t, 4> m_queueMapping{
	    kUnassignedQueueSlot, kUnassignedQueueSlot, kUnassignedQueueSlot, kUnassignedQueueSlot};
	std::array<svk::Queue, 4> m_queues;

public:
	// NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
	enum QueueType : std::size_t
	{
		TRANSFER = 0,
		COMPUTE = 1,
		GRAPHICS = 2,
		PRESENT = 3
	};

	// 1. Headless Mode (Compute & Transfer Only)
	Device(const svk::Instance& instance, const std::string& deviceName, const svk::ConsoleLogger& logger)
	    : m_logger(logger),
	      m_queues{svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device)}
	{ initialize(instance, nullptr, deviceName); }

	// 2. Surfaced Mode (Graphics & Present Enabled)
	Device(const svk::Instance& instance,
	       const vk::raii::SurfaceKHR& surface,
	       const std::string& deviceName,
	       const svk::ConsoleLogger& logger)
	    : m_logger(logger),
	      m_queues{svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device)}
	{ initialize(instance, &surface, deviceName); }

	// Ironclad Constraints
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
	Device(Device&&) = delete;
	Device& operator=(Device&&) = delete;

	~Device() = default;

	// Accessors
	[[nodiscard]] const vk::raii::PhysicalDevice& physicalDevice() const { return m_physicalDevice; }
	[[nodiscard]] const vk::raii::Device& device() const { return m_device; }

	// Validates a bound UBO descriptor range against hardware maxUniformBufferRange and the 64 KiB baseline.
	void validateUniformBufferRange(vk::DeviceSize boundRange) const;
	// Validates effective descriptor binding bounds (and UBO range limits when applicable).
	void validateDescriptorBinding(const svk::BufferBinding& binding) const;

	// The Laboratories
	[[nodiscard]] const svk::Queue& transferQueue() const { return m_queues[queueSlot(TRANSFER)]; }
	[[nodiscard]] const svk::Queue& computeQueue() const { return m_queues[queueSlot(COMPUTE)]; }
	[[nodiscard]] const svk::Queue& graphicsQueue() const { return m_queues[queueSlot(GRAPHICS)]; }
	[[nodiscard]] const svk::Queue& presentQueue() const { return m_queues[queueSlot(PRESENT)]; }

	// Factory Methods
	[[nodiscard]] svk::Buffer createBuffer(vk::DeviceSize size,
	                                       vk::BufferUsageFlags usage,
	                                       vk::MemoryPropertyFlags properties,
	                                       std::span<const QueueType> targetQueues) const;

	[[nodiscard]] svk::Buffer createBuffer(vk::DeviceSize size,
	                                       vk::BufferUsageFlags usage,
	                                       vk::MemoryPropertyFlags properties,
	                                       std::initializer_list<QueueType> targetQueues) const
	{
		return createBuffer(
		    size, usage, properties, std::span<const QueueType>(targetQueues.begin(), targetQueues.size()));
	}

	[[nodiscard]] svk::Image createImage(const vk::ImageCreateInfo& imageInfo,
	                                     vk::MemoryPropertyFlags properties,
	                                     vk::ImageAspectFlags aspectFlags) const;

	[[nodiscard]] svk::Command
	createCommand(QueueType queueType, uint32_t count, vk::CommandPoolCreateFlags flags) const
	{ return svk::Command(m_device, m_queues[queueSlot(queueType)].getFamilyIndex(), count, flags); }

	void waitIdle() const
	{
		// Unconditionally lock all 4 laboratories instantly to prevent deadlocks.
		// This ensures no thread is mid-submission before we halt the GPU.
		const std::scoped_lock lock(m_queues[0].m_mutex, m_queues[1].m_mutex, m_queues[2].m_mutex, m_queues[3].m_mutex);
		m_device.waitIdle();
	}

private:
	[[nodiscard]] std::size_t queueSlot(QueueType queueType) const
	{
		const auto index = static_cast<std::size_t>(queueType);
		if (index >= m_queueMapping.size() || m_queueMapping[index] == kUnassignedQueueSlot) [[unlikely]]
		{
			throw std::logic_error("Requested queue type is not initialized on this svk::Device");
		}
		return m_queueMapping[index];
	}
};