// src/triangle/main.cpp
// Minimal triangle example using SVK engine

#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// SVK Engine headers - order matters!
#include "engine/Logger.hpp"
#include "engine/Instance.hpp"
#include "engine/Device.hpp"
#include "engine/Swapchain.hpp"
#include "engine/Queue.hpp"
#include "engine/GraphicsPipeline.hpp"
#include "engine/Frames.hpp"
#include "engine/DepthResources.hpp"
#include "engine/RenderRoutine.hpp"
#include "engine/RenderTask.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowContext.hpp"

// Generated shader header
#include "triangle.hpp"

// Constants
constexpr uint32_t WINDOW_WIDTH = 800;
constexpr uint32_t WINDOW_HEIGHT = 600;
constexpr const char* APP_NAME = "Triangle Example";
constexpr const char* DEVICE_NAME = "Intel(R) Iris(R) Xe Graphics (ADL GT2)";

int main()
{
    try
    {
        // === 1. Setup Logger ===
        svk::Logger logger("/tmp/triangle.log");
        logger.cInfo("Triangle Example Starting...");

        // === 2. Create Vulkan Instance ===
        svk::WindowContext windowContext;
        svk::Instance instance(
            APP_NAME,
            windowContext.getRequiredInstanceExtensions(),
            logger);
        logger.cInfo("Vulkan Instance created");

        // === 3. Create Window ===
        svk::Window window(
            windowContext,
            instance.getInstance(),
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            APP_NAME);
        logger.cInfo("Window created ({}x{})", WINDOW_WIDTH, WINDOW_HEIGHT);

        // === 4. Create Device ===
        svk::Device device(
            instance,
            window.getSurface(),
            DEVICE_NAME,
            logger);
        logger.cInfo("Device created");

        // === 5. Create Swapchain ===
        svk::Swapchain swapchain(device, window);
        logger.cInfo("Swapchain created");

        // === 6. Create RenderRoutine ===
        svk::RenderRoutine renderRoutine(device, swapchain, svk::MAX_FRAMES_IN_FLIGHT);
        logger.cInfo("RenderRoutine created");
        
        { // Temporary objects to setup RenderTask
            // === 7. Load Shader Module ===
            const vk::raii::ShaderModule triangleModule(
                device.device(),
                triangle::smci);
            logger.cInfo("Triangle shader module loaded");

            // === 8. Create Graphics Pipeline & RenderTask ===
            // No vertex input (hardcoded vertices in shader)
            const vk::PipelineVertexInputStateCreateInfo vertexInput{
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = nullptr
            };

            // Shader stages: vertex + fragment (from triangle.slang)
            const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {
                vk::PipelineShaderStageCreateInfo{
                    .stage = vk::ShaderStageFlagBits::eVertex,
                    .module = *triangleModule,
                    .pName = "vertMain"
                },
                vk::PipelineShaderStageCreateInfo{
                    .stage = vk::ShaderStageFlagBits::eFragment,
                    .module = *triangleModule,
                    .pName = "fragMain"
                }
            };

            // No descriptor bindings needed for this simple example
            const std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings;

            // Emplace and activate the triangle render task
            auto& triangleTask = renderRoutine.m_tasks.emplace_back(
                device.device(),
                shaderStages,
                vertexInput,
                vk::PrimitiveTopology::eTriangleList,
                vk::CullModeFlags{},  // No culling for this simple triangle
                descriptorBindings,
                swapchain.getFormat(),
                renderRoutine.getDepthFormat());
            triangleTask.m_active = true;

            // Register buffers: no vertex buffers, no index buffer, just draw 3 vertices
            triangleTask.registerBuffers(
                std::vector<svk::BufferBinding>{},  // No descriptor bindings
                std::vector<svk::BufferBinding>{},  // No vertex buffers
                std::nullopt,                        // No index buffer
                3,                                   // Vertex count (triangle)
                1);                                  // Instance count
        }
        logger.cInfo("Triangle RenderTask created and registered");

        // === 9. Setup frame synchronization ===
        std::vector<vk::raii::Fence> inFlightFences;
        for (uint32_t i = 0; i < svk::MAX_FRAMES_IN_FLIGHT; ++i)
        {
            inFlightFences.emplace_back(device.device(),
                vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
        }
        logger.cInfo("Frame synchronization primitives created");

        // === 10. Main Render Loop ===
        uint32_t currentFrame = 0;

        while (!window.shouldClose())
        {
            window.pollEvents();
            window.updateFPS(APP_NAME);

            // Wait for fence
            vk::Fence fence = *inFlightFences[currentFrame];
            if (device.device().waitForFences({fence}, vk::True, UINT64_MAX) != vk::Result::eSuccess)
                { throw std::runtime_error("Failed to wait for fence"); }
            device.device().resetFences({fence});

            // Execute render routine (handles acquire, record, submit, present)
            renderRoutine.draw(currentFrame, fence);

            // Advance to next frame
            currentFrame = svk::advanceFrame(currentFrame);
        }

        logger.cInfo("Render loop ended");

        // === Cleanup (RAII handles destructions automatically) ===
        device.waitIdle();
        logger.cInfo("Triangle Example ended successfully");

        return EXIT_SUCCESS;
    }
	catch ( const std::exception& e )
	{
		std::cerr << svk::Logger::COLOR_RED << "Error: " << e.what() << svk::Logger::COLOR_RESET << std::endl;
	}
	catch ( ... )
	{
		std::cerr << svk::Logger::COLOR_RED << "An unknown error occurred." << svk::Logger::COLOR_RESET << std::endl;
	}
	return EXIT_FAILURE; // If error occured 
}
