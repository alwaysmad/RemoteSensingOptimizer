// src/engine/Device.hpp
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <span>
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
	static constexpr float queuePriority = 1.0f;
	static constexpr uint32_t kVulkanRecommendedMaxAllocations = 4096;
	static constexpr uint32_t kAllocationHardWarning = 4086;
	static constexpr uint32_t kAllocationNearWarning = 4000;
	static constexpr vk::DeviceSize kConventionalUboLimit = 64ull * 1024ull; // 64 KB

	static void logAllocationPressure(const svk::ConsoleLogger& logger, uint32_t currentCount);

	static void selectPhysicalDevice(const vk::raii::Instance& instance,
	                                 const std::string& requestedDeviceName,
	                                 vk::raii::PhysicalDevice& outPhysicalDevice,
	                                 const svk::ConsoleLogger& logger);

	static void pickHeadlessQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice,
	                                      uint32_t& outGraphics,
	                                      uint32_t& outPresent,
	                                      uint32_t& outCompute,
	                                      uint32_t& outTransfer);

	static void pickSurfaceQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice,
	                                     const vk::raii::SurfaceKHR& surface,
	                                     uint32_t& outGraphics,
	                                     uint32_t& outPresent,
	                                     uint32_t& outCompute,
	                                     uint32_t& outTransfer);

	void initialize(const svk::Instance& instance, const vk::raii::SurfaceKHR* surface, const std::string& deviceName);

	// private helper functions
	[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

	vk::raii::PhysicalDevice m_physicalDevice = nullptr;
	vk::raii::Device m_device = nullptr;
	const svk::ConsoleLogger& m_logger;

	std::array<uint32_t, 4> m_queueMapping{0, 0, 0, 0};
	std::array<svk::Queue, 4> m_queues;

	mutable std::atomic<uint32_t> allocationCount{0};

public:
	// NOLINTNEXTLINE(cppcoreguidelines-use-enum-class)
	enum QueueType : uint32_t
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
	{
		initialize(instance, nullptr, deviceName);
	}

	// 2. Surfaced Mode (Graphics & Present Enabled)
	Device(const svk::Instance& instance,
	       const vk::raii::SurfaceKHR& surface,
	       const std::string& deviceName,
	       const svk::ConsoleLogger& logger)
	    : m_logger(logger),
	      m_queues{svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device)}
	{
		initialize(instance, &surface, deviceName);
	}

	// Ironclad Constraints
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
	Device(Device&&) = delete;
	Device& operator=(Device&&) = delete;

	~Device() = default;

	// Accessors
	[[nodiscard]] const vk::raii::PhysicalDevice& physicalDevice() const { return m_physicalDevice; }
	[[nodiscard]] const vk::raii::Device& device() const { return m_device; }

	// The Laboratories
	[[nodiscard]] const svk::Queue& transferQueue() const { return m_queues[m_queueMapping[TRANSFER]]; }
	[[nodiscard]] const svk::Queue& computeQueue() const { return m_queues[m_queueMapping[COMPUTE]]; }
	[[nodiscard]] const svk::Queue& graphicsQueue() const { return m_queues[m_queueMapping[GRAPHICS]]; }
	[[nodiscard]] const svk::Queue& presentQueue() const { return m_queues[m_queueMapping[PRESENT]]; }

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
	{
		return svk::Command(m_device, m_queueMapping[queueType], count, flags);
	}

	void waitIdle() const
	{
		// Unconditionally lock all 4 laboratories instantly to prevent deadlocks.
		// This ensures no thread is mid-submission before we halt the GPU.
		const std::scoped_lock lock(m_queues[0].m_mutex, m_queues[1].m_mutex, m_queues[2].m_mutex, m_queues[3].m_mutex);
		m_device.waitIdle();
	}
};