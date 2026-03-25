// src/engine/Device.cpp
#include <algorithm> // std::ranges::none_of
#include <array>     // required extension list
#include <cstring>   // std::strcmp
#include <format>    // std::format
#include <set>       // queue family dedupe
#include <stdexcept> // std::runtime_error
#include <vector>    // queue create infos

#include "engine/Flags.hpp"
#include "engine/Device.hpp"
#include "engine/Instance.hpp"

namespace
{
constexpr float queuePriority = 1.0f;

inline void selectPhysicalDevice(
    const vk::raii::Instance& instance,
    const std::string& requestedDeviceName,
    vk::raii::PhysicalDevice& outPhysicalDevice)
{
    // Find and pick physical device based on name and Vulkan 1.3 support
    const auto devices = instance.enumeratePhysicalDevices();

    if (devices.empty())
        { throw std::runtime_error("Failed to find any device with Vulkan support"); }

    for (const auto& device : devices)
    { // Grab device with the provided name
        if (std::strcmp(device.getProperties().deviceName, requestedDeviceName.c_str()) == 0)
            { outPhysicalDevice = device; break; }
    }

    if (*outPhysicalDevice == nullptr)
    { // If not found, pick the first one
        if constexpr (svk::enableValidationLayers)
            { outPhysicalDevice = devices.front(); }
        else // or throw an error
            { throw std::runtime_error(std::format("Could not find requested device: '{}'", requestedDeviceName)); }
    }

    if (outPhysicalDevice.getProperties().apiVersion < vk::ApiVersion13)
        { throw std::runtime_error("Selected device does not support Vulkan 1.3"); }
}

inline void pickQueueFamilies(
    const vk::raii::PhysicalDevice& physicalDevice,
    const vk::raii::SurfaceKHR* surface,
    uint32_t& outGraphics,
    uint32_t& outPresent,
    uint32_t& outCompute,
    uint32_t& outTransfer)
{
    const auto queueFamilies = physicalDevice.getQueueFamilyProperties();

    outGraphics = UINT32_MAX;
    outPresent = UINT32_MAX;
    outCompute = UINT32_MAX;
    outTransfer = UINT32_MAX;

    if (surface != nullptr)
    {
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        { // 1. Try to find a queue family that supports BOTH Graphics and Present
            const bool supportsGraphics = (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags {};
            const bool supportsPresent = physicalDevice.getSurfaceSupportKHR(i, **surface);
            if (supportsGraphics && supportsPresent)
                { outGraphics = outPresent = i; break; }
        }

        // 2. If we didn't find a unified one, fallback to separate queues
        if (outGraphics == UINT32_MAX)
        { // Find any graphics queue
            for (uint32_t i = 0; i < queueFamilies.size(); ++i)
            {
                if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags {})
                    { outGraphics = i; break; }
            }
        }
        if (outPresent == UINT32_MAX)
        { // Find any present queue
            for (uint32_t i = 0; i < queueFamilies.size(); ++i)
            {
                if (physicalDevice.getSurfaceSupportKHR(i, **surface))
                    { outPresent = i; break; }
            }
        }
    }

    // 3. Find Compute (Dedicated if possible, or reuse graphics)
    // Some vendors have a dedicated compute queue (Async Compute) which is faster.
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        const bool supportsCompute = (queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) != vk::QueueFlags {};
        if (supportsCompute && i != outGraphics)
            { outCompute = i; break; }
    }

    // Fallback: if no dedicated compute queue, use the graphics queue
    if (outCompute == UINT32_MAX)
    {
        if (outGraphics != UINT32_MAX)
            { outCompute = outGraphics; }
        else
        { // or find any compute queue if graphics is also unavailable / not picked
            for (uint32_t i = 0; i < queueFamilies.size(); ++i)
            {
                if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) != vk::QueueFlags {})
                    { outCompute = i; break; }
            }
        }
    }

    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    { // 4. Find Transfer (Dedicated if possible, or reuse compute/graphics)
        const auto flags = queueFamilies[i].queueFlags;
        const bool transfer = (flags & vk::QueueFlagBits::eTransfer) != vk::QueueFlags {};
        const bool graphics = (flags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags {};
        const bool compute = (flags & vk::QueueFlagBits::eCompute) != vk::QueueFlags {};
        if (transfer && !graphics && !compute)
            { outTransfer = i; break; }
    }

    // Fallbacks for transfer queue: prefer compute, then graphics
    if (outTransfer == UINT32_MAX && outCompute != UINT32_MAX)
        { outTransfer = outCompute; }
    if (outTransfer == UINT32_MAX)
        { outTransfer = outGraphics; }

    if (surface != nullptr)
    {
        if (outGraphics == UINT32_MAX)
            { throw std::runtime_error("Failed to find graphics queue family"); }
        if (outPresent == UINT32_MAX)
            { throw std::runtime_error("Failed to find queue family that can present to surface"); }
    } else
    {
        if (outCompute == UINT32_MAX)
            { throw std::runtime_error("Failed to find compute queue family"); }
        // Route non-surface types to a valid queue for safety.
        outGraphics = outCompute;
        outPresent = outCompute;
    }

    if (outCompute == UINT32_MAX)
        { throw std::runtime_error("Failed to find compute queue family"); }
    if (outTransfer == UINT32_MAX)
        { throw std::runtime_error("Failed to find transfer queue family"); }
}

} // namespace

namespace svk
{

Device::Device(const svk::Instance& instance, const std::string& deviceName)
    : m_queues { svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device) }
{
    initialize(instance, nullptr, deviceName);
}

Device::Device(const svk::Instance& instance, const vk::raii::SurfaceKHR& surface, const std::string& deviceName)
    : m_queues { svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device), svk::Queue(m_device) }
{
    initialize(instance, &surface, deviceName);
}

void Device::initialize(const svk::Instance& instance, const vk::raii::SurfaceKHR* surface, const std::string& deviceName)
{
    selectPhysicalDevice(instance.getInstance(), deviceName, m_physicalDevice);

    uint32_t graphicsQueueIndex = UINT32_MAX;
    uint32_t presentQueueIndex = UINT32_MAX;
    uint32_t computeQueueIndex = UINT32_MAX;
    uint32_t transferQueueIndex = UINT32_MAX;

    pickQueueFamilies(
        m_physicalDevice,
        surface,
        graphicsQueueIndex,
        presentQueueIndex,
        computeQueueIndex,
        transferQueueIndex);

    std::set<uint32_t> uniqueQueueFamilies {
        transferQueueIndex,
        computeQueueIndex,
        graphicsQueueIndex,
        presentQueueIndex
    };

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    for (const auto queueFamily : uniqueQueueFamilies)
    {
        queueCreateInfos.emplace_back(
            vk::DeviceQueueCreateInfo {
                .queueFamilyIndex = queueFamily,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority,
            });
    }

    std::vector<const char*> requiredDeviceExtensions;
    if (surface != nullptr)
        { requiredDeviceExtensions.push_back(vk::KHRSwapchainExtensionName); }

    const auto availableExtensions = m_physicalDevice.enumerateDeviceExtensionProperties();
    for (const auto requiredExtension : requiredDeviceExtensions) {
        if (std::ranges::none_of(
                availableExtensions,
                [requiredExtension](const auto& extensionProperty) {
                    return std::strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                }))
            { throw std::runtime_error(std::format("Required device extension not supported: {}", requiredExtension)); }
    }

    // --- 1. Feature Chain (Safe, automatic pNext linking) ---
    const vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    > featureChain = {
        // Core Features 1.0
        { .features = { .samplerAnisotropy = vk::True, .shaderInt16 = vk::True } },
        // Vulkan 1.1 Features
        {
            .storageBuffer16BitAccess = vk::True,
            .uniformAndStorageBuffer16BitAccess = vk::True,
            .storagePushConstant16 = vk::True,
            .shaderDrawParameters = vk::True
        },
        // Vulkan 1.2 Features
        { .shaderFloat16 = vk::True },
        // Vulkan 1.3 Features
        { .synchronization2 = vk::True, .dynamicRendering = vk::True },
        // Extended Dynamic State
        { .extendedDynamicState = vk::True }
    };

    const vk::DeviceCreateInfo deviceCreateInfo {
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
    };

    m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);

    // Build queue-family -> slot mapping and initialize each slot once.
    // --- Queue Mapping ---
    uint32_t currentSlot = 0;
    // Iterate over the std::set we already built earlier in the function
    for (const uint32_t family : uniqueQueueFamilies)
    {
        // Boot the laboratory in the contiguous array
        m_queues[currentSlot].init(m_device.getQueue(family, 0), family);

        // Route the semantic types to this specific slot
        if (family == transferQueueIndex) { m_queueMapping[TRANSFER] = currentSlot; }
        if (family == computeQueueIndex)  { m_queueMapping[COMPUTE]  = currentSlot; }
        if (family == graphicsQueueIndex) { m_queueMapping[GRAPHICS] = currentSlot; }
        if (family == presentQueueIndex)  { m_queueMapping[PRESENT]  = currentSlot; }

        currentSlot++;
    }
}

void Device::waitIdle()
{
    // Unconditionally lock all 4 laboratories instantly to prevent deadlocks. 
    // This ensures no thread is mid-submission before we halt the GPU.
    std::scoped_lock lock(
        m_queues[0].m_mutex, 
        m_queues[1].m_mutex, 
        m_queues[2].m_mutex, 
        m_queues[3].m_mutex
    );
    m_device.waitIdle();
}

} // namespace svk
