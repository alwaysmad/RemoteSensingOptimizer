// src/engine/ComputeRoutine.hpp

#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "engine/Buffer.hpp"
#include "engine/Command.hpp"
#include "engine/ComputePipeline.hpp"
#include "engine/Device.hpp"
#include "engine/Queue.hpp"

namespace svk
{
struct BufferClear;
class ComputeRoutine;
} // namespace svk

// =========================================================================
//  Descriptor for a buffer clear / fill operation prior to compute dispatch
// =========================================================================
struct svk::BufferClear
{
	vk::Buffer buffer = nullptr;
	vk::DeviceSize offset = 0;
	vk::DeviceSize size = vk::WholeSize;
	uint32_t data = 0;

	BufferClear() = default;
	BufferClear(vk::Buffer buf, vk::DeviceSize off = 0, vk::DeviceSize sz = vk::WholeSize, uint32_t val = 0)
	    : buffer(buf), offset(off), size(sz), data(val)
	{}
	BufferClear(const svk::Buffer& buf, vk::DeviceSize off = 0, vk::DeviceSize sz = vk::WholeSize, uint32_t val = 0)
	    : buffer(*buf.getBuffer()), offset(off), size(sz), data(val)
	{}
	BufferClear(const svk::BufferBinding& binding,
	            vk::DeviceSize off = 0,
	            vk::DeviceSize sz = vk::WholeSize,
	            uint32_t val = 0)
	    : buffer(binding.buffer), offset(off), size(sz), data(val)
	{}
};

// =========================================================================
//  "The Compute Wizard"
//  Commands pure mathematical dispatch operations on the Compute Queue.
// =========================================================================
class svk::ComputeRoutine
{
private:
	const svk::Device* m_device = nullptr;
	const svk::Queue* m_computeQueue = nullptr;

	// The Contract and Memory
	svk::ComputePipeline m_pipeline;
	vk::raii::DescriptorPool m_descriptorPool = nullptr;
	vk::raii::DescriptorSets m_descriptorSets = nullptr;
	std::vector<std::vector<BufferClear>> m_clearBuffers;

	// The Spellbook
	svk::Command m_command;

public:
	// Ironclad Constraints
	ComputeRoutine(const ComputeRoutine&) = delete;
	ComputeRoutine& operator=(const ComputeRoutine&) = delete;
	ComputeRoutine(ComputeRoutine&&) noexcept = default;
	ComputeRoutine& operator=(ComputeRoutine&&) noexcept = default;

	ComputeRoutine(const svk::Device& device,
	               const vk::PipelineShaderStageCreateInfo& shaderStage,
	               std::span<const vk::DescriptorSetLayoutBinding> descriptorBindings,
	               uint32_t bufferCount);

	ComputeRoutine(const svk::Device& device,
	               const vk::PipelineShaderStageCreateInfo& shaderStage,
	               std::initializer_list<vk::DescriptorSetLayoutBinding> descriptorBindings,
	               uint32_t bufferCount);

	~ComputeRoutine() = default;

	// Binds buffer memory and optional clear buffers to the specified Descriptor Set index
	void registerBuffers(uint32_t setIndex,
	                     std::span<const svk::BufferBinding> descriptorBindings,
	                     std::span<const BufferClear> clearBuffers = {});

	void registerBuffers(uint32_t setIndex,
	                     std::initializer_list<svk::BufferBinding> descriptorBindings,
	                     std::span<const BufferClear> clearBuffers = {});

	void registerBuffers(uint32_t setIndex,
	                     std::span<const svk::BufferBinding> descriptorBindings,
	                     std::initializer_list<BufferClear> clearBuffers);

	void registerBuffers(uint32_t setIndex,
	                     std::initializer_list<svk::BufferBinding> descriptorBindings,
	                     std::initializer_list<BufferClear> clearBuffers);

	// Prepare the spell
	void bakeCommands(uint32_t cmdIndex,
	                  vk::CommandBufferUsageFlags usage,
	                  uint32_t groupCountX,
	                  uint32_t groupCountY = 1,
	                  uint32_t groupCountZ = 1) const;

	// Cast the spell
	void submitCommands(uint32_t cmdIndex,
	                    vk::Semaphore waitSemaphore = nullptr,
	                    vk::Semaphore signalSemaphore = nullptr,
	                    vk::Fence fence = nullptr) const;
};
