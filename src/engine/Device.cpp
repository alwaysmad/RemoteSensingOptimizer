// src/engine/Device.cpp
#include <algorithm> // std::ranges::sort, std::ranges::unique
#include <atomic>    // std::memory_order_relaxed
#include <cstddef>   // std::size_t
#include <cstdint>   // uint32_t
#include <span>
#include <stdexcept> // std::runtime_error
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "engine/ConsoleLogger.hpp"
#include "engine/Device.hpp"
#include "engine/Flags.hpp"
#include "engine/Instance.hpp"

void svk::Device::logAllocationPressure() const
{
	const uint32_t currentCount = allocationCount.load(std::memory_order_relaxed);
	if (currentCount >= kAllocationHardWarning)
	{
		m_logger.cWarn("[WARNING] Device allocation count is {} (very close to Vulkan limit {}).",
		               currentCount,
		               kVulkanRecommendedMaxAllocations);
	}
	else if (currentCount >= kAllocationNearWarning)
	{
		m_logger.cWarn("[WARNING] Device allocation count is {} (nearing Vulkan limit {}).",
		               currentCount,
		               kVulkanRecommendedMaxAllocations);
	}
}

void svk::Device::selectPhysicalDevice(const vk::raii::Instance& instance, const std::string& requestedDeviceName)
{
	// Find and pick physical device based on name
	const auto devices = instance.enumeratePhysicalDevices();
	if (devices.empty()) { throw std::runtime_error("Failed to find any device with Vulkan support"); }

	// Log all available devices if debug
	if constexpr (svk::enableValidationLayers)
	{
		m_logger.cDebug("Available physical devices ({}):", devices.size());
		for (const auto& device : devices)
		{
			const auto props = device.getProperties();
			m_logger.cDebug("  - {} (api {}.{}.{})",
			                std::string(props.deviceName),
			                vk::apiVersionMajor(props.apiVersion),
			                vk::apiVersionMinor(props.apiVersion),
			                vk::apiVersionPatch(props.apiVersion));
		}
	}

	for (const auto& device : devices)
	{ // Grab device with the provided name
		if (std::string_view(device.getProperties().deviceName.data()) == requestedDeviceName)
		{
			m_physicalDevice = device;
			break;
		}
	}

	if (*m_physicalDevice == nullptr)
	{
		if constexpr (svk::enableValidationLayers)
		{ // In debug, if not found, pick the first one
			m_physicalDevice = devices.front();
			m_logger.cWarn("Requested device '{}' was not found; falling back to '{}'.",
			               requestedDeviceName,
			               std::string(m_physicalDevice.getProperties().deviceName));
		}
		else // In release, we throw an error
		{
			throw std::runtime_error(std::string("Could not find requested device: '") + requestedDeviceName + "'");
		}
	}

	if constexpr (svk::enableValidationLayers)
	{
		m_logger.cInfo("Selected device: '{}'", std::string(m_physicalDevice.getProperties().deviceName));
	}

	// Ensure the selected device supports Vulkan 1.3
	if (m_physicalDevice.getProperties().apiVersion < vk::ApiVersion13)
	{
		throw std::runtime_error("Selected device does not support Vulkan 1.3");
	}
}

void svk::Device::pickComputeAndTransferQueues(uint32_t& outCompute, uint32_t& outTransfer) const
{
	const auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();

	outCompute = kUnassignedQueueFamily;
	outTransfer = kUnassignedQueueFamily;

	// 1. Prefer a dedicated compute queue (compute without graphics)
	for (uint32_t i = 0; i < queueFamilies.size(); ++i)
	{
		const auto flags = queueFamilies[i].queueFlags;
		const bool supportsCompute = (flags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{};
		const bool supportsGraphics = (flags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
		if (supportsCompute && !supportsGraphics)
		{
			outCompute = i;
			break;
		}
	}

	// 2. Fallback: any queue family supporting compute
	if (outCompute == kUnassignedQueueFamily)
	{
		for (uint32_t i = 0; i < queueFamilies.size(); ++i)
		{
			if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{})
			{
				outCompute = i;
				break;
			}
		}
	}

	if (outCompute == kUnassignedQueueFamily) { throw std::runtime_error("Failed to find compute queue family"); }

	// 3. Prefer a dedicated transfer queue (transfer without graphics or compute)
	for (uint32_t i = 0; i < queueFamilies.size(); ++i)
	{
		const auto flags = queueFamilies[i].queueFlags;
		const bool supportsTransfer = (flags & vk::QueueFlagBits::eTransfer) != vk::QueueFlags{};
		const bool supportsGraphics = (flags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
		const bool supportsCompute = (flags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{};
		if (supportsTransfer && !supportsGraphics && !supportsCompute)
		{
			outTransfer = i;
			break;
		}
	}

	// 4. Fallback: reuse compute queue (which implicitly supports transfer operations per Vulkan spec)
	if (outTransfer == kUnassignedQueueFamily) { outTransfer = outCompute; }
}

void svk::Device::pickGraphicsAndPresentQueues(const vk::raii::SurfaceKHR& surface,
                                               uint32_t& outGraphics,
                                               uint32_t& outPresent) const
{
	const auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();

	outGraphics = kUnassignedQueueFamily;
	outPresent = kUnassignedQueueFamily;

	// 1. Prefer a queue family that supports both graphics and present
	for (uint32_t i = 0; i < queueFamilies.size(); ++i)
	{
		const bool supportsGraphics = (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
		const bool supportsPresent = m_physicalDevice.getSurfaceSupportKHR(i, surface) == vk::True;
		if (supportsGraphics && supportsPresent)
		{
			outGraphics = outPresent = i;
			break;
		}
	}

	// 2. Fallback: separate graphics queue
	if (outGraphics == kUnassignedQueueFamily)
	{
		for (uint32_t i = 0; i < queueFamilies.size(); ++i)
		{
			if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{})
			{
				outGraphics = i;
				break;
			}
		}
	}

	// 3. Fallback: separate present queue
	if (outPresent == kUnassignedQueueFamily)
	{
		for (uint32_t i = 0; i < queueFamilies.size(); ++i)
		{
			if (m_physicalDevice.getSurfaceSupportKHR(i, surface) == vk::True)
			{
				outPresent = i;
				break;
			}
		}
	}

	if (outGraphics == kUnassignedQueueFamily) { throw std::runtime_error("Failed to find graphics queue family"); }
	if (outPresent == kUnassignedQueueFamily)
	{
		throw std::runtime_error("Failed to find queue family that can present to surface");
	}
}

svk::Buffer svk::Device::createBuffer(vk::DeviceSize size,
                                      vk::BufferUsageFlags usage,
                                      vk::MemoryPropertyFlags properties,
                                      std::span<const QueueType> targetQueues) const
{
	logAllocationPressure();

	if ((usage & vk::BufferUsageFlagBits::eUniformBuffer) != vk::BufferUsageFlags{})
	{
		const vk::DeviceSize supportedLimit = m_physicalDevice.getProperties().limits.maxUniformBufferRange;
		if (size > supportedLimit)
		{
			throw std::runtime_error(std::string("Requested UBO size (") + std::to_string(size) +
			                         ") exceeds device maxUniformBufferRange (" + std::to_string(supportedLimit) + ")");
		}
		if (size > kConventionalUboLimit)
		{
			m_logger.cWarn("Requested UBO size ({}) is larger than the conventional 64 KB budget.", size);
		}
	}

	std::vector<uint32_t> uniqueFamiliesVector;
	uniqueFamiliesVector.reserve(targetQueues.size());
	for (const QueueType queue : targetQueues)
	{
		uniqueFamiliesVector.emplace_back(m_queues[queueSlot(queue)].getFamilyIndex());
	}
	std::ranges::sort(uniqueFamiliesVector);
	const auto [first, last] = std::ranges::unique(uniqueFamiliesVector);
	uniqueFamiliesVector.erase(first, last);

	const vk::BufferCreateInfo bufferInfo{
	    .size = size,
	    .usage = usage,
	    .sharingMode = (uniqueFamiliesVector.size() > 1) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
	    .queueFamilyIndexCount =
	        static_cast<uint32_t>((uniqueFamiliesVector.size() > 1) ? uniqueFamiliesVector.size() : 0),
	    .pQueueFamilyIndices = (uniqueFamiliesVector.size() > 1) ? uniqueFamiliesVector.data() : nullptr,
	};

	vk::raii::Buffer buffer(m_device, bufferInfo);
	const auto memReq = buffer.getMemoryRequirements();
	const uint32_t memType = findMemoryType(memReq.memoryTypeBits, properties);

	const vk::MemoryAllocateInfo allocInfo{.allocationSize = memReq.size, .memoryTypeIndex = memType};
	vk::raii::DeviceMemory memory(m_device, allocInfo);

	return svk::Buffer(std::move(buffer), std::move(memory), size, usage, allocationCount);
}

svk::Image svk::Device::createImage(const vk::ImageCreateInfo& imageInfo,
                                    vk::MemoryPropertyFlags properties,
                                    vk::ImageAspectFlags aspectFlags) const
{
	logAllocationPressure();

	vk::raii::Image image(m_device, imageInfo);
	const auto memReq = image.getMemoryRequirements();
	const uint32_t memType = findMemoryType(memReq.memoryTypeBits, properties);

	const vk::MemoryAllocateInfo allocInfo{.allocationSize = memReq.size, .memoryTypeIndex = memType};
	vk::raii::DeviceMemory memory(m_device, allocInfo);

	return svk::Image(m_device,
	                  std::move(image),
	                  std::move(memory),
	                  imageInfo.format,
	                  imageInfo.extent,
	                  imageInfo.usage,
	                  aspectFlags,
	                  allocationCount);
}

uint32_t svk::Device::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
	const auto memProperties = m_physicalDevice.getMemoryProperties();
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (((typeFilter & (1u << i)) != 0u) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	throw std::runtime_error("Failed to find suitable memory type");
}

void svk::Device::initialize(const svk::Instance& instance,
                             const vk::raii::SurfaceKHR* surface,
                             const std::string& deviceName)
{
	selectPhysicalDevice(instance.getInstance(), deviceName);

	const auto props = m_physicalDevice.getProperties();
	if (props.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
	{
		m_logger.cWarn("Selected device '{}' is NOT a discrete GPU.", deviceName);
	}

	// Initialize queue family indices as unset
	uint32_t transferQueueIndex = kUnassignedQueueFamily;
	uint32_t computeQueueIndex = kUnassignedQueueFamily;
	uint32_t graphicsQueueIndex = kUnassignedQueueFamily;
	uint32_t presentQueueIndex = kUnassignedQueueFamily;

	// Pick queue families
	pickComputeAndTransferQueues(computeQueueIndex, transferQueueIndex);
	if (surface != nullptr) { pickGraphicsAndPresentQueues(*surface, graphicsQueueIndex, presentQueueIndex); }

	// Collect unique queue families and prepare queue create infos
	std::vector<uint32_t> uniqueQueueFamilies{transferQueueIndex, computeQueueIndex};
	if (surface != nullptr)
	{
		uniqueQueueFamilies.push_back(graphicsQueueIndex);
		uniqueQueueFamilies.push_back(presentQueueIndex);
	}
	std::ranges::sort(uniqueQueueFamilies);
	const auto [first, last] = std::ranges::unique(uniqueQueueFamilies);
	uniqueQueueFamilies.erase(first, last);

	// Prepare queue create infos
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.reserve(uniqueQueueFamilies.size());
	for (const auto queueFamily : uniqueQueueFamilies)
	{
		queueCreateInfos.emplace_back(vk::DeviceQueueCreateInfo{
		    .queueFamilyIndex = queueFamily,
		    .queueCount = 1,
		    .pQueuePriorities = &queuePriority,
		});
	}

	// Prepare required device extensions
	std::vector<const char*> requiredDeviceExtensions;
	if (surface != nullptr) { requiredDeviceExtensions.emplace_back(vk::KHRSwapchainExtensionName); }
	// Check availability of required extensions
	const auto availableExtensions = m_physicalDevice.enumerateDeviceExtensionProperties();
	std::unordered_set<std::string_view> availableExtensionSet;
	availableExtensionSet.reserve(availableExtensions.size());
	for (const auto& ext : availableExtensions) { availableExtensionSet.emplace(ext.extensionName.data()); }
	// In debug log available extensions
	if constexpr (svk::enableValidationLayers)
	{
		m_logger.cDebug("Available device extensions ({}):", availableExtensions.size());
		for (const auto& ext : availableExtensions)
		{
			m_logger.cDebug("  - {}", std::string_view(ext.extensionName.data()));
		}
	}
	// Check if all required extensions are available
	for (const auto& requiredExtension : requiredDeviceExtensions)
	{
		if (!availableExtensionSet.contains(requiredExtension))
		{
			throw std::runtime_error(std::string("Required device extension not supported: ") + requiredExtension);
		}
	}

	// --- 1. Query & Verify Required Hardware Features ---
	const bool enableGraphics = (surface != nullptr);

	const auto supportedFeatures = m_physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2,
	                                                             vk::PhysicalDeviceVulkan11Features,
	                                                             vk::PhysicalDeviceVulkan13Features>();
	const auto& supported10 = supportedFeatures.get<vk::PhysicalDeviceFeatures2>().features;
	const auto& supported11 = supportedFeatures.get<vk::PhysicalDeviceVulkan11Features>();
	const auto& supported13 = supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();

	if (supported13.synchronization2 != vk::True)
	{
		throw std::runtime_error("Selected device does not support required Vulkan 1.3 feature: synchronization2");
	}
	if (enableGraphics)
	{
		if (supported13.dynamicRendering != vk::True)
		{
			throw std::runtime_error("Selected device does not support required Vulkan 1.3 feature: dynamicRendering");
		}
		if (supported11.shaderDrawParameters != vk::True)
		{
			throw std::runtime_error(
			    "Selected device does not support required Vulkan 1.1 feature: shaderDrawParameters");
		}
		if (supported10.largePoints != vk::True)
		{
			throw std::runtime_error("Selected device does not support required Vulkan 1.0 feature: largePoints");
		}
	}

	// --- 2. Feature Chain (Safe, automatic pNext linking) ---
	const vk::StructureChain<vk::PhysicalDeviceFeatures2,
	                         vk::PhysicalDeviceVulkan11Features,
	                         vk::PhysicalDeviceVulkan13Features>
	    featureChain = {
	        // Core Features 1.0
	        {.features = {.largePoints = enableGraphics ? vk::True : vk::False}},
	        // Vulkan 1.1 Features (required by Slang SV_VertexID / SV_InstanceID -> SpvCapabilityDrawParameters)
	        {.shaderDrawParameters = enableGraphics ? vk::True : vk::False},
	        // Vulkan 1.3 Features
	        {.synchronization2 = vk::True, .dynamicRendering = enableGraphics ? vk::True : vk::False},
	    };

	const vk::DeviceCreateInfo deviceCreateInfo{
	    .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
	    .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
	    .pQueueCreateInfos = queueCreateInfos.data(),
	    .enabledLayerCount = 0,
	    .ppEnabledLayerNames = nullptr,
	    .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
	    .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
	};

	m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);

	// Build queue-family -> slot mapping and initialize each slot once.
	// --- Queue Mapping ---
	std::size_t currentSlot = 0;
	// Iterate over the unique queue families we already built earlier in the function
	for (const uint32_t family : uniqueQueueFamilies)
	{
		// Boot the laboratory in the contiguous array
		m_queues[currentSlot].init(m_device.getQueue(family, 0), family);

		// Route the semantic types to this specific slot
		if (family == transferQueueIndex) { m_queueMapping[TRANSFER] = currentSlot; }
		if (family == computeQueueIndex) { m_queueMapping[COMPUTE] = currentSlot; }
		if (surface != nullptr)
		{
			if (family == graphicsQueueIndex) { m_queueMapping[GRAPHICS] = currentSlot; }
			if (family == presentQueueIndex) { m_queueMapping[PRESENT] = currentSlot; }
		}

		currentSlot++;
	}

	if constexpr (svk::enableValidationLayers)
	{
		if (surface != nullptr)
		{
			m_logger.cInfo("Queue mapping (slot->family): transfer={} compute={} graphics={} present={}",
			               m_queues[m_queueMapping[TRANSFER]].getFamilyIndex(),
			               m_queues[m_queueMapping[COMPUTE]].getFamilyIndex(),
			               m_queues[m_queueMapping[GRAPHICS]].getFamilyIndex(),
			               m_queues[m_queueMapping[PRESENT]].getFamilyIndex());
		}
		else
		{
			m_logger.cInfo("Queue mapping (slot->family, headless): transfer={} compute={}",
			               m_queues[m_queueMapping[TRANSFER]].getFamilyIndex(),
			               m_queues[m_queueMapping[COMPUTE]].getFamilyIndex());
		}
	}
}