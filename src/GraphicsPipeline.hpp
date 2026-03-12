#pragma once
class VulkanDevice;

class GraphicsPipeline
{
public:

	inline const vk::raii::Pipeline& getPipeline() const { return m_pipeline; }
	inline const vk::raii::PipelineLayout& getLayout() const { return m_pipelineLayout; }
	inline const vk::raii::DescriptorSetLayout& getDescriptorSetLayout() const { return m_descriptorSetLayout; }
protected:
	// Nested Config Struct (Protected: only subclasses need this)
	struct Config
	{
		// 1. Shader (Combined Vert/Frag)
		vk::ShaderModuleCreateInfo shaderInfo;
		
		// 2. Vertex Input State
		vk::PipelineVertexInputStateCreateInfo vertexInputState;

		// 3. Fixed Function Options
		vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
		vk::CullModeFlagBits cullMode = vk::CullModeFlagBits::eBack;
	};

	// Protected Constructor
	GraphicsPipeline(
			const VulkanDevice& device,
			const Config& config,
			vk::Format colorFormat,
			vk::Format depthFormat
	);

	vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout m_pipelineLayout = nullptr;
	vk::raii::Pipeline m_pipeline = nullptr;
};

// --- Subclass: Earth Mesh ---
class MeshPipeline : public GraphicsPipeline
{
public:
	MeshPipeline(const VulkanDevice& device, vk::Format cFmt, vk::Format dFmt);
	~MeshPipeline();
};

// --- Subclass: Satellite Wireframes ---
class SatellitePipeline : public GraphicsPipeline
{
public:
	SatellitePipeline(const VulkanDevice& device, vk::Format cFmt, vk::Format dFmt);
	~SatellitePipeline();
};
