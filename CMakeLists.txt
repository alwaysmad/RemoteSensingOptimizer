cmake_minimum_required(VERSION 3.16)

# Set the base directory for FetchContent BEFORE project()
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/_deps CACHE PATH "Base directory for FetchContent")

project(RemoteSensingOptimizer LANGUAGES CXX)

# ============================================================================
# Global Build Configuration
# ============================================================================
include(cmake/build_config.cmake)

# ============================================================================
# Global CMake Includes
# ============================================================================
include(FetchContent)
include(ExternalProject)

# --- Pre-fetch Global Dependencies (needed by multiple projects) ---
# GLM is used by engine and all projects
FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)
message(STATUS "GLM 1.0.3 fetched and available")

# --- Find Vulkan SDK (needed by engine) ---
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan SDK ${Vulkan_VERSION} found")

# --- Fetch Vulkan.hpp C++ Bindings (needed by engine) ---
set(VULKAN_HPP_GIT_TAG "v${Vulkan_VERSION}")
FetchContent_Declare(
	vulkan-hpp
	GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
	GIT_TAG        ${VULKAN_HPP_GIT_TAG}
)
FetchContent_MakeAvailable(vulkan-hpp)
message(STATUS "vulkan-hpp ${VULKAN_HPP_GIT_TAG} fetched and available")

# --- Find Slang Compiler ---
find_program(SLANGC_EXECUTABLE slangc)
if(NOT SLANGC_EXECUTABLE)
	message(FATAL_ERROR "slangc not found! Please install Slang and add it to your PATH.")
endif()
message(STATUS "Found Slang compiler: ${SLANGC_EXECUTABLE}")

# ============================================================================
# Common Build Helpers
# ============================================================================
include(cmake/svk_common.cmake)

# ============================================================================
# Add Subdirectories (Projects)
# ============================================================================
add_subdirectory(src/engine)
add_subdirectory(src/rso)
add_subdirectory(src/triangle)
