# src/rso/CMakeLists.txt

set(RSO_COMMON_SOURCES
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
	${CMAKE_SOURCE_DIR}/src/engine/RenderTask.cpp
	${CMAKE_SOURCE_DIR}/src/engine/RenderTask.hpp
	${CMAKE_SOURCE_DIR}/src/engine/RenderRoutine.cpp
	${CMAKE_SOURCE_DIR}/src/engine/RenderRoutine.hpp
	${CMAKE_SOURCE_DIR}/src/engine/TransferRoutine.cpp
	${CMAKE_SOURCE_DIR}/src/engine/TransferRoutine.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Image.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Instance.cpp
	${CMAKE_SOURCE_DIR}/src/engine/Instance.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Logger.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Queue.hpp
	${CMAKE_SOURCE_DIR}/src/engine/Swapchain.cpp
	${CMAKE_SOURCE_DIR}/src/engine/Swapchain.hpp
	Settings.hpp
	BufferStructs.hpp
	Application.hpp
	Application.cpp
	EngineInstance.hpp
	EngineInstance.cpp
)

set(RSO_COMMON_COMPILE_DEFINITIONS
	$<$<NOT:$<CONFIG:Debug>>:NDEBUG>
	VULKAN_HPP_NO_CONSTRUCTORS
	GLFW_INCLUDE_VULKAN
	GLM_FORCE_CXX20
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	GLM_FORCE_RADIANS
)

function(add_rso_binary TARGET_NAME)
	cmake_parse_arguments(ARG "" "" "DEFINITIONS" ${ARGN})

	add_executable(${TARGET_NAME} main.cpp)
	target_sources(${TARGET_NAME} PRIVATE ${RSO_COMMON_SOURCES})
	target_compile_definitions(${TARGET_NAME} PRIVATE ${RSO_COMMON_COMPILE_DEFINITIONS} ${ARG_DEFINITIONS})
	target_include_directories(${TARGET_NAME} PRIVATE
		${vulkan-hpp_BINARY_DIR}
		${sgp4_SOURCE_DIR}/libsgp4
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/..
		${CMAKE_CURRENT_BINARY_DIR}
	)
	target_link_libraries(${TARGET_NAME} PRIVATE 
		Vulkan::Vulkan
		glfw
		glm::glm
		sgp4
	)

	target_precompile_headers(${TARGET_NAME} PRIVATE
	# STL
	<vector>      # dynamic arrays (extensions, layer lists, etc.)
	<array>       # fixed-size math/vertex helper arrays
	<string>      # owning text (paths, names, titles)
	<stdexcept>   # runtime_error and standard exceptions
	<exception>   # base exception types for catch clauses
	<fstream>     # file-based logger output
	<iostream>    # console logging streams
	<mutex>       # logger synchronization primitives
	<string_view> # non-owning string constants/views
	<format>      # type-safe formatting in logger methods
	<optional>    # optional singleton application instance
	<cstdlib>     # EXIT_SUCCESS / EXIT_FAILURE constants
	<utility>     # std::move / forwarding helpers
	<algorithm>   # ranges algorithms and utilities
	<cstring>     # C string interop (strcmp)
	<filesystem>  # temporary path and filesystem helpers
	<chrono>      # frame time delta calculations
	<limits>
	<cstddef>

	# Third Party
	<vulkan/vulkan_raii.hpp>
	<GLFW/glfw3.h>
	<glm/glm.hpp>
	<glm/mat4x4.hpp>
	<glm/gtc/packing.hpp>
	<glm/gtc/constants.hpp>
	<glm/gtc/matrix_transform.hpp> # NEW: For lookAt/perspective

	# Project Local
	"${CMAKE_SOURCE_DIR}/src/engine/Flags.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Frames.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Logger.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/window/WindowContext.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/window/Window.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/window/Camera.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Queue.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Buffer.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Command.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Device.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/GraphicsPipeline.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Image.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Swapchain.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/Instance.hpp"
	"${CMAKE_SOURCE_DIR}/src/engine/VulkanHelpers.hpp"
)

endfunction()

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

add_rso_binary(rso DEFINITIONS)
add_rso_binary(rso_headless DEFINITIONS HEADLESS)

add_dependencies(rso flock_tle_data CompileMesh CompileComputeAlpha CompileSatellite CompileReduce)
add_dependencies(rso_headless flock_tle_data CompileMesh CompileComputeAlpha CompileSatellite CompileReduce)
