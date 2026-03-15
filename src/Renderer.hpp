#pragma once
#include "VulkanSwapchain.hpp"
#include "GraphicsPipeline.hpp"
#include "VulkanCommand.hpp" 

#include "PushConstants.hpp"

#include <optional>

class VulkanDevice;
class VulkanWindow;

class Mesh;
class SatelliteNetwork;

class DrawTask {
private:
	vk::raii::CommandPool pool;
	std::vector<vk::raii::CommandBuffer> cmds; 
public:
	DrawTask(const VulkanDevice& device, uint32_t queueFamily) 
	: pool(device.device(), { 
		// FIX 1: Add eResetCommandBuffer
		.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer, 
		.queueFamilyIndex = queueFamily 
	})
	{
		// FIX 3: Allocate 2 buffers (MAX_FRAMES_IN_FLIGHT)
		vk::CommandBufferAllocateInfo allocInfo {
			.commandPool = *pool,
			.level = vk::CommandBufferLevel::eSecondary,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT
		};
		// allocateCommandBuffers returns a vector, so we just move it.
		cmds = vk::raii::CommandBuffers(device.device(), allocInfo);
	}

	// Helper to get the current frame's buffer
	inline const vk::raii::CommandBuffer& get(uint32_t frame) { return cmds[frame]; }
};

class Renderer
{
public:
	Renderer(const VulkanDevice& device, const VulkanWindow& window, const SatelliteNetwork& satNet);
	~Renderer();

	// - Returns void. The App controls the frame loop.
	// - Guarantees 'fence' is signaled, even if drawing fails/skips.
	//
	// SYNC OBJECTS:
	// - fence (Mandatory): 
	//     Signaled by GPU when this frame's submission completes. 
	//     Used by CPU (App) to wait before re-using resources (like CommandBuffers).
	//
	// - waitSemaphore (Optional): 
	//     If provided (not null), the Graphics Queue will wait for this semaphore 
	//     (at Vertex Input stage) before processing. Used to sync Compute -> Graphics.
	void draw(const Mesh& mesh,
		  const SatelliteNetwork& satNet,
		  uint32_t currentFrame,
		  vk::Fence fence,
		  vk::Semaphore waitSemaphore = {},
		  const glm::mat4& ModelMatrix = defaultModel,
		  const glm::mat4& viewMatrix = defaultView );

private:
	// Push constant members and logic
	glm::mat4 m_proj;
	void updateProjectionMatrix();
	CameraPushConstants m_pc; 

	const VulkanDevice& m_device;

	// Main primary command
	VulkanCommand m_command;

	// --- NEW: The Modules ---
	// These replace the monolithic recording logic
	std::optional<DrawTask> m_meshTask;
	std::optional<DrawTask> m_satelliteTask;

	// Recording functions for the modules
	void bakeMeshTask(uint32_t, const Mesh& mesh, const glm::mat4& model, const glm::mat4& view);
	void bakeSatelliteTask(uint32_t, const SatelliteNetwork& satNet, const glm::mat4& view);
		
	VulkanSwapchain m_swapchain;
	void recreateSwapchain();

	// Depth Resources
	vk::raii::Image     m_depthImage = nullptr;
	TrackedDeviceMemory m_depthMemory;
	vk::raii::ImageView m_depthView = nullptr;
	vk::Format          m_depthFormat;
	void createDepthBuffer();

	// Pipelines
	MeshPipeline m_meshPipeline;
	SatellitePipeline m_satellitePipeline;

	// Descriptors for Graphics (Satellite UBO)
	vk::raii::DescriptorPool m_descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> m_satelliteDescriptors;
	void createDescriptors(const SatelliteNetwork& satNet);

	// Sync
	// Fixed size [MAX_FRAMES_IN_FLIGHT]
	std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
	// This one must match Swapchain Image Count
	std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
	void remakeRenderFinishedSemaphores();
	
	void submitDummy(vk::Fence fence, vk::Semaphore waitSemaphore);

	// Constants and default values
	static constexpr std::array<float, 4> backgroundColor = {0.004f, 0.004f, 0.004f, 1.0f};
	static constexpr glm::mat4 defaultModel = glm::mat4(1.0f);
	// Default View: Eye(0, 1.5, 3), Center(0, 0, 0), Up(0, 1, 0)
	// Precomputed values:
	// cos(theta) = 3.0 / sqrt(1.5^2 + 3.0^2) = 0.894427
	// sin(theta) = 1.5 / sqrt(1.5^2 + 3.0^2) = 0.447214
	// Translation Z = -magnitude = -3.354102
	static constexpr glm::mat4 defaultView = {
		1.0f,       0.0f,       0.0f,       0.0f, // Col 0 (Right)
		0.0f,       0.894427f,  0.447214f,  0.0f, // Col 1 (Up-ish)
		0.0f,      -0.447214f,  0.894427f,  0.0f, // Col 2 (Forward-ish)
		0.0f,       0.0f,      -3.354102f,  1.0f  // Col 3 (Translation)
	};
};
