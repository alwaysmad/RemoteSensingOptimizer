# ============================================================================
# SVK Common Helpers
# ============================================================================

# Helper function to add SVK-based binaries with PCH
function(add_svk_binary TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;SHADERS;DEPENDENCIES" ${ARGN})
    
    add_executable(${TARGET_NAME} ${ARG_SOURCES})
    
    # Apply engine PCH
    target_precompile_headers(${TARGET_NAME} PRIVATE ${SVK_PCH_HEADERS})
    
    # Standard includes
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
        ${vulkan-hpp_BINARY_DIR}
    )
    
    # Link engine dependencies
    target_link_libraries(${TARGET_NAME} PRIVATE
        Vulkan::Vulkan glfw glm::glm ${ARG_DEPENDENCIES}
    )
    
    # Compile definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
        VULKAN_HPP_NO_CONSTRUCTORS
        GLFW_INCLUDE_VULKAN
        GLM_FORCE_CXX20
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
    )
    
    # Add shader dependencies
    foreach(SHADER ${ARG_SHADERS})
        add_dependencies(${TARGET_NAME} ${SHADER})
    endforeach()
endfunction()

# Shader compilation function (engine-owned)
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
        # Default fallback
        set(ENTRY_POINT_FLAGS "-entry" "vertMain" "-entry" "fragMain")
    endif()

    add_custom_command(
        OUTPUT ${OUTPUT_HEADER}
        COMMAND ${SLANGC_EXECUTABLE} ${ARG_SOURCES} 
            -target spirv -profile spirv_1_4 -emit-spirv-directly 
            -fvk-use-entrypoint-name ${ENTRY_POINT_FLAGS} 
            -o ${OUTPUT_SPV}

        COMMAND ${CMAKE_COMMAND} 
            -DINPUT_FILE=${OUTPUT_SPV} 
            -DOUTPUT_FILE=${OUTPUT_HEADER}
            -DNAMESPACE=${SHADER_NAME}
            -P ${CMAKE_SOURCE_DIR}/cmake/embed_spv.cmake

        DEPENDS ${ARG_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/embed_spv.cmake ${ARG_DEPENDS}
        COMMENT "Compiling and Embedding Slang Shader: ${SHADER_NAME}"
    )

    add_custom_target(${TARGET_NAME} DEPENDS ${OUTPUT_HEADER})
endfunction()
