# src/CMakeLists.txt

add_executable(rso main.cpp)

# 2. Add sources
target_sources(rso PRIVATE
	Application.cpp
	EngineInstance.cpp
	engine/Instance.cpp
	engine/Instance.hpp
	engine/Logger.hpp
	Application.hpp
	EngineInstance.hpp
	Settings.hpp
	engine/window/WindowContext.hpp
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
	<vector>
	<array>
	<string>
	<stdexcept>
	<exception>
	<fstream>
	<iostream>
	<mutex>
	<string_view>
	<format>
	<optional>
	<cstdlib>
	<utility>
	<algorithm>
	<cstring>
	<filesystem>

	# Third Party
	<vulkan/vulkan_raii.hpp>
	<GLFW/glfw3.h>
	#<glm/glm.hpp>
	#<glm/gtc/packing.hpp>
	#<glm/gtc/matrix_transform.hpp> # NEW: For lookAt/perspective

	# Project Local
	"Settings.hpp"
	"engine/Logger.hpp"
	"Application.hpp"
	#"EngineInstance.hpp"
	"engine/Instance.hpp"
	"engine/window/WindowContext.hpp"
	#"Frames.hpp"
)

# 8. Add dependencies so they build before the executable
#add_dependencies(rso CompileMesh CompileSatellite CompileSolver)
