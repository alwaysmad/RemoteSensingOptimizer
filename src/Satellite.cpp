#include "Satellite.hpp"
#include "VulkanLoader.hpp"

SatelliteNetwork::SatelliteNetwork(const VulkanDevice& device, uint32_t count) 
	: m_device(device)
{
	// Yell at user if too many satellites
	if (count > MAX_SATELLITES)
		{ throw std::runtime_error("Satellite count is higher " + std::to_string(MAX_SATELLITES)); }

	////////////////////////////////////////////////////////////////////////////////
	// Check UBO Size Limits
	////////////////////////////////////////////////////////////////////////////////
	const auto props = device.physicalDevice().getProperties();
	const auto uboLimit = props.limits.maxUniformBufferRange;

	LOG_DEBUG("Device UBO Limit: " << uboLimit << " bytes");
	LOG_DEBUG("Required Satellite Buffer: " << requiredUBOsize << " bytes");

	if (uboLimit < requiredUBOsize)
	{
		// Yell at the user (Critical Error)
		std::cerr << DBG_COLOR_RED
			<< "Selected device '" << props.deviceName
			<< "' has insufficient Uniform Buffer space!" << std::endl;
		std::cerr << "\tLimit: " << uboLimit << " bytes" << std::endl;
		std::cerr << "\tRequired: " << requiredUBOsize << " bytes" <<  DBG_COLOR_RESET << std::endl;

		throw std::runtime_error("Device UBO limit too small for satellite data");
	}

	// Pre-allocate vector capacity to avoid reallocations
	satellites.reserve(count);
	satellites.resize(count); // tmp, will remove later

	// 0. Uniform Buffer offsets must be aligned to minUniformBufferOffsetAlignment (usually 256 bytes)
	const auto align = m_device.physicalDevice().getProperties().limits.minUniformBufferOffsetAlignment;
	m_frameSize = (requiredUBOsize + align - 1) & ~(align - 1); // Align up

	// 1. Create Staging Ring Buffer (Size * Frames)
	const vk::DeviceSize stagingSize = m_frameSize * MAX_FRAMES_IN_FLIGHT;
	auto [sBuf, sMem] = m_device.createBuffer(
		stagingSize,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);
	m_stagingBuffer = std::move(sBuf);
	m_stagingMemory = std::move(sMem);
	m_mappedPtr = m_stagingMemory->mapMemory(0, vk::WholeSize);

	// 2. Create GPU Buffer (Single Frame Size)
	// Must be eDeviceLocal for performance
	auto [dBuf, dMem] = m_device.createBuffer(
		m_frameSize, // Only 1 frame worth of space
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal
	);
	m_deviceBuffer = std::move(dBuf);
	m_deviceMemory = std::move(dMem);
	
	LOG_DEBUG("SatelliteNetwork: Staging Ring + Device Local Buffer created");
}

SatelliteNetwork::~SatelliteNetwork()
{
	// RAII handles unmapping and destruction
	LOG_DEBUG("SatelliteNetwork destroyed");
}

void SatelliteNetwork::upload(uint32_t currentFrame, VulkanLoader& loader, vk::Semaphore signalSemaphore)
{
	// 1. CPU Write to Staging Ring
	// We use the 'currentFrame' slot in the staging buffer
	const size_t stagingOffset = currentFrame * m_frameSize;

	// Safety clamp
	size_t count = std::min(static_cast<uint32_t>(satellites.size()), MAX_SATELLITES);

	// 2. Copy to staging buffer
	char* dst = static_cast<char*>(m_mappedPtr) + stagingOffset;
	std::memcpy(dst, satellites.data(), count * sizeof(SatelliteData));

	// 3. Request Async Upload
	// Src: Staging[Frame] -> Dst: Device[0] (Always offset 0)
	loader.uploadAsync(
		currentFrame,
		m_stagingBuffer, stagingOffset,
		m_deviceBuffer, 0,
		m_frameSize,
		signalSemaphore
	);
}
