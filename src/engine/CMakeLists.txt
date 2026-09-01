# src/engine/CMakeLists.txt
# Engine configuration: Vulkan, GLFW, GLM, PCH headers

# ============================================================================
# Engine Dependencies
# ============================================================================

# === Vulkan SDK ===
# Vulkan is found at root level (system package) and available to all projects
message(STATUS "Engine: Vulkan ${Vulkan_VERSION} available (found at root)")

# === GLM Math Library ===
if(NOT glm_POPULATED)
	include(FetchContent)
	FetchContent_Declare(
		glm
		GIT_REPOSITORY https://github.com/g-truc/glm.git
		GIT_TAG        1.0.3
	)
	FetchContent_MakeAvailable(glm)
	message(STATUS "Engine: GLM 1.0.3 fetched")
endif()

# === Vulkan.hpp C++ Bindings ===
set(VULKAN_HPP_GIT_TAG "v${Vulkan_VERSION}")
if(NOT vulkan-hpp_POPULATED)
	include(FetchContent)
	FetchContent_Declare(
		vulkan-hpp
		GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
		GIT_TAG        ${VULKAN_HPP_GIT_TAG}
	)
	FetchContent_MakeAvailable(vulkan-hpp)
	message(STATUS "Engine: vulkan-hpp ${VULKAN_HPP_GIT_TAG} fetched")
endif()

# === GLFW Window System (Abstract Layer) ===
# To migrate to SDL: just replace find_package(glfw3) with SDL2
find_package(glfw3 3.4 REQUIRED)
message(STATUS "Engine: GLFW 3.4+ found (window abstraction)")

# ============================================================================
# SVK Engine PCH Headers
# ============================================================================

# === SVK Engine PCH Headers ===
# These headers are included by all projects that use SVK
set(SVK_PCH_HEADERS
    # STL - Memory & Containers
    <vector>
    <array>
    <deque>
    <map>
    <unordered_map>
    <set>
    <unordered_set>
    
    # STL - Strings & Format
    <string>
    <string_view>
    <format>
    <sstream>
    
    # STL - Exception & Utility
    <stdexcept>
    <exception>
    <optional>
    <variant>
    <utility>
    <functional>
    
    # STL - Algorithms & Iterators
    <algorithm>
    <numeric>
    <ranges>
    <iterator>
    
    # STL - I/O & Files
    <iostream>
    <fstream>
    <iomanip>
    
    # STL - Threading & Synchronization
    <mutex>
    <thread>
    <atomic>
    <chrono>
    
    # STL - System & Utilities
    <cstring>
    <cstdlib>
    <cstddef>
    <cmath>
    <cassert>
    <climits>
    <limits>
    <filesystem>
    
    # Third Party - Vulkan
    <vulkan/vulkan_raii.hpp>
    
    # Third Party - Window (GLFW abstraction)
    <GLFW/glfw3.h>
    
    # Third Party - Math
    <glm/glm.hpp>
    <glm/mat4x4.hpp>
    <glm/gtc/matrix_transform.hpp>
    <glm/gtc/type_ptr.hpp>
    <glm/gtc/constants.hpp>
    <glm/gtc/packing.hpp>
    
    # SVK Engine Headers (using angle brackets so they rely on -I paths)
    <engine/Flags.hpp>
    <engine/Frames.hpp>
    <engine/Logger.hpp>
    <engine/Queue.hpp>
    <engine/Buffer.hpp>
    <engine/Command.hpp>
    <engine/Device.hpp>
    <engine/GraphicsPipeline.hpp>
    <engine/Image.hpp>
    <engine/Swapchain.hpp>
    <engine/Instance.hpp>
    <engine/RenderRoutine.hpp>
    <engine/RenderTask.hpp>
    <engine/ComputePipeline.hpp>
    <engine/ComputeRoutine.hpp>
    <engine/TransferRoutine.hpp>
    <engine/VulkanHelpers.hpp>
    <engine/window/WindowContext.hpp>
    <engine/window/Window.hpp>
    <engine/window/Camera.hpp>
)

# Export to parent scope so projects can use them
set(SVK_PCH_HEADERS "${SVK_PCH_HEADERS}" PARENT_SCOPE)

message(STATUS "Engine: PCH headers configured")
