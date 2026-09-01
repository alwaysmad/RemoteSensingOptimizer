# ============================================================================
# Build Configuration
# ============================================================================

# Compiler Version Check
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0)
        message(FATAL_ERROR "GCC 11+ required. Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0)
        message(WARNING "GCC 13+ recommended for best C++20 support. Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
        message(WARNING "Clang 15+ recommended. Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
endif()

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Type Selection with Options
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING 
        "Build type: Debug|Release|RelWithDebInfo|MinSizeRel" FORCE)
    message(STATUS "No build type specified. Defaulting to: ${CMAKE_BUILD_TYPE}")
else()
    message(STATUS "Building with: ${CMAKE_BUILD_TYPE}")
endif()

# ============================================================================
# Compiler Flags by Build Type
# ============================================================================

# Base warnings (all builds)
add_compile_options(-Wall -Wextra -Wpedantic)

# Debug Build
set(CMAKE_CXX_FLAGS_DEBUG 
    "-g -O0 -DDEBUG"
    CACHE STRING "Debug flags")

# Release Build (optimized)
set(CMAKE_CXX_FLAGS_RELEASE 
    "-O3 -DNDEBUG -march=native"
    CACHE STRING "Release flags")

# Release with Debug Info
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "-O2 -g -DNDEBUG"
    CACHE STRING "RelWithDebInfo flags")

# Minimal Size Release
set(CMAKE_CXX_FLAGS_MINSIZEREL 
    "-Os -DNDEBUG"
    CACHE STRING "MinSizeRel flags")

# ============================================================================
# Optimization & LTO
# ============================================================================

option(ENABLE_LTO "Enable Link Time Optimization (Release only)" ON)

if(ENABLE_LTO AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
    
    if(IPO_SUPPORTED)
        message(STATUS "Link Time Optimization enabled")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION True)
    else()
        message(WARNING "IPO not supported: ${IPO_ERROR}")
    endif()
endif()

# ============================================================================
# Sanitizers
# ============================================================================

option(ENABLE_ASAN "Enable AddressSanitizer (Debug only)" ON)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (Debug only)" ON)
option(ENABLE_TSAN "Enable ThreadSanitizer (Debug only, conflicts with ASan)" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Sanitizer flags (mutually exclusive: ASAN+UBSAN or TSAN)
    if(ENABLE_TSAN)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(STATUS "ThreadSanitizer enabled (conflicts with ASan)")
            add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
            add_link_options(-fsanitize=thread)
        endif()
    else()
        # ASan + UBSan (default for Debug)
        if(ENABLE_ASAN OR ENABLE_UBSAN)
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                set(SANITIZER_FLAGS "")
                
                if(ENABLE_ASAN)
                    set(SANITIZER_FLAGS "${SANITIZER_FLAGS}address,")
                    message(STATUS "AddressSanitizer enabled")
                endif()
                
                if(ENABLE_UBSAN)
                    set(SANITIZER_FLAGS "${SANITIZER_FLAGS}undefined")
                    message(STATUS "UndefinedBehaviorSanitizer enabled")
                endif()
                
                # Remove trailing comma if only one enabled
                string(REGEX REPLACE ",$" "" SANITIZER_FLAGS "${SANITIZER_FLAGS}")
                
                add_compile_options(-fsanitize=${SANITIZER_FLAGS} -fno-omit-frame-pointer)
                add_link_options(-fsanitize=${SANITIZER_FLAGS})
                
                # Note: Sanitizer suppression files (-fsanitize-ignorelist) vary by compiler version
                # For GCC 13+, use -fsanitize-suppressions or disable this feature if not supported
            endif()
        endif()
    endif()
else()
    message(STATUS "Sanitizers disabled (Release build)")
endif()

# ============================================================================
# Output Directories
# ============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler:          ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "LTO Enabled:       ${ENABLE_LTO}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "ASan Enabled:      ${ENABLE_ASAN}")
    message(STATUS "UBSan Enabled:     ${ENABLE_UBSAN}")
    message(STATUS "TSan Enabled:      ${ENABLE_TSAN}")
endif()
message(STATUS "==========================")
message(STATUS "")
