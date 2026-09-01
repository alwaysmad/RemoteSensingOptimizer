# src/triangle/CMakeLists.txt
# Triangle Example: Simple hardcoded RGB triangle demonstrating SVK API
# Dependencies: SVK Engine only

# ============================================================================
# Triangle Shader
# ============================================================================

# === Compile Triangle Shader ===
add_slang_shader_target(CompileTriangle
	SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.slang
	ENTRIES vertMain fragMain
)

# ============================================================================
# Triangle Binary
# ============================================================================

# === Build Triangle Binary ===
add_svk_binary(triangle
	SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
		${CMAKE_SOURCE_DIR}/src/engine/window/WindowContext.hpp
		${CMAKE_SOURCE_DIR}/src/engine/window/Window.hpp
		${CMAKE_SOURCE_DIR}/src/engine/window/Camera.cpp
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
	SHADERS
		CompileTriangle
	DEPENDENCIES
)

message(STATUS "Triangle: Example configured")