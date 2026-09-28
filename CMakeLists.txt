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

# --- Find or Fetch Slang Compiler ---
#
# Pinned to v2026.18.3 — this version is known to work with this project.
# A newer version is very likely fine, but is NOT guaranteed to be compatible.
# To upgrade: change SLANG_TAG and SLANG_VER below and test a full build + run.
#
# If slangc is already on PATH (e.g. system or custom install), it will be
# used directly and the download is skipped.

find_program(SLANGC_EXECUTABLE slangc)

if(NOT SLANGC_EXECUTABLE)
    set(SLANG_TAG "v2026.18.3")    # GitHub release tag  (has the v prefix)
    set(SLANG_VER "2026.18.3")     # Archive filename version (no v prefix)
    message(STATUS "slangc not found on PATH — fetching Slang ${SLANG_TAG}")

    # Archive filename format: slang-<ver>-<os>-<arch>.tar.gz  (no v in filename)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(SLANG_ARCHIVE "slang-${SLANG_VER}-linux-x86_64.tar.gz")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(SLANG_ARCHIVE "slang-${SLANG_VER}-windows-x86_64.zip")
    else()
        message(FATAL_ERROR "Unsupported platform for Slang auto-download: ${CMAKE_SYSTEM_NAME}")
    endif()

    FetchContent_Declare(slang
        URL "https://github.com/shader-slang/slang/releases/download/${SLANG_TAG}/${SLANG_ARCHIVE}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(slang)

    # The extracted binary lives at <base>/slang-src/bin/slangc (Linux)
    find_program(SLANGC_EXECUTABLE slangc
        HINTS "${slang_SOURCE_DIR}/bin"
        NO_DEFAULT_PATH
    )
    if(NOT SLANGC_EXECUTABLE)
        message(FATAL_ERROR "Slang downloaded but slangc binary not found in ${slang_SOURCE_DIR}/bin.\n"
                            "Check the archive layout for Slang ${SLANG_TAG}.")
    endif()
    message(STATUS "Found Slang compiler (fetched): ${SLANGC_EXECUTABLE}")
else()
    message(STATUS "Found Slang compiler (system): ${SLANGC_EXECUTABLE}")
endif()


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
