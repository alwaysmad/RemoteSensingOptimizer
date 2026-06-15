cmake_minimum_required(VERSION 3.16)

# Set the base directory for FetchContent BEFORE project()
# This ensures it's set before any other paths are initialized.
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/_deps CACHE PATH "Base directory for FetchContent")

project(RemoteSensingOptimizer  LANGUAGES CXX)

# --- Check for GCC 13+ ---
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0)
		message(FATAL_ERROR "GCC version must be at least 13.0! Found: ${CMAKE_CXX_COMPILER_VERSION}. Please enable gcc-toolset-13.")
	endif()
endif()
# -------------------------

# --- Set Build Type and Flags ---
# Set a default build type if none is specified (e.g., Release)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type: Debug, Release, RelWithDebInfo, MinSizeRel")
endif()

# Set standard compiler flags for Release and Debug builds
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g")

# Print the build type being used
message(STATUS "Building project with build type: ${CMAKE_BUILD_TYPE}")
# --------------------------------

# Set the C++ standard to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Good practice

# --- Add Project-Wide Compiler Warnings ---
# These flags are great for catching bugs and ensuring code quality.
# -Wall: Enable all standard warnings
# -Wextra: Enable extra (but still useful) warnings
# -Wpedantic: Warn on non-standard C++ extensions
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Sanitizers ---
# Only active in DEBUG builds by default to avoid slowing down Release builds.
option(ENABLE_SANITIZER "Enable Address and UB Sanitizers" ON)

if(ENABLE_SANITIZER AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		message(STATUS "ASan + UBSan enabled")

		# 1. AddressSanitizer (ASan): Catches buffer overflows, use-after-free
		# 2. UndefinedBehaviorSanitizer (UBSan): Catches integer overflows, null checks
		# 3. -fno-omit-frame-pointer: Ensures nice stack traces in the report
		add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)

		# Critical: Must also LINK with these flags
		add_link_options(-fsanitize=address,undefined)
	endif()
endif()

# ------------------------------------------

# --- Set standard output directories ---
# Put executables in ${CMAKE_BINARY_DIR}/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# Put libraries in ${CMAKE_BINARY_DIR}/lib
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# ---------------------------------------

# --- Find Vulkan SDK ---
find_package(Vulkan REQUIRED)
find_package(glfw3 3.4 REQUIRED)

# --- Automatically determine vulkan.hpp tag ---
set(VULKAN_HPP_GIT_TAG "v${Vulkan_VERSION}")
message(STATUS "Found Vulkan SDK version: ${Vulkan_VERSION}")
message(STATUS "Setting vulkan.hpp Git tag to: ${VULKAN_HPP_GIT_TAG}")

# --- Fetch vulkan.hpp (The C++ Bindings) ---
include(FetchContent)
include(ExternalProject)

FetchContent_Declare(
	vulkan-hpp
	GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
	GIT_TAG        ${VULKAN_HPP_GIT_TAG}
)
FetchContent_MakeAvailable(vulkan-hpp)

# --- Add GLM ---
FetchContent_Declare(
	glm
	GIT_REPOSITORY https://github.com/g-truc/glm.git
	GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

# --- Add SGP4 ---
FetchContent_Declare(
	sgp4
	GIT_REPOSITORY https://github.com/dnwrnr/sgp4.git
	GIT_TAG        master
)
FetchContent_MakeAvailable(sgp4)

# --- Fetch Celestrak active TLE data ---
set(CELESTRAK_TLE_URL "https://celestrak.org/NORAD/ELEMENTS/gp.php?GROUP=active&FORMAT=tle")
set(CELESTRAK_TLE_PREFIX "${FETCHCONTENT_BASE_DIR}/celestrak_tle")
set(CELESTRAK_TLE_DOWNLOAD_DIR "${CELESTRAK_TLE_PREFIX}/download")
set(CELESTRAK_TLE_DATA_FILE "${CELESTRAK_TLE_DOWNLOAD_DIR}/active.tle")
set(FLOCK_TLE_HEADER_FILE "${CMAKE_BINARY_DIR}/src/rso/FLOCK_tle_data.hpp")

ExternalProject_Add(celestrak_tle_raw
	URL ${CELESTRAK_TLE_URL}
	DOWNLOAD_NO_EXTRACT TRUE
	DOWNLOAD_NAME active.tle
	DOWNLOAD_DIR ${CELESTRAK_TLE_DOWNLOAD_DIR}
	PREFIX ${CELESTRAK_TLE_PREFIX}
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
)

add_custom_command(
	OUTPUT ${FLOCK_TLE_HEADER_FILE}
	COMMAND ${CMAKE_COMMAND}
		-DRAW_TLE_FILE=${CELESTRAK_TLE_DATA_FILE}
		-DOUTPUT_HEADER_FILE=${FLOCK_TLE_HEADER_FILE}
		-P ${CMAKE_SOURCE_DIR}/scripts/filter_flock_tle.cmake
	DEPENDS ${CELESTRAK_TLE_DATA_FILE} ${CMAKE_SOURCE_DIR}/scripts/filter_flock_tle.cmake
	COMMENT "Filtering and embedding FLOCK TLE data"
	VERBATIM
)

add_custom_target(flock_tle_data DEPENDS ${FLOCK_TLE_HEADER_FILE})
add_dependencies(flock_tle_data celestrak_tle_raw)

# --- Find Slang Compiler ---
find_program(SLANGC_EXECUTABLE slangc)
if(NOT SLANGC_EXECUTABLE)
	message(FATAL_ERROR "slangc not found! Please install Slang and add it to your PATH.")
endif()
message(STATUS "Found Slang compiler: ${SLANGC_EXECUTABLE}")

# --- Define Shader Compilation Function ---
function(add_slang_shader_target TARGET_NAME)
	# parse arguments list
	cmake_parse_arguments(ARG "" "" "SOURCES;ENTRIES;DEPENDS" ${ARGN})

	list(GET ARG_SOURCES 0 MAIN_SOURCE)
	get_filename_component(SHADER_NAME ${MAIN_SOURCE} NAME_WE)

	set(WORK_DIR ${CMAKE_CURRENT_BINARY_DIR})
	set(OUTPUT_SPV ${WORK_DIR}/${SHADER_NAME}.spv)
	set(OUTPUT_HEADER ${WORK_DIR}/${SHADER_NAME}.hpp)

	# Generate flags dynamically based on input
	set(ENTRY_POINT_FLAGS "")
	if(ARG_ENTRIES)
		foreach(ENTRY ${ARG_ENTRIES})
			list(APPEND ENTRY_POINT_FLAGS "-entry" "${ENTRY}")
		endforeach()
	else()
		# Default fallback (for triangle.slang)
		set(ENTRY_POINT_FLAGS "-entry" "vertMain" "-entry" "fragMain")
	endif()

	add_custom_command(
		OUTPUT ${OUTPUT_HEADER}
		# Use ${ENTRY_POINT_FLAGS} variable
		COMMAND ${SLANGC_EXECUTABLE} ${ARG_SOURCES} 
			-target spirv -profile spirv_1_4 -emit-spirv-directly 
			-fvk-use-entrypoint-name ${ENTRY_POINT_FLAGS} 
			-o ${OUTPUT_SPV}

		COMMAND ${CMAKE_COMMAND} 
			-DINPUT_FILE=${OUTPUT_SPV} 
			-DOUTPUT_FILE=${OUTPUT_HEADER}
			-DNAMESPACE=${SHADER_NAME}
			-P ${CMAKE_SOURCE_DIR}/scripts/embed_spv.cmake

		DEPENDS ${ARG_SOURCES} ${CMAKE_SOURCE_DIR}/scripts/embed_spv.cmake ${ARG_DEPENDS}
		COMMENT "Compiling and Embedding Slang Shader: ${SHADER_NAME}"
	)

	add_custom_target(${TARGET_NAME} DEPENDS ${OUTPUT_HEADER})
endfunction()

# Add the RSO application directory
add_subdirectory(src/rso)
