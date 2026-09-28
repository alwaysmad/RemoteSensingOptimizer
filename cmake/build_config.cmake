# ============================================================================
# Build Configuration
# ============================================================================

# All FetchContent dependencies are treated as SYSTEM headers — their code is
# not ours to fix, so we suppress warnings from it globally. This covers GLM,
# googletest, sgp4, and any future fetched dependency.
set(FETCHCONTENT_SYSTEM ON CACHE BOOL "Treat all FetchContent targets as SYSTEM" FORCE)


# Compiler Version Check
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.0)
        message(FATAL_ERROR
            "GCC 13+ required. Found: ${CMAKE_CXX_COMPILER_VERSION}\n"
            "Run: source /opt/rh/gcc-toolset-13/enable")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17.0)
        message(WARNING "Clang 17+ recommended for full C++20. Found: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
else()
    message(WARNING "Untested compiler: ${CMAKE_CXX_COMPILER_ID}. GCC 13+ or Clang 17+ recommended.")
endif()

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)   # No GNU extensions — pure ISO C++20

# ============================================================================
# Build Types
#
#  Debug    — Vulkan validation layers ON, sanitizers, no optimization, debug symbols
#  Release  — Vulkan validation layers OFF, O3 + LTO + march=native
#  Bench    — Same as Release + fast-math + loop unrolling + PGO-ready
#             WARNING: -ffast-math breaks IEEE 754. Not for correctness testing.
#
# Usage:
#   cmake -B build_debug   -DCMAKE_BUILD_TYPE=Debug
#   cmake -B build_release -DCMAKE_BUILD_TYPE=Release
#   cmake -B build_bench   -DCMAKE_BUILD_TYPE=Bench
# ============================================================================

# Register "Bench" as a known build type
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT IS_MULTI_CONFIG)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
        STRINGS "Debug;Release;Bench;RelWithDebInfo;MinSizeRel")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type: Debug | Release | Bench | RelWithDebInfo | MinSizeRel" FORCE)
    message(STATUS "No build type specified. Defaulting to: Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ============================================================================
# Base Warnings (all builds)
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter   # Too noisy during active development
        -Wshadow                # Variable shadowing is a real bug source
        -Wconversion            # Catch implicit narrowing
        -Wsign-conversion       # Catch signed/unsigned mismatch
    )
endif()

# ============================================================================
# Debug
# Goal: maximum correctness information, sanitizers, no optimization noise
# ============================================================================
set(CMAKE_CXX_FLAGS_DEBUG
    "-g3 -O0 -DDEBUG"
    CACHE STRING "Debug: full symbols, no optimization" FORCE)
# -g3: includes macro definitions in debug info (useful with GDB)
# -O0: no optimization — what you write is what executes, no reordering

# ============================================================================
# Release
# Goal: fast binary for normal use, portable flags
# ============================================================================
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -DNDEBUG -march=native -fno-plt"
    CACHE STRING "Release: O3 + native + LTO" FORCE)
# -march=native:  enables AVX2 + FMA on this i7-12700H (not portable to other CPUs)
# -fno-plt:       eliminates PLT trampolines for shared lib calls — small free win
# NOTE: binaries are NOT portable between different CPU microarchitectures

# ============================================================================
# Bench — maximum aggression for performance measurement only
# ============================================================================
set(CMAKE_CXX_FLAGS_BENCH
    "-O3 -DNDEBUG -march=native -fno-plt -ffast-math -funroll-loops -fno-omit-frame-pointer"
    CACHE STRING "Bench: maximum aggression, benchmarking only" FORCE)
# -ffast-math:           allows reassociation, no-NaN, no-Inf, no-signed-zero.
#                        Enables auto-vectorization the compiler refuses under strict IEEE.
#                        The orbital mechanics (SGP4, sun vector) may produce subtly
#                        different results — never use Bench to validate simulation output.
# -funroll-loops:        aggressive loop unrolling — benefits the compute_alpha inner loop
# -fno-omit-frame-pointer: keep call stacks intact for `perf record` profiling

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # PGO (Profile-Guided Optimization):
    # Step 1: build with -DCMAKE_BUILD_TYPE=Bench -DENABLE_PGO_GENERATE=ON
    # Step 2: run the binary (generates .gcda profile files)
    # Step 3: rebuild with -DCMAKE_BUILD_TYPE=Bench -DENABLE_PGO_GENERATE=OFF
    #         The compiler reads .gcda and optimizes hot paths accordingly.
    option(ENABLE_PGO_GENERATE "Instrument binary for PGO profile generation" OFF)
    if(ENABLE_PGO_GENERATE AND CMAKE_BUILD_TYPE STREQUAL "Bench")
        add_compile_options(-fprofile-generate)
        add_link_options(-fprofile-generate)
        message(STATUS "PGO: instrumentation build (run binary to collect .gcda files)")
    elseif(NOT ENABLE_PGO_GENERATE AND CMAKE_BUILD_TYPE STREQUAL "Bench")
        # Silently ignored if no .gcda files exist yet
        add_compile_options(-fprofile-use -fprofile-correction)
        add_link_options(-fprofile-use)
        message(STATUS "PGO: using profile data (if .gcda files exist from prior run)")
    endif()
endif()

# ============================================================================
# RelWithDebInfo — optimized + debug symbols for profiling (perf, valgrind)
# ============================================================================
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "-O2 -g -DNDEBUG -march=native -fno-omit-frame-pointer"
    CACHE STRING "RelWithDebInfo: O2 + debug symbols for profilers" FORCE)

# ============================================================================
# MinSizeRel
# ============================================================================
set(CMAKE_CXX_FLAGS_MINSIZEREL
    "-Os -DNDEBUG"
    CACHE STRING "MinSizeRel: smallest binary" FORCE)

# ============================================================================
# Link Time Optimization — Release and Bench only
# ============================================================================
option(ENABLE_LTO "Enable Link Time Optimization (Release/Bench only)" ON)

if(ENABLE_LTO AND CMAKE_BUILD_TYPE MATCHES "^(Release|Bench)$")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
    if(IPO_SUPPORTED)
        message(STATUS "LTO: enabled")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # Thin LTO: much faster link time than full LTO, nearly same quality
            add_compile_options(-flto=thin)
            add_link_options(-flto=thin)
        endif()
    else()
        message(WARNING "LTO not supported: ${IPO_ERROR}")
    endif()
else()
    message(STATUS "LTO: disabled (${CMAKE_BUILD_TYPE})")
endif()

# ============================================================================
# Sanitizers — Debug only
# Default: ASan + UBSan. TSan is opt-in (cannot run alongside ASan).
# ============================================================================
option(ENABLE_ASAN  "Enable AddressSanitizer"                          ON)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer"                ON)
option(ENABLE_TSAN  "Enable ThreadSanitizer (conflicts with ASan/UBSan)" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(ENABLE_TSAN)
        message(STATUS "Sanitizers: ThreadSanitizer")
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    else()
        set(SAN_FLAGS "")
        if(ENABLE_ASAN)
            string(APPEND SAN_FLAGS "address,")
            message(STATUS "Sanitizers: AddressSanitizer")
        endif()
        if(ENABLE_UBSAN)
            string(APPEND SAN_FLAGS "undefined")
            message(STATUS "Sanitizers: UndefinedBehaviorSanitizer")
        endif()
        string(REGEX REPLACE ",$" "" SAN_FLAGS "${SAN_FLAGS}")
        if(SAN_FLAGS)
            add_compile_options(-fsanitize=${SAN_FLAGS} -fno-omit-frame-pointer)
            add_link_options(-fsanitize=${SAN_FLAGS})
        endif()
    endif()
else()
    message(STATUS "Sanitizers: disabled (${CMAKE_BUILD_TYPE})")
endif()

# ============================================================================
# Slang shader optimization level
# Exported as SLANGC_OPT_FLAGS for use in svk_common.cmake
# ============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SLANGC_OPT_FLAGS "-O0;-g"
        CACHE STRING "slangc optimization flags" FORCE)
    # -g: embed SPIR-V debug info — names appear in RenderDoc / Nsight
elseif(CMAKE_BUILD_TYPE STREQUAL "Bench")
    set(SLANGC_OPT_FLAGS "-O3"
        CACHE STRING "slangc optimization flags" FORCE)
else()
    set(SLANGC_OPT_FLAGS "-O2"
        CACHE STRING "slangc optimization flags" FORCE)
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
message(STATUS "=== SVK Build Configuration ===")
message(STATUS "  Build Type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler    : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Std     : ${CMAKE_CXX_STANDARD}")
message(STATUS "  LTO         : ${ENABLE_LTO}")
message(STATUS "  Slang flags : ${SLANGC_OPT_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "  ASan        : ${ENABLE_ASAN}")
    message(STATUS "  UBSan       : ${ENABLE_UBSAN}")
    message(STATUS "  TSan        : ${ENABLE_TSAN}")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Bench")
    message(STATUS "  [!] -ffast-math ON  — IEEE 754 not guaranteed in Bench")
    message(STATUS "  [!] Use Release for correctness checks, Bench only for timing")
endif()
message(STATUS "================================")
message(STATUS "")
