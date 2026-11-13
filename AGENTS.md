# AGENTS.md — Workspace Rules & Developer Guide for AI Agents

This file is automatically loaded into the agent context for all tasks in this repository. Follow these rules strictly.

---

## 1. Environment & Compiler (AlmaLinux 9 / GCC 13)

- **OS:** AlmaLinux 9 (RHEL 9 compatible). The default system compiler (`/usr/bin/g++`, GCC 11) is **too old** and will fail CMake configuration and `clang-tidy` checks.
- **Required Toolchain:** `gcc-toolset-13` (`/opt/rh/gcc-toolset-13/root/usr/bin/g++`).
- **CRITICAL Rule for Terminal Commands:** Each `run_command` execution spawns a **new, isolated shell session**—environment variables do **not** persist across calls. Whenever running `cmake`, `make`, `ninja`, `clang-tidy`, or `ctest`, **always** source the toolset in the same command invocation:
  ```bash
  source /opt/rh/gcc-toolset-13/enable && cmake -B build -DCMAKE_BUILD_TYPE=Release
  source /opt/rh/gcc-toolset-13/enable && cmake --build build -j$(nproc)
  ```
- **Offline / Sandbox Builds:** Third-party dependencies (`glm`, `vulkan-hpp`, `sgp4`, `googletest`, `slang`, `celestrak_tle`) are already populated in `_deps/`. When configuring a new build directory inside the sandbox (no network), pass `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` if needed, or reuse the existing `build/` directory.

---

## 2. Code Style & Formatting (`.clang-format`)

All C++ code under `src/` **must** conform to [`.clang-format`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/.clang-format):
- **Indentation:** Tabs (`UseTab: ForIndentation`, `TabWidth: 4`, `IndentWidth: 4`).
- **Line Length:** `120` characters maximum (`ColumnLimit: 120`).
- **Braces:** Allman-style custom brace wrapping (`AfterClass: true`, `AfterFunction: true`, `AfterControlStatement: Always`, `BeforeElse: true`, `BeforeCatch: true`), with short single-line blocks/functions/ifs allowed (`AllowShortBlocksOnASingleLine: Always`, `AllowShortIfStatementsOnASingleLine: WithoutElse`).
- **Pointers & References:** Left-aligned (`Type* ptr`, `Type& ref`).
- **Mandatory Post-Edit Step:** After creating or modifying any `.cpp` or `.hpp` file in `src/`, always format it before finishing:
  ```bash
  clang-format -i <modified_files>
  ```

---

## 3. Include Hygiene (IWYU) & Precompiled Headers (PCH)

- **Strict Self-Contained Headers (Include What You Use):** Every `.hpp` and `.cpp` file must explicitly `#include` every header defining the types/functions it directly uses. Never rely on transitive includes or `SVK_PCH_HEADERS` to make a file compile.
- **PCH Role:** `SVK_PCH_HEADERS` in [`src/engine/CMakeLists.txt`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/src/engine/CMakeLists.txt) exists **solely** to accelerate compilation speed.
- **Static Analysis (`clang-tidy`):** Enforce [`.clang-tidy`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/.clang-tidy) rules and verify includes when modifying translation units:
  ```bash
  source /opt/rh/gcc-toolset-13/enable && clang-tidy -p build <path/to/file.cpp>
  source /opt/rh/gcc-toolset-13/enable && clang-tidy -p build -checks='-*,misc-include-cleaner' <path/to/file.cpp>
  ```

---

## 4. Core Engine (`svk`) & Simulator (`rso`) Design Philosophy

1. **Extreme Simplicity & Zero Bloat:**
   - Do **not** introduce heavy third-party runtime dependencies (no VMA / VulkanMemoryAllocator, no Dear ImGui, no external asset loaders).
   - Binaries must remain self-contained. Shaders (`.slang`) are compiled to SPIR-V at build time via `add_slang_shader_target` ([`cmake/svk_common.cmake`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/cmake/svk_common.cmake)) and embedded directly as C++ `uint32_t` array headers (`<shader_name>::smci`).
2. **Modern C++20 & Vulkan 1.3 RAII:**
   - Use `vk::raii::*` handles exclusively for automatic deterministic resource cleanup.
   - Declare class member `vk::raii` handles in strict dependency order so reverse-order destruction is always valid (e.g., `DescriptorSetLayout` $\to$ `PipelineLayout` $\to$ `Pipeline`).
   - Use C++20 designated initializers (`.field = value`) for all Vulkan structs (`VULKAN_HPP_NO_CONSTRUCTORS` is defined project-wide).
   - Target Vulkan 1.3 core features: `synchronization2` (`vk::SubmitInfo2`, `vk::ImageMemoryBarrier2`, `vk::MemoryBarrier2`, `pipelineBarrier2`) and `dynamicRendering` (`cmd.beginRendering`, no `VkRenderPass` or `VkFramebuffer`).
3. **GPU Memory & Alignment Discipline:**
   - Host-shared GPU structs (`UBO`, `AgentData`, vertex/storage structs) must use explicit `alignas(16)` and `static_assert` size/offset checks to guarantee exact layout compatibility with Slang shaders.
   - Maintain explicit separation between `TRANSFER`, `COMPUTE`, `GRAPHICS`, and `PRESENT` queues in [`svk::Device`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/src/engine/Device.hpp).

---

## 5. Build Profiles Reference

See [`how_to_build.md`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/how_to_build.md) and [`cmake/build_config.cmake`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/cmake/build_config.cmake) for full details:

| Build Type | Command | Notes |
| :--- | :--- | :--- |
| **Debug** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug` | `-O0 -g3`, Vulkan Validation Layers ON, ASan + UBSan ON |
| **Release** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build -DCMAKE_BUILD_TYPE=Release` | `-O3 -march=native -fno-plt`, LTO ON, Validation OFF |
| **Bench** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build_bench -DCMAKE_BUILD_TYPE=Bench` | Adds `-ffast-math -funroll-loops` (timing only; breaks strict IEEE 754) |
| **RelWithDebInfo** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build_reldbg -DCMAKE_BUILD_TYPE=RelWithDebInfo` | `-O2 -g -fno-omit-frame-pointer` for `perf` profiling |
