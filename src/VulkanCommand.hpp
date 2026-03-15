#pragma once
#include "VulkanDevice.hpp"

class VulkanCommand
{
public:
	VulkanCommand(const VulkanDevice& device, uint32_t queueFamilyIndex)
		: m_commandPool(
			device.device(),
			vk::CommandPoolCreateInfo{
				.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = queueFamilyIndex
			}
		),
		m_commandBuffers(
			device.device(),
			vk::CommandBufferAllocateInfo{
				.commandPool = m_commandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = MAX_FRAMES_IN_FLIGHT
			}
		)
		{ LOG_DEBUG("VulkanCommand created for Queue Family " << queueFamilyIndex); }

	~VulkanCommand() { LOG_DEBUG("VulkanCommand resources destroyed"); }

	inline const vk::raii::CommandBuffer& getBuffer(uint32_t index) const { return m_commandBuffers[index]; }
private:
	vk::raii::CommandPool m_commandPool;
	vk::raii::CommandBuffers m_commandBuffers;
};
