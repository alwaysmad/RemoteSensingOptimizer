#include "engine/ComputePipeline.hpp"

namespace svk
{

ComputePipeline::ComputePipeline(
    const vk::raii::Device& device,
    const vk::PipelineShaderStageCreateInfo& computeShader,
    const std::vector<vk::DescriptorSetLayoutBinding>& descriptorBindings)
{
    // DescriptorSetLayout
    const vk::DescriptorSetLayoutCreateInfo layoutInfo {
        .bindingCount = static_cast<uint32_t>(descriptorBindings.size()),
        .pBindings = descriptorBindings.data(),
    };
    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);

    // PipelineLayout (no push constants, one descriptor set)
    const vk::DescriptorSetLayout descriptorSetLayout = *m_descriptorSetLayout;
    const vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    m_pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    // Compute Pipeline
    const vk::ComputePipelineCreateInfo pipelineInfo {
        .stage = computeShader,
        .layout = *m_pipelineLayout,
    };

    m_pipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

} // namespace svk
