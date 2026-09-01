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

# --- Find Vulkan SDK (system package, needed by all projects) ---
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan SDK ${Vulkan_VERSION} found")

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
# Engine is added FIRST because it owns Vulkan, GLM, vulkan-hpp dependencies
# Other projects reference these via the engine subdir
add_subdirectory(src/engine)
add_subdirectory(src/rso)
add_subdirectory(src/triangle)
