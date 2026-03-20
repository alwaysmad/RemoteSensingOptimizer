#include "VulkanSwapchain.hpp"
#include "VulkanDevice.hpp"
#include "VulkanWindow.hpp"

static inline vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats)
{
	for (const auto& availableFormat : formats)
	{
		// pick the good format
		if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
				availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			return availableFormat;
	}
	// or pick just the first one
	return formats[0];
}

static inline vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes)
{
	for (const auto& availablePresentMode : presentModes)
	{
		// Choose triple buffering if available
		if (availablePresentMode == vk::PresentModeKHR::eMailbox)
			return availablePresentMode;
	}
	// Otherwise pick V-sync as it's guaranteed to be available
	return vk::PresentModeKHR::eFifo;
}

static inline vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps, const VulkanWindow& window)
{
	if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
		return caps.currentExtent;
	
	vk::Extent2D actualExtent = window.getExtent();
	
	actualExtent.width = std::clamp(actualExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	
	return actualExtent;
}

VulkanSwapchain::VulkanSwapchain(const VulkanDevice& device, const VulkanWindow& window) :
	m_device(device),
	m_window(window),
	m_swapchain(nullptr)
{
	recreate();
	LOG_DEBUG("VulkanSwapchain instance created");
}

VulkanSwapchain::~VulkanSwapchain()
	{ LOG_DEBUG("VulkanSwapchain instance destroyed"); }

void VulkanSwapchain::createSyncObjects()
{
	// Recreate semaphores to perfectly match the new image count
	m_renderFinishedSemaphores.clear();
	constexpr vk::SemaphoreCreateInfo semaphoreInfo{};
	for (size_t i = 0; i < m_images.size(); ++i)
		{ m_renderFinishedSemaphores.emplace_back(m_device.device(), semaphoreInfo); }
}

// --- Acquire Logic ---
std::optional<SwapchainSlice> VulkanSwapchain::acquireNextImage(vk::Semaphore imageAvailableSemaphore)
{
	try {
		const auto result = m_device.device().acquireNextImage2KHR({
			.swapchain = *m_swapchain,
			.timeout = UINT64_MAX,
			.semaphore = imageAvailableSemaphore,
			.deviceMask = 1
		});
		
		const uint32_t idx = result.second;
		
		return SwapchainSlice {
			.imageIndex = idx,
			.image = m_images[idx],
			.imageView = *m_imageViews[idx],
			.renderFinishedSemaphore = *m_renderFinishedSemaphores[idx]
		};
	} 
	// Cleanly signal to the caller that acquisition failed
	catch (const vk::OutOfDateKHRError&)
	{
		recreate();
		return std::nullopt;
	}
}

// --- NEW: PRESENT ---
bool VulkanSwapchain::present(const vk::raii::Queue& presentQueue, const SwapchainSlice& slice)
{
	try {
		const auto result = presentQueue.presentKHR({
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &slice.renderFinishedSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &*m_swapchain,
			.pImageIndices = &slice.imageIndex
		});

		// Return true if perfectly optimal, false if the window shifted
		return (result != vk::Result::eSuboptimalKHR);
	} 
	catch (const vk::OutOfDateKHRError&) {
		recreate();
		return false;
	}
}

void VulkanSwapchain::recreate()
{
	vk::Extent2D extent = m_window.getExtent();

	// PRAGMATIC FIX: Blame the Window Manager.
	// We simply block the thread until the window provides a usable size.
	// This avoids the "Resize Storm" crash and 0-sized surface errors.
	while (extent.width <= 1 || extent.height <= 1)
	{
		m_window.waitEvents(); // Blocks CPU until OS sends an event
		extent = m_window.getExtent();
	}

	m_device.device().waitIdle(); 

	// RAII handles destruction of old m_swapchain and m_imageViews here
	createSwapchain();
	createImageViews();
	createSyncObjects();
	LOG_DEBUG("Swapchain recreated");
}

void VulkanSwapchain::createSwapchain()
{
	// 1. Query Support
	const auto& physicalDevice = m_device.physicalDevice();
	const auto& surface = m_window.getSurface();

	const auto caps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	const auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
	const auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

	// 2. Choose Settings
	const auto surfaceFormat = chooseSwapSurfaceFormat(formats);
	const auto presentMode = chooseSwapPresentMode(presentModes);
	const auto extent = chooseSwapExtent(caps, m_window);

	////////////////////////////////////////////////////////////////////////////////
	// Logging details of swap chain support
	////////////////////////////////////////////////////////////////////////////////
	if constexpr (enableValidationLayers)
	{
		LOG_DEBUG("Surface Capabilities:");
		LOG_DEBUG("\tMin Image Count: " << caps.minImageCount);
		LOG_DEBUG("\tMax Image Count: " << caps.maxImageCount 
			<< (caps.maxImageCount == 0 ? " (unlimited)" : ""));
		
		LOG_DEBUG("\tCurrent Extent: " << caps.currentExtent.width 
			<< "x" << caps.currentExtent.height);
		
		LOG_DEBUG("\tMin Extent: " << caps.minImageExtent.width 
			<< "x" << caps.minImageExtent.height);
		
		LOG_DEBUG("\tMax Extent: " << caps.maxImageExtent.width 
			<< "x" << caps.maxImageExtent.height);

		LOG_DEBUG("\tMax Image Array Layers: " << caps.maxImageArrayLayers);
		LOG_DEBUG("\tSupported Transforms: " << to_string(caps.supportedTransforms));
		LOG_DEBUG("\tCurrent Transform: " << to_string(caps.currentTransform));
		LOG_DEBUG("\tSupported Composite Alpha: " << to_string(caps.supportedCompositeAlpha));
		LOG_DEBUG("\tSupported Usage Flags: " << to_string(caps.supportedUsageFlags));

		LOG_DEBUG("Available Surface Formats (" << formats.size() << "):");
		for ([[maybe_unused]] const auto& fmt : formats)
		{
			LOG_DEBUG("\tFormat: " << to_string(fmt.format) 
				<< " | ColorSpace: " << to_string(fmt.colorSpace));
		}

		LOG_DEBUG("Available Present Modes (" << presentModes.size() << "):");
		for ([[maybe_unused]] const auto& mode : presentModes)
		{
			LOG_DEBUG("\t" << to_string(mode));
		}
	}

	// 3. Image Count
	// 0 means unlimited
	const uint32_t maxImages = (caps.maxImageCount == 0) 
                         ? std::numeric_limits<uint32_t>::max() 
                         : caps.maxImageCount;
	// Target 3 images, but clamp to valid range
	const uint32_t imageCount = std::clamp(3u, caps.minImageCount, maxImages);	

	// 4. Sharing Mode (ROBUST IMPLEMENTATION)
	// We gather ALL unique queue families that might access the swapchain.
	// This ensures that Graphics, Present, AND Compute/Transfer can all use the image without barriers.
	const std::set<uint32_t> uniqueQueueFamilies = {
		m_device.getGraphicsQueueIndex(),
		m_device.getPresentQueueIndex()
	};

	std::vector<uint32_t> queueFamilyIndices(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end());

	vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
	
	if (queueFamilyIndices.size() > 1)
	{
		// If more than one distinct queue family is involved, use Concurrent.
		// This avoids the complexity of Ownership Transfer Barriers.
		sharingMode = vk::SharingMode::eConcurrent;
	}

	// 5. Create Info
	const vk::SwapchainCreateInfoKHR createInfo {
		.surface = *surface,
		.minImageCount = imageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, // Added TransferDst just in case (e.g. clearing)
		.imageSharingMode = sharingMode,
		.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size()),
		.pQueueFamilyIndices = queueFamilyIndices.data(),
		.preTransform = caps.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = presentMode,
		.clipped = vk::True,
		// We capture the handle of the current swapchain (if it exists) before we overwrite it.
		.oldSwapchain = (*m_swapchain) ? *m_swapchain : nullptr
	};

	// 6. Create
	// Note: We assign to the member variable, replacing any old one.
	m_swapchain = vk::raii::SwapchainKHR(m_device.device(), createInfo);
    
	// 7. Store attributes
	m_imageFormat = surfaceFormat.format;
	m_extent = extent;
	
	// 8. Get Images (Raw handles, not RAII)
	m_images = m_swapchain.getImages();
    
	LOG_DEBUG("Swapchain created (" << m_extent.width << "x" << m_extent.height << ")");
}

void VulkanSwapchain::createImageViews()
{
	m_imageViews.clear(); // Clear old views if recreating

	for (const auto& image : m_images)
	{
		const vk::ImageViewCreateInfo createInfo {
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = m_imageFormat,
			.components = { 
				.r = vk::ComponentSwizzle::eIdentity,
				.g = vk::ComponentSwizzle::eIdentity,
				.b = vk::ComponentSwizzle::eIdentity,
				.a = vk::ComponentSwizzle::eIdentity 
			},
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		// Emplace back constructs the RAII object in the vector
		m_imageViews.emplace_back(m_device.device(), createInfo);
	}
}
