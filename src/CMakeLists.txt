# src/CMakeLists.txt

set(RSO_COMMON_SOURCES
	engine/window/Camera.cpp
	engine/window/WindowContext.hpp
	engine/window/Window.hpp
	engine/Buffer.hpp
	engine/Command.hpp
	engine/ComputePipeline.cpp
	engine/ComputePipeline.hpp
	engine/ComputeRoutine.cpp
	engine/ComputeRoutine.hpp
	engine/DepthResources.hpp
	engine/Device.cpp
	engine/Device.hpp
	engine/GraphicsPipeline.cpp
	engine/RenderTask.cpp
	engine/RenderTask.hpp
	engine/RenderRoutine.cpp
	engine/RenderRoutine.hpp
	engine/TransferRoutine.cpp
	engine/TransferRoutine.hpp
	engine/Image.hpp
	engine/Instance.cpp
	engine/Instance.hpp
	engine/Logger.hpp
	engine/Queue.hpp
	engine/Swapchain.cpp
	engine/Swapchain.hpp
	Settings.hpp
	BufferStructs.hpp
	Application.hpp
	Application.cpp
	EngineInstance.hpp
	EngineInstance.cpp
)

set(RSO_COMMON_COMPILE_DEFINITIONS
	$<$<CONFIG:Debug>:ENABLE_DEBUG_LOGGING>
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
	"engine/Flags.hpp"
	"engine/Frames.hpp"
	"engine/Logger.hpp"
	"engine/window/WindowContext.hpp"
	"engine/window/Window.hpp"
	"engine/window/Camera.hpp"
	"engine/Queue.hpp"
	"engine/Buffer.hpp"
	"engine/Command.hpp"
	"engine/Device.hpp"
	"engine/GraphicsPipeline.hpp"
	"engine/Image.hpp"
	"engine/Swapchain.hpp"
	"engine/Instance.hpp"
	"engine/VulkanHelpers.hpp"
)

endfunction()

add_slang_shader_target(CompileMesh
	SOURCES ${CMAKE_SOURCE_DIR}/shaders/mesh.slang
	ENTRIES vertMain fragMain
	DEPENDS ${CMAKE_SOURCE_DIR}/shaders/shared.slang
)

add_slang_shader_target(CompileComputeAlpha
	SOURCES ${CMAKE_SOURCE_DIR}/shaders/compute_alpha.slang
	ENTRIES computeMain
	DEPENDS ${CMAKE_SOURCE_DIR}/shaders/shared.slang
)

add_slang_shader_target(CompileSatellite
	SOURCES ${CMAKE_SOURCE_DIR}/shaders/satellite.slang
	ENTRIES vertMain fragMain
	DEPENDS ${CMAKE_SOURCE_DIR}/shaders/shared.slang
)

add_slang_shader_target(CompileReduce
	SOURCES ${CMAKE_SOURCE_DIR}/shaders/reduce.slang
	ENTRIES reduceMain
	DEPENDS ${CMAKE_SOURCE_DIR}/shaders/shared.slang
)

add_rso_binary(rso)
add_rso_binary(rso_headless DEFINITIONS HEADLESS)

add_dependencies(rso flock_tle_data CompileMesh CompileComputeAlpha CompileSatellite CompileReduce)
add_dependencies(rso_headless flock_tle_data CompileMesh CompileComputeAlpha CompileSatellite CompileReduce)
