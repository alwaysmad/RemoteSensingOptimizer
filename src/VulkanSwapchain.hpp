#pragma once

// Forward declarations to avoid circular includes
class VulkanDevice;
class VulkanWindow;

// The packaged slice of the current frame's resources
struct SwapchainSlice
{
	uint32_t      imageIndex;
	vk::Image     image;
	vk::ImageView imageView;
	vk::Semaphore renderFinishedSemaphore;
};

class VulkanSwapchain
{
public:
	VulkanSwapchain(const VulkanDevice&, const VulkanWindow&);
	~VulkanSwapchain();
	
	// Called when window is resized
	void recreate();
	
	// --- Presentation API ---
	// Returns nullopt if the swapchain is out of date and needs recreation
	std::optional<SwapchainSlice> acquireNextImage(vk::Semaphore imageAvailableSemaphore);

	// Returns false if the swapchain became suboptimal or out of date during present
	bool present(const vk::raii::Queue& presentQueue, const SwapchainSlice& slice);

	inline vk::Format getImageFormat() const { return m_imageFormat; }
	inline vk::Extent2D getExtent() const { return m_extent; }
private:
	// We keep references to dependencies so we can recreate() later
	const VulkanDevice& m_device;
	const VulkanWindow& m_window;

	vk::raii::SwapchainKHR m_swapchain = nullptr;

	// Swapchain resources
	std::vector<vk::Image> m_images; // Handles (owned by swapchain)
	std::vector<vk::raii::ImageView> m_imageViews; // Wrappers (owned by us)
	std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;

	vk::Format m_imageFormat;
	vk::Extent2D m_extent;

	// Internal helpers
	void createSwapchain();
	void createImageViews();
	void createSyncObjects();
};
