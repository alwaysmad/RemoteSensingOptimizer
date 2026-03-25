# src/CMakeLists.txt

add_executable(rso main.cpp)

# 2. Add sources
target_sources(rso PRIVATE
	Application.cpp
	EngineInstance.cpp
	engine/Buffer.hpp
	engine/Device.cpp
	engine/Device.hpp
	engine/Image.hpp
	engine/Instance.cpp
	engine/Instance.hpp
	engine/Logger.hpp
	engine/Queue.hpp
	engine/Swapchain.cpp
	engine/Swapchain.hpp
	Application.hpp
	EngineInstance.hpp
	Settings.hpp
	engine/window/WindowContext.hpp
	engine/window/Window.hpp
)

# 3. Compile Definitions
target_compile_definitions(rso PRIVATE
	$<$<CONFIG:Debug>:ENABLE_DEBUG_LOGGING>
	VULKAN_HPP_NO_CONSTRUCTORS
	GLFW_INCLUDE_VULKAN
	GLM_FORCE_CXX20
	GLM_FORCE_DEPTH_ZERO_TO_ONE
	GLM_FORCE_RADIANS
)

# 4. Include Directories (CRITICAL FIX)
target_include_directories(rso PRIVATE
	${vulkan-hpp_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)

# 5. Link Libraries
target_link_libraries(rso PRIVATE 
	Vulkan::Vulkan
	glfw
	glm::glm
)

# 6. Precompiled Headers (PCH)
target_precompile_headers(rso PRIVATE
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
	<limits>

	# Third Party
	<vulkan/vulkan_raii.hpp>
	<GLFW/glfw3.h>
	#<glm/glm.hpp>
	#<glm/gtc/packing.hpp>
	#<glm/gtc/matrix_transform.hpp> # NEW: For lookAt/perspective

	# Project Local
	"Settings.hpp"
	"engine/Flags.hpp"
	"engine/Logger.hpp"
	"engine/window/WindowContext.hpp"
	"engine/window/Window.hpp"
	"engine/Queue.hpp"
	"engine/Buffer.hpp"
	"engine/Device.hpp"
	"engine/Image.hpp"
	"engine/Swapchain.hpp"
	"engine/Instance.hpp"
	#"Frames.hpp"
)

# 8. Add dependencies so they build before the executable
#add_dependencies(rso CompileMesh CompileSatellite CompileSolver)
