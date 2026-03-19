#include "Renderer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanWindow.hpp"

#include "Mesh.hpp"
#include "Satellite.hpp"

Renderer::Renderer(const VulkanDevice& device, const VulkanWindow& window, const SatelliteNetwork& satNet) :
	m_device(device), 
	m_command(device, device.getGraphicsQueueIndex()),
	m_swapchain(device, window),
	// Find Depth Format
	m_depthFormat(
		device.findSupportedFormat(
			{vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment )
		),
	// Create pipeline with that depth format
	m_meshPipeline(device, m_swapchain.getImageFormat(), m_depthFormat),
	m_satellitePipeline(device, m_swapchain.getImageFormat(), m_depthFormat)
{
	// 1. Create Per-Frame Sync Objects (Image Available)
	constexpr vk::SemaphoreCreateInfo semaphoreInfo{};

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{ m_imageAvailableSemaphores.emplace_back(m_device.device(), semaphoreInfo); }

	// 2. Create Per-Image Sync Objects (Render Finished)
	m_renderFinishedSemaphores.reserve(m_swapchain.getImages().size());
	remakeRenderFinishedSemaphores();
	
	createDepthBuffer();

	updateProjectionMatrix();

	createDescriptors(satNet);

	LOG_DEBUG("Renderer initialized");
}

Renderer::~Renderer() { LOG_DEBUG("Renderer destroyed"); }

void Renderer::createDescriptors(const SatelliteNetwork& satNet)
{
	// Create Pool
	static constexpr vk::DescriptorPoolSize poolSize { vk::DescriptorType::eUniformBuffer, 1 };
	constexpr vk::DescriptorPoolCreateInfo poolInfo {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize
	};
	m_descriptorPool = vk::raii::DescriptorPool(m_device.device(), poolInfo);

	// Allocate Set (Using Satellite Pipeline Layout)
	const vk::DescriptorSetLayout dsl = *m_satellitePipeline.getDescriptorSetLayout();
	const vk::DescriptorSetAllocateInfo allocInfo {
		.descriptorPool = *m_descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &dsl
	};
	m_satelliteDescriptors = vk::raii::DescriptorSets(m_device.device(), allocInfo);

	// Update Set
	const vk::DescriptorBufferInfo bufInfo {
		.buffer = *satNet.getBuffer(),
		.offset = 0,
		.range = satNet.getFrameSize()
	};
	const vk::WriteDescriptorSet write {
		.dstSet = *m_satelliteDescriptors[0],
		.dstBinding = 0, .descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.pBufferInfo = &bufInfo
	};
	m_device.device().updateDescriptorSets(write, nullptr);
}

void Renderer::createDepthBuffer()
{
	const auto extent = m_swapchain.getExtent();

	const vk::ImageCreateInfo imageInfo {
		.imageType = vk::ImageType::e2D,
		.format = m_depthFormat,
		.extent = { extent.width, extent.height, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined
	};

	auto [img, mem] = m_device.createImage(imageInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);
	m_depthImage = std::move(img);
	m_depthMemory = std::move(mem);

	const vk::ImageViewCreateInfo viewInfo {
		.image = *m_depthImage,
		.viewType = vk::ImageViewType::e2D,
		.format = m_depthFormat,
		.subresourceRange = {
		.aspectMask = vk::ImageAspectFlagBits::eDepth,
		.baseMipLevel = 0, .levelCount = 1,
		.baseArrayLayer = 0, .layerCount = 1
		}
	};
	m_depthView = vk::raii::ImageView(m_device.device(), viewInfo);
}

void Renderer::remakeRenderFinishedSemaphores()
{
	const uint32_t imageCount = m_swapchain.getImages().size();
	// Re-create semaphores if image count changed
	if (m_renderFinishedSemaphores.size() != imageCount)
	{
		m_renderFinishedSemaphores.clear();
		constexpr vk::SemaphoreCreateInfo semaphoreInfo{};
		for (uint32_t i = 0; i < imageCount; ++i)
			{ m_renderFinishedSemaphores.emplace_back(m_device.device(), semaphoreInfo); }
	}
}

void Renderer::recreateSwapchain()
{
	m_swapchain.recreate();
	remakeRenderFinishedSemaphores();
	createDepthBuffer();
	updateProjectionMatrix();
}

void Renderer::updateProjectionMatrix()
{
	const auto extent = m_swapchain.getExtent();
	const auto min = static_cast<float>( std::min(extent.width, extent.height) );

	// MANUAL PROJECTION MATRIX CONSTRUCTION
	// FOV: 45 degrees
	// Near: 1.0f
	// Far: Infinity
	// Z-Range: [0, 1] (Standard Vulkan)
	
	// f = 1.0 / tan(fov / 2)
	// For 45 deg, tan(22.5) approx 0.4142 -> f approx 2.4142
	constexpr float f = 2.41421356f;
	constexpr float near = 0.1f;

	constexpr glm::mat4 hardcodedProj = {
		f,	  0.0f,	0.0f,	0.0f,	// Col 0 (Right)
		0.0f,	  -f,	0.0f,	0.0f,	// Col 1 (Up-ish)
		0.0f,	  0.0f,	-1.0f,	-1.0f,	// Col 2 (Forward-ish)
		0.0f,     0.0f,	-near,	0.0f	// Col 3 (Translation)
	};

	m_proj = hardcodedProj;
	m_proj[0][0] *= min / static_cast<float>(extent.width);
	m_proj[1][1] *= min / static_cast<float>(extent.height);
}

void Renderer::submitDummy(vk::Fence fence, vk::Semaphore waitSemaphore)
{
	// must reset the fence before submitting
	m_device.device().resetFences({fence});

	constexpr vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAllCommands;
	
	const vk::SubmitInfo submitInfo {
		// If waitSemaphore exists, wait for it.
		.waitSemaphoreCount = waitSemaphore ? 1u : 0u,
		.pWaitSemaphores = waitSemaphore ? &waitSemaphore : nullptr,
		.pWaitDstStageMask = waitSemaphore ? &waitStage : nullptr,
		.commandBufferCount = 0, 
		.signalSemaphoreCount = 0
	};
	
	// Must signal 'fence' so the CPU knows this "frame" is done
	m_device.graphicsQueue().submit(submitInfo, fence);
}

// --- NEW: Bake Mesh (Secondary) ---
void Renderer::bakeMeshTask(const vk::raii::CommandBuffer& cmd, const Mesh& mesh, const glm::mat4& model, const glm::mat4& view)
{
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_meshPipeline.getPipeline());

	// Push Constants
	m_pc.viewProj = PackedHalfMat4(m_proj * view);
	m_pc.model = model; 
	cmd.pushConstants<CameraPushConstants>(*m_meshPipeline.getLayout(), vk::ShaderStageFlagBits::eVertex, 0, m_pc);

	// Bind Geometry & Draw
	cmd.bindVertexBuffers(0, {*mesh.getVertexBuffer()}, {0});
	cmd.bindIndexBuffer(*mesh.getIndexBuffer(), 0, vk::IndexType::eUint32);
	cmd.drawIndexed(static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
}

// --- NEW: Bake Satellite (Secondary) ---
void Renderer::bakeSatelliteTask(const vk::raii::CommandBuffer& cmd, const SatelliteNetwork& satNet, const glm::mat4& view)
{
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_satellitePipeline.getPipeline());

	// Push Constants
	m_pc.viewProj = PackedHalfMat4(m_proj * view);
	cmd.pushConstants<PackedHalfMat4>(*m_satellitePipeline.getLayout(), vk::ShaderStageFlagBits::eVertex, 0, m_pc.viewProj);

	// Bind Descriptors & Draw
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *m_satellitePipeline.getLayout(), 0, {*m_satelliteDescriptors[0]}, nullptr);
	cmd.draw(32, static_cast<uint32_t>(satNet.satellites.size()), 0, 0);
}

void Renderer::draw( const Mesh& mesh,
		     const SatelliteNetwork& satNet,
		     uint32_t currentFrame,
		     vk::Fence fence,
		     vk::Semaphore waitSemaphore,
		     const glm::mat4& modelMatrix,
		     const glm::mat4& viewMatrix )
{
	// 0. Check for Minimization
	const auto extent = m_swapchain.getExtent();
	if (extent.width <= 1 || extent.height <= 1)
		{ submitDummy(fence, waitSemaphore); return; }

	// 1. Acquire Image
	// Waits for 'imgSem' to be signaled when the presentation engine releases an image
	const auto& imgSem = m_imageAvailableSemaphores[currentFrame];
	uint32_t imageIndex;
	try {
		const auto result = m_device.device().acquireNextImage2KHR({
			.swapchain = *m_swapchain.getSwapchain(),
			.timeout = UINT64_MAX,
			.semaphore = *imgSem,
			.deviceMask = 1
		});
		imageIndex = result.second;
	} catch (const vk::OutOfDateKHRError&)
		{ recreateSwapchain(); submitDummy(fence, waitSemaphore); return; }

	// 2. Reset Fence
	// We are about to submit work that will signal the fence.
	// We must reset it before submission.
	m_device.device().resetFences({fence});

	// 3.1 Record
	const auto& cmd = m_command.getBuffer(currentFrame);
	const auto& swapchainImageView = m_swapchain.getImageViews()[imageIndex];
	const auto& swapchainImage = m_swapchain.getImages()[imageIndex];

	// --- 3.2. Record Primary (The Glue) ---
	cmd.reset();
	cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	// --- BARRIERS ---
	const vk::ImageMemoryBarrier2 barriers[] = { 
		// Barrier: Undefined -> Color Attachment
		{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = swapchainImage,
			.subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
		},
		// DEPTH BARRIER (Undefined -> DepthAttachment)
		{
			.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.image = *m_depthImage,
			.subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eDepth,
				.baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
		}
	};
	cmd.pipelineBarrier2({ .imageMemoryBarrierCount = 2, .pImageMemoryBarriers = barriers });
	
	// --- ATTACHMENTS ---
	// Color Attachment
	constexpr vk::ClearValue clearColor { .color = { backgroundColor } };
	const vk::RenderingAttachmentInfo colorAttachment {
		.imageView = swapchainImageView,
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = clearColor
	};
	// Depth Attachment
	const vk::RenderingAttachmentInfo depthAttachment {
		.imageView = *m_depthView,
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = vk::ClearValue{ .depthStencil = { 1.0f, 0 } } // Clear to 1.0 (Far)
	};
	
	const vk::RenderingInfo renderInfo {
		.renderArea = { .extent = extent },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment
	};

	// --- 4.1. START RENDERING ---
	cmd.beginRendering(renderInfo);

	// --- 4.2. SET GLOBAL DYNAMIC STATE ONCE ---
	const vk::Viewport vp { .width = (float)extent.width, .height = (float)extent.height, .maxDepth = 1.0f };
	cmd.setViewport(0, vp);

	const vk::Rect2D scissor { .extent = extent };
	cmd.setScissor(0, scissor);

	// --- 4.3. EXECUTE INLINE ---
	bakeMeshTask(cmd, mesh, modelMatrix, viewMatrix);
	bakeSatelliteTask(cmd, satNet, viewMatrix);

	// --- 4.4. END RENDERING ---
	cmd.endRendering();

	// --- BARRIER (PRESENT) ---
	// Barrier: Color Attachment -> Present
	const vk::ImageMemoryBarrier2 postRenderBarrier {
		// We must wait for rendering to finish
		.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
		
		// BottomOfPipe is correct here: we just need to finish before the batch ends.
		.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
		.dstAccessMask = vk::AccessFlagBits2::eNone, 
		
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::ePresentSrcKHR,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = swapchainImage,
		.subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor, 
			.baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
	};
	cmd.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &postRenderBarrier });

	cmd.end();

	// 5. Submit
	// Signals 'renderSem' when rendering finishes, so Present can start.
	vk::Semaphore renderSem = *m_renderFinishedSemaphores[imageIndex]; // extract handle

	// Define stages statically (Fixed mapping: Index 0 = Color, Index 1 = Vertex)
	constexpr std::array<vk::PipelineStageFlags, 2> waitStages = {
		vk::PipelineStageFlagBits::eColorAttachmentOutput,
		vk::PipelineStageFlagBits::eVertexInput
	};

	// If waitSemaphore is null, it sits harmlessly at index 1 because waitCount will be 1.
	const std::array<vk::Semaphore, 2> waitSems = { *imgSem, waitSemaphore };

	const vk::SubmitInfo submitInfo {
		.waitSemaphoreCount = waitSemaphore ? 2u : 1u,
		.pWaitSemaphores = waitSems.data(),
		.pWaitDstStageMask = waitStages.data(),
		.commandBufferCount = 1, .pCommandBuffers = &*cmd,
		.signalSemaphoreCount = 1, .pSignalSemaphores = &renderSem
	};

	// Submit to queue.
	// - Waits on 'waitSems'
	// - Signals 'renderSem'
	// - Signals 'fence' when ALL work is done (for CPU sync)
	m_device.graphicsQueue().submit(submitInfo, fence);

	// 6. Present
	try {
		const auto result = m_device.presentQueue().presentKHR({
			// COMMENT: Wait for Rendering to finish before showing image
			.waitSemaphoreCount = 1, .pWaitSemaphores = &renderSem,
			.swapchainCount = 1, .pSwapchains = &*m_swapchain.getSwapchain(),
			.pImageIndices = &imageIndex
		});
		if (result == vk::Result::eSuboptimalKHR) { recreateSwapchain(); }
	} catch (const vk::OutOfDateKHRError&)
		{ recreateSwapchain(); }
}
