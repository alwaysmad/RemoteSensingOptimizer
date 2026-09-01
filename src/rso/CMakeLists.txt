# src/rso/CMakeLists.txt
# Remote Sensing Optimizer: Earth observation satellite visualization
# Dependencies: SGP4 (orbital mechanics), Celestrak TLE data

# ============================================================================
# RSO Dependencies
# ============================================================================

# === SGP4 Orbital Mechanics Library (Library Only) ===
# Disable example binaries (sattrack, runtest, passpredict, csvprop)
# We only need the libsgp4 classes, not the utilities
if(NOT sgp4_POPULATED)
	include(FetchContent)
	FetchContent_Declare(
		sgp4
		GIT_REPOSITORY https://github.com/dnwrnr/sgp4.git
		GIT_TAG        master
	)
	# Disable SGP4 example binaries and tests
	set(BUILD_EXAMPLES OFF CACHE BOOL "Disable SGP4 example binaries" FORCE)
	set(BUILD_TESTS OFF CACHE BOOL "Disable SGP4 tests" FORCE)
	FetchContent_MakeAvailable(sgp4)
	message(STATUS "RSO: SGP4 orbital mechanics library fetched (library only, no binaries)")
endif()

# === Fetch Celestrak Active TLE Data ===
set(CELESTRAK_TLE_URL "https://celestrak.org/NORAD/ELEMENTS/gp.php?GROUP=active&FORMAT=tle")
set(CELESTRAK_TLE_PREFIX "${FETCHCONTENT_BASE_DIR}/celestrak_tle")
set(CELESTRAK_TLE_DOWNLOAD_DIR "${CELESTRAK_TLE_PREFIX}/download")
set(CELESTRAK_TLE_DATA_FILE "${CELESTRAK_TLE_DOWNLOAD_DIR}/active.tle")
set(FLOCK_TLE_HEADER_FILE "${CMAKE_BINARY_DIR}/src/rso/FLOCK_tle_data.hpp")

include(ExternalProject)
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

# ============================================================================
# RSO Shaders
# ============================================================================

# === Compile RSO Shaders ===
add_slang_shader_target(CompileMesh
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.slang
	ENTRIES vertMain fragMain
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shared.slang
)

add_slang_shader_target(CompileComputeAlpha
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compute_alpha.slang
	ENTRIES computeMain
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shared.slang
)

add_slang_shader_target(CompileSatellite
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/satellite.slang
	ENTRIES vertMain fragMain
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shared.slang
)

add_slang_shader_target(CompileReduce
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/reduce.slang
	ENTRIES reduceMain
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shared.slang
)

# ============================================================================
# RSO Common Sources
# ============================================================================

# === RSO Common Sources ===
set(RSO_COMMON_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Settings.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/BufferStructs.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/Application.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/EngineInstance.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/EngineInstance.cpp
	${CMAKE_SOURCE_DIR}/src/engine/window/Camera.cpp
	${CMAKE_SOURCE_DIR}/src/engine/window/WindowContext.hpp
	${CMAKE_SOURCE_DIR}/src/engine/window/Window.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Buffer.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Command.hpp
	${CMAKE_SOURCE_DIR}/src/engine/ComputePipeline.cpp
	${CMAKE_SOURCE_DIR}/src/engine/ComputePipeline.hpp
	${CMAKE_SOURCE_DIR}/src/engine/ComputeRoutine.cpp
	${CMAKE_SOURCE_DIR}/src/engine/ComputeRoutine.hpp
	${CMAKE_SOURCE_DIR}/src/engine/DepthResources.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Device.cpp
	${CMAKE_SOURCE_DIR}/src/engine/Device.hpp
	${CMAKE_SOURCE_DIR}/src/engine/GraphicsPipeline.cpp
	${CMAKE_SOURCE_DIR}/src/engine/GraphicsPipeline.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Image.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Instance.cpp
	${CMAKE_SOURCE_DIR}/src/engine/Instance.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Logger.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Queue.hpp
	${CMAKE_SOURCE_DIR}/src/engine/RenderTask.cpp
	${CMAKE_SOURCE_DIR}/src/engine/RenderTask.hpp
	${CMAKE_SOURCE_DIR}/src/engine/RenderRoutine.cpp
	${CMAKE_SOURCE_DIR}/src/engine/RenderRoutine.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Swapchain.cpp
	${CMAKE_SOURCE_DIR}/src/engine/Swapchain.hpp
	${CMAKE_SOURCE_DIR}/src/engine/TransferRoutine.cpp
	${CMAKE_SOURCE_DIR}/src/engine/TransferRoutine.hpp
)

# ============================================================================
# Build RSO (GUI version)
# ============================================================================

# === Build RSO (GUI version) ===
add_executable(rso ${RSO_COMMON_SOURCES})
target_precompile_headers(rso PRIVATE ${SVK_PCH_HEADERS})
target_include_directories(rso PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${sgp4_SOURCE_DIR}/libsgp4
	${vulkan-hpp_BINARY_DIR}
)
target_compile_definitions(rso PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:NDEBUG>
	VULKAN_HPP_NO_CONSTRUCTORS
	GLFW_INCLUDE_VULKAN
	GLM_FORCE_CXX20
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	GLM_FORCE_RADIANS
)
target_link_libraries(rso PRIVATE
	Vulkan::Vulkan
	glfw
	glm::glm
	sgp4
)
add_dependencies(rso flock_tle_data CompileMesh CompileComputeAlpha CompileSatellite CompileReduce)

# ============================================================================
# Build RSO Headless (no GUI)
# ============================================================================

# === Build RSO Headless (no GUI) ===
add_executable(rso_headless ${RSO_COMMON_SOURCES})
target_precompile_headers(rso_headless PRIVATE ${SVK_PCH_HEADERS})
target_include_directories(rso_headless PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${sgp4_SOURCE_DIR}/libsgp4
	${vulkan-hpp_BINARY_DIR}
)
target_compile_definitions(rso_headless PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:NDEBUG>
	VULKAN_HPP_NO_CONSTRUCTORS
	GLFW_INCLUDE_VULKAN
	GLM_FORCE_CXX20
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	GLM_FORCE_RADIANS
	HEADLESS
)
target_link_libraries(rso_headless PRIVATE
	Vulkan::Vulkan
	glfw
	glm::glm
	sgp4
)
add_dependencies(rso_headless flock_tle_data CompileMesh CompileComputeAlpha CompileSatellite CompileReduce)

message(STATUS "RSO: Applications configured (rso + rso_headless with SGP4 and Celestrak TLE)")