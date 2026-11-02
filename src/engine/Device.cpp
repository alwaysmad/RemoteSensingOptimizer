// src/engine/Device.cpp
#include <algorithm>      // std::ranges::sort, std::ranges::unique
#include <span>           // std::span
#include <stdexcept>      // std::runtime_error
#include <string>         // std::string
#include <string_view>    // std::string_view
#include <unordered_set>  // std::unordered_set
#include <vector>         // queue create infos

#include "engine/Flags.hpp"
#include "engine/Device.hpp"
#include "engine/Instance.hpp"
#include "engine/ConsoleLogger.hpp"


void svk::Device::logAllocationPressure(const svk::ConsoleLogger& logger, uint32_t currentCount)
{
    if (currentCount >= kAllocationHardWarning)
    {
        logger.cWarn("[WARNING] Device allocation count is {} (very close to Vulkan limit {}).",
            currentCount, kVulkanRecommendedMaxAllocations);
    }
    else if (currentCount >= kAllocationNearWarning)
    {
        logger.cWarn("[WARNING] Device allocation count is {} (nearing Vulkan limit {}).",
            currentCount, kVulkanRecommendedMaxAllocations);
    }
}

void svk::Device::selectPhysicalDevice(
    const vk::raii::Instance& instance,
    const std::string& requestedDeviceName,
    vk::raii::PhysicalDevice& outPhysicalDevice,
    const svk::ConsoleLogger& logger)
{
    // Find and pick physical device based on name and Vulkan 1.3 support
    const auto devices = instance.enumeratePhysicalDevices();

    if (devices.empty())
        { throw std::runtime_error("Failed to find any device with Vulkan support"); }

    // Log all available devices if debug
    if constexpr (svk::enableValidationLayers)
    {
        logger.cDebug("Available physical devices ({}):", devices.size());
        for (const auto& device : devices)
        {
            const auto props = device.getProperties();
            logger.cDebug("  - {} (api {}.{}.{})",
                std::string(props.deviceName),
                vk::apiVersionMajor(props.apiVersion),
                vk::apiVersionMinor(props.apiVersion),
                vk::apiVersionPatch(props.apiVersion));
        }
    }

    for (const auto& device : devices)
    { // Grab device with the provided name
        if (std::string_view(device.getProperties().deviceName.data()) == requestedDeviceName)
            { outPhysicalDevice = device; break; }
    }

    if (*outPhysicalDevice == nullptr)
    {
        if constexpr (svk::enableValidationLayers)
        { // In debug, if not found, pick the first one
            outPhysicalDevice = devices.front();
            logger.cWarn("Requested device '{}' was not found; falling back to '{}'.",
                requestedDeviceName,
                std::string(outPhysicalDevice.getProperties().deviceName));
        }
        else // In release, we throw an error
            { throw std::runtime_error(std::string("Could not find requested device: '") + requestedDeviceName + "'"); }
    }

    if constexpr (svk::enableValidationLayers)
        { logger.cInfo("Selected device: '{}'", std::string(outPhysicalDevice.getProperties().deviceName)); }

    // Ensure the selected device supports Vulkan 1.3
    if (outPhysicalDevice.getProperties().apiVersion < vk::ApiVersion13)
        { throw std::runtime_error("Selected device does not support Vulkan 1.3"); }

}

void svk::Device::pickHeadlessQueueFamilies(
    const vk::raii::PhysicalDevice& physicalDevice,
    uint32_t& outGraphics,
    uint32_t& outPresent,
    uint32_t& outCompute,
    uint32_t& outTransfer)
{
    const auto queueFamilies = physicalDevice.getQueueFamilyProperties();

    outGraphics = outPresent = outCompute = outTransfer = std::numeric_limits<uint32_t>::max();

    // Headless mode should never depend on surface-backed queue capabilities.
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        const bool supportsCompute = (queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{};
        if (supportsCompute && i != outGraphics) { outCompute = i; break; }
    }

    // Fallback: if no dedicated compute queue, use the graphics queue
    if (outCompute == std::numeric_limits<uint32_t>::max())
    {
        if (outGraphics != std::numeric_limits<uint32_t>::max()) { outCompute = outGraphics; }
        else
        { // or find any compute queue if graphics is also unavailable / not picked
            for (uint32_t i = 0; i < queueFamilies.size(); ++i)
            {
                if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{})
                    { outCompute = i; break; }
            }
        }
    }

    // Find Transfer (Dedicated if possible, or reuse compute/graphics)
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        const auto flags = queueFamilies[i].queueFlags;
        const bool transfer = (flags & vk::QueueFlagBits::eTransfer) != vk::QueueFlags{};
        const bool graphics = (flags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
        const bool compute = (flags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{};
        if (transfer && !graphics && !compute) { outTransfer = i; break; }
    }

    // Fallback for transfer queue: prefer compute, then graphics
    if (outTransfer == std::numeric_limits<uint32_t>::max() && outCompute != std::numeric_limits<uint32_t>::max())
        { outTransfer = outCompute; }
    if (outTransfer == std::numeric_limits<uint32_t>::max())
        { outTransfer = outGraphics; }

    // Check if we found all required queues
    if (outCompute == std::numeric_limits<uint32_t>::max())
        { throw std::runtime_error("Failed to find compute queue family"); }
    if (outTransfer == std::numeric_limits<uint32_t>::max())
        { throw std::runtime_error("Failed to find transfer queue family"); }

    // Route non-surface types to a valid queue for safety.
    outGraphics = outCompute;
    outPresent = outCompute;
}

void svk::Device::pickSurfaceQueueFamilies(
    const vk::raii::PhysicalDevice& physicalDevice,
    const vk::raii::SurfaceKHR& surface,
    uint32_t& outGraphics,
    uint32_t& outPresent,
    uint32_t& outCompute,
    uint32_t& outTransfer)
{
    const auto queueFamilies = physicalDevice.getQueueFamilyProperties();

    outGraphics = outPresent = outCompute = outTransfer = std::numeric_limits<uint32_t>::max();

    // 1. try to find a queue family that supports both graphics and present
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        const bool supportsGraphics = (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
        const bool supportsPresent = physicalDevice.getSurfaceSupportKHR(i, surface) == vk::True;
        if (supportsGraphics && supportsPresent) { outGraphics = outPresent = i; break; }
    }

    // 2. if not found, pick a dedicated graphics queue
    if (outGraphics == std::numeric_limits<uint32_t>::max() )
    {
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{})
                { outGraphics = i; break; }
        }
    }

    // 3. if not found, pick a dedicated present queue
    if (outPresent == std::numeric_limits<uint32_t>::max())
    {
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            if (physicalDevice.getSurfaceSupportKHR(i, surface) == vk::True)
                { outPresent = i; break; }
        }
    }

    // 4. try to find a dedicated compute queue
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        const bool supportsCompute = (queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{};
        if (supportsCompute && i != outGraphics) { outCompute = i; break; }
    }

    // 5. Fallback: if no dedicated compute queue, try the graphics queue
    if (outCompute == std::numeric_limits<uint32_t>::max())
    {
        if (outGraphics != std::numeric_limits<uint32_t>::max()) { outCompute = outGraphics; }
        else
        {
            for (uint32_t i = 0; i < queueFamilies.size(); ++i)
            {
                if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{})
                    { outCompute = i; break; }
            }
        }
    }

    // 6. try to find a dedicated transfer queue
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        const auto flags = queueFamilies[i].queueFlags;
        const bool transfer = (flags & vk::QueueFlagBits::eTransfer) != vk::QueueFlags{};
        const bool graphics = (flags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
        const bool compute = (flags & vk::QueueFlagBits::eCompute) != vk::QueueFlags{};
        if (transfer && !graphics && !compute)
            { outTransfer = i; break; }
    }

    // 7. Fallback for transfer queue: prefer compute, then graphics
    if (outTransfer == std::numeric_limits<uint32_t>::max() && outCompute != std::numeric_limits<uint32_t>::max())
        { outTransfer = outCompute; }
    if (outTransfer == std::numeric_limits<uint32_t>::max())
        { outTransfer = outGraphics; }

    // 8. Check that all queues were found
    if (outGraphics == std::numeric_limits<uint32_t>::max())
        { throw std::runtime_error("Failed to find graphics queue family"); }
    if (outPresent == std::numeric_limits<uint32_t>::max())
        { throw std::runtime_error("Failed to find queue family that can present to surface"); }
    if (outCompute == std::numeric_limits<uint32_t>::max())
        { throw std::runtime_error("Failed to find compute queue family"); }
    if (outTransfer == std::numeric_limits<uint32_t>::max())
        { throw std::runtime_error("Failed to find transfer queue family"); }
}

svk::Buffer svk::Device::createBuffer(vk::DeviceSize size,
                                 vk::BufferUsageFlags usage,
                                 vk::MemoryPropertyFlags properties,
                                 std::span<const QueueType> targetQueues) const
{
    logAllocationPressure(m_logger, allocationCount.load(std::memory_order_relaxed));

    if ((usage & vk::BufferUsageFlagBits::eUniformBuffer) != vk::BufferUsageFlags{})
    {
        const vk::DeviceSize supportedLimit = m_physicalDevice.getProperties().limits.maxUniformBufferRange;
        if (size > supportedLimit)
        {
            throw std::runtime_error(
                std::string("Requested UBO size (") + std::to_string(size) +
                ") exceeds device maxUniformBufferRange (" + std::to_string(supportedLimit) + ")");
        }
        if (size > kConventionalUboLimit)
            { m_logger.cWarn("Requested UBO size ({}) is larger than the conventional 64 KB budget.", size); }
    }

    std::vector<uint32_t> uniqueFamiliesVector;
    uniqueFamiliesVector.reserve(targetQueues.size());
    for (const QueueType queue : targetQueues)
    {
        const uint32_t queueSlot = m_queueMapping[queue];
        uniqueFamiliesVector.emplace_back(m_queues[queueSlot].getFamilyIndex());
    }
    std::ranges::sort(uniqueFamiliesVector);
    const auto [first, last] = std::ranges::unique(uniqueFamiliesVector);
    uniqueFamiliesVector.erase(first, last);
    
    const vk::BufferCreateInfo bufferInfo {
        .size = size,
        .usage = usage,
        .sharingMode = (uniqueFamiliesVector.size() > 1) ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = static_cast<uint32_t>( (uniqueFamiliesVector.size() > 1) ? uniqueFamiliesVector.size() : 0),
        .pQueueFamilyIndices = (uniqueFamiliesVector.size() > 1) ? uniqueFamiliesVector.data() : nullptr,
    };

    vk::raii::Buffer buffer(m_device, bufferInfo);
    const auto memReq = buffer.getMemoryRequirements();
    const uint32_t memType = findMemoryType(memReq.memoryTypeBits, properties);

    const vk::MemoryAllocateInfo allocInfo{         
        .allocationSize = memReq.size, 
        .memoryTypeIndex = memType              
    };                                              
    vk::raii::DeviceMemory memory(m_device, allocInfo);

    return svk::Buffer(std::move(buffer), std::move(memory), size, usage, allocationCount);
}

svk::Image svk::Device::createImage(const vk::ImageCreateInfo& imageInfo,
                              vk::MemoryPropertyFlags properties,
                              vk::ImageAspectFlags aspectFlags) const
{
    logAllocationPressure(m_logger, allocationCount.load(std::memory_order_relaxed));

    vk::raii::Image image(m_device, imageInfo);
    const auto memReq = image.getMemoryRequirements();
    const uint32_t memType = findMemoryType(memReq.memoryTypeBits, properties);

    const vk::MemoryAllocateInfo allocInfo{         
        .allocationSize = memReq.size, 
        .memoryTypeIndex = memType              
    };
    vk::raii::DeviceMemory memory(m_device, allocInfo);

    return svk::Image(
        m_device,
        std::move(image),
        std::move(memory),
        imageInfo.format,
        imageInfo.extent,
        imageInfo.usage,
        aspectFlags,
        allocationCount );
}

uint32_t svk::Device::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
    const auto memProperties = m_physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if (((typeFilter & (1u << i)) != 0u) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            { return i; }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

void svk::Device::initialize(const svk::Instance& instance, const vk::raii::SurfaceKHR* surface, const std::string& deviceName)
{
    selectPhysicalDevice(instance.getInstance(), deviceName, m_physicalDevice, m_logger);

    const auto props = m_physicalDevice.getProperties();
    if (props.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
        { m_logger.cWarn("Selected device '{}' is NOT a discrete GPU.", deviceName); }

    // Initialize queue family indices as unset
    uint32_t graphicsQueueIndex = std::numeric_limits<uint32_t>::max();
    uint32_t presentQueueIndex = std::numeric_limits<uint32_t>::max();
    uint32_t computeQueueIndex = std::numeric_limits<uint32_t>::max();
    uint32_t transferQueueIndex = std::numeric_limits<uint32_t>::max();
    // Pick queue families
    if (surface != nullptr)
    {
        pickSurfaceQueueFamilies(
            m_physicalDevice,
            *surface,
            graphicsQueueIndex,
            presentQueueIndex,
            computeQueueIndex,
            transferQueueIndex);
    }
    else
    {
        pickHeadlessQueueFamilies(
            m_physicalDevice,
            graphicsQueueIndex,
            presentQueueIndex,
            computeQueueIndex,
            transferQueueIndex);
    }

    // Collect unique queue families and prepare queue create infos
    std::vector<uint32_t> uniqueQueueFamilies {
        transferQueueIndex, computeQueueIndex, graphicsQueueIndex, presentQueueIndex };
    std::ranges::sort(uniqueQueueFamilies);
    const auto [first, last] = std::ranges::unique(uniqueQueueFamilies);
    uniqueQueueFamilies.erase(first, last);
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

    // Prepare required device extensions
    std::vector<const char*> requiredDeviceExtensions;
    if (surface != nullptr)
        { requiredDeviceExtensions.emplace_back(vk::KHRSwapchainExtensionName); }
    // Check availability of required extensions
    const auto availableExtensions = m_physicalDevice.enumerateDeviceExtensionProperties();
    std::unordered_set<std::string_view> availableExtensionSet;
    availableExtensionSet.reserve(availableExtensions.size());
    for (const auto& ext : availableExtensions)
        { availableExtensionSet.emplace(ext.extensionName.data()); }

    if constexpr (svk::enableValidationLayers)
    {
        m_logger.cDebug("Available device extensions ({}):", availableExtensions.size());
        for (const auto& ext : availableExtensions)
            { m_logger.cDebug("  - {}", std::string_view(ext.extensionName.data())); }
    }

    for (const auto& requiredExtension : requiredDeviceExtensions)
    {
        if (!availableExtensionSet.contains(requiredExtension))
            { throw std::runtime_error(std::string("Required device extension not supported: ") + requiredExtension); }
    }

    // --- 1. Feature Chain (Safe, automatic pNext linking) ---
    const vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    > featureChain = {
        // Core Features 1.0 (vk::PhysicalDeviceFeatures2)
        { 
            .features = { 
                .largePoints = vk::True,
                .samplerAnisotropy = vk::True, 
                .shaderInt16 = vk::True 
            } 
        },
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
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
    };

    m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);

    // Build queue-family -> slot mapping and initialize each slot once.
    // --- Queue Mapping ---
    uint32_t currentSlot = 0;
    // Iterate over the unique queue families we already built earlier in the function
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

    if constexpr (svk::enableValidationLayers)
    {
        m_logger.cInfo(
            "Queue mapping (slot->family): transfer={} compute={} graphics={} present={}",
            m_queues[m_queueMapping[TRANSFER]].getFamilyIndex(),
            m_queues[m_queueMapping[COMPUTE]].getFamilyIndex(),
            m_queues[m_queueMapping[GRAPHICS]].getFamilyIndex(),
            m_queues[m_queueMapping[PRESENT]].getFamilyIndex());
    }
}