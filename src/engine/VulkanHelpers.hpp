#pragma once

#include <span>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace svk
{

[[nodiscard]] inline std::vector<vk::DescriptorPoolSize> deduceDescriptorPoolSizes(
    std::span<const vk::DescriptorSetLayoutBinding> bindings)
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.reserve(bindings.size());

    // We use a flat vector linear search instead of std::map because the number
    // of unique descriptor types in a single layout is typically < 5.
    // A linear search over contiguous memory utterly destroys std::map here.
    for (const vk::DescriptorSetLayoutBinding& binding : bindings)
    {
        bool merged = false;
        for (vk::DescriptorPoolSize& poolSize : poolSizes)
        {
            if (poolSize.type == binding.descriptorType)
            {
                poolSize.descriptorCount += binding.descriptorCount;
                merged = true;
                break;
            }
        }

        if (!merged)
        {
            poolSizes.emplace_back(vk::DescriptorPoolSize {
                .type = binding.descriptorType,
                .descriptorCount = binding.descriptorCount,
            });
        }
    }

    return poolSizes;
}

} // namespace svk
