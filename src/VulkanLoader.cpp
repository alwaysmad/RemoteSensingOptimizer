#include "VulkanLoader.hpp"

VulkanLoader::VulkanLoader(const VulkanDevice& device)
	: m_device(device),
	  m_command(device, device.getTransferQueueIndex()) 
	{ LOG_DEBUG("VulkanLoader initialized"); }

VulkanLoader::~VulkanLoader() { LOG_DEBUG("VulkanLoader destroyed"); }

std::pair<vk::raii::Buffer, TrackedDeviceMemory> 
VulkanLoader::createBuffer(const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage)
{
	// 1. Staging Buffer
	auto [sBuf, sMem] = m_device.createBuffer(size, 
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	
	// Map and Copy
	void* mapped = sMem->mapMemory(0, size);
	std::memcpy(mapped, data, size);
	sMem->unmapMemory();

	// 2. Device Buffer (Add Transfer bits automatically)
	auto [dBuf, dMem] = m_device.createBuffer(size, 
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc | usage,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// 3. Record Copy
	const auto& cmd = m_command.getBuffer(0);
	cmd.reset();
	cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	
	vk::BufferCopy region{ .size = size };
	cmd.copyBuffer(*sBuf, *dBuf, region);
	
	cmd.end();

	// 4. Submit & Wait (Modern Vulkan 1.3)
	const vk::CommandBufferSubmitInfo cmdInfo { .commandBuffer = *cmd };
	const vk::SubmitInfo2 submitInfo {
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmdInfo
	};

	m_device.transferQueue().submit2(submitInfo, nullptr);
	m_device.transferQueue().waitIdle();

	return { std::move(dBuf), std::move(dMem) };
}

void VulkanLoader::downloadBuffer(const vk::raii::Buffer& src, void* dst, vk::DeviceSize size)
{
	// 1. Staging Buffer (Transfer Dst)
	auto [sBuf, sMem] = m_device.createBuffer(size, 
			vk::BufferUsageFlagBits::eTransferDst, 
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// 2. Record Copy (Device -> Staging)
	const auto& cmd = m_command.getBuffer(0);
	cmd.reset();
	cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	vk::BufferCopy region{ .size = size };
	cmd.copyBuffer(*src, *sBuf, region);

	// Barrier: Make transfer visible to Host Read
	const vk::MemoryBarrier2 barrier {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eHost,
		.dstAccessMask = vk::AccessFlagBits2::eHostRead
	};
	const vk::DependencyInfo depInfo { .memoryBarrierCount = 1, .pMemoryBarriers = &barrier };
	cmd.pipelineBarrier2(depInfo);

	cmd.end();

	// 3. Submit & Wait (Modern Vulkan 1.3)
	const vk::CommandBufferSubmitInfo cmdInfo { .commandBuffer = *cmd };
	const vk::SubmitInfo2 submitInfo {
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmdInfo
	};

	m_device.transferQueue().submit2(submitInfo, nullptr);
	m_device.transferQueue().waitIdle();

	// 4. Map & Read
	void* mapped = sMem->mapMemory(0, size);
	std::memcpy(dst, mapped, size);
	sMem->unmapMemory();
}

void VulkanLoader::uploadAsync(
		uint32_t currentFrame,
		const vk::raii::Buffer& src, vk::DeviceSize srcOffset,
		const vk::raii::Buffer& dst, vk::DeviceSize dstOffset,
		vk::DeviceSize size,
		vk::Semaphore signalSemaphore,
    		vk::PipelineStageFlags2 dstStage,
		vk::AccessFlags2 dstAccess )
{
	// 1. Get a Command Buffer for this frame
	const auto& cmd = m_command.getBuffer(currentFrame);

	cmd.reset();
	cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	// 2. Record Copy
	const vk::BufferCopy region {
		.srcOffset = srcOffset,
		.dstOffset = dstOffset,
		.size = size
	};
	cmd.copyBuffer(*src, *dst, region);

	// 3. Barrier: Ensure transfer write is available/visible
	// (Though Semaphores handle execution dependency, a barrier is good practice
	// to flush caches before the semaphore signals).
	const vk::BufferMemoryBarrier2 barrier {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = dstStage,
		.dstAccessMask = dstAccess,	
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored, // Concurrent sharing used
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.buffer = *dst,
		.offset = dstOffset,
		.size = size
	};

	const vk::DependencyInfo depInfo {
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &barrier
	};
	cmd.pipelineBarrier2(depInfo);

	cmd.end();

	// 4. Submit with Signal
	const vk::CommandBufferSubmitInfo cmdInfo { .commandBuffer = *cmd };

	const vk::SemaphoreSubmitInfo signalInfo {
		.semaphore = signalSemaphore,
		.stageMask = vk::PipelineStageFlagBits2::eTransfer
	};

	const vk::SubmitInfo2 submitInfo {
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmdInfo,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signalInfo
	};

	m_device.transferQueue().submit2(submitInfo, nullptr);
}
