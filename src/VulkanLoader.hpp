#pragma once
#include "VulkanDevice.hpp"
#include "VulkanCommand.hpp"

class VulkanLoader
{
public:
	explicit VulkanLoader(const VulkanDevice& device);
	~VulkanLoader();

	// 1. GENERIC SYNC UPLOAD (Create + Upload)
	// Creates a DeviceLocal buffer and fills it with 'data'.
	// Usage automatically includes TransferDst | TransferSrc.
	[[nodiscard]] std::pair<vk::raii::Buffer, TrackedDeviceMemory> 
	createBuffer(const void* data, vk::DeviceSize size, vk::BufferUsageFlags usage);

	// 2. GENERIC SYNC DOWNLOAD (Download)
	// Reads 'src' buffer back to CPU 'dst' pointer.
	void downloadBuffer(const vk::raii::Buffer& src, void* dst, vk::DeviceSize size);

	// 3. ASYNC UPLOAD (Staging -> Device)
	// Records copy from src -> dst
	// Submits to Transfer Queue
	// Signals 'signalSemaphore' when done
	void uploadAsync( uint32_t currentFrame,
			  const vk::raii::Buffer& src, vk::DeviceSize srcOffset,
			  const vk::raii::Buffer& dst, vk::DeviceSize dstOffset,
			  vk::DeviceSize size, vk::Semaphore signalSemaphore,
			  vk::PipelineStageFlags2 dstStage = vk::PipelineStageFlagBits2::eComputeShader,
    			  vk::AccessFlags2 dstAccess = vk::AccessFlagBits2::eUniformRead
	);

private:
	const VulkanDevice& m_device;
	VulkanCommand m_command; 
};
