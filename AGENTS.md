# AGENTS.md — Workspace Rules & Developer Guide for AI Agents

This file is automatically loaded into the agent context for all tasks in this repository. Follow these rules strictly.

---

## 1. Project Identity & Core Philosophy (`svk` & `rso`)

- **What `svk` Is:** `svk` is a **resource-oriented C++20 + Vulkan 1.3 engine for scientific computation** with optional rudimentary visualization.
- **Fragile, Precise, and Optimized:** `svk` is **not** a general-purpose game engine. Prefer explicit contracts, minimal state, deterministic execution, and fail-fast invariants over defensive bloat or runtime indirection. Numerical accuracy of scientific computation always takes priority over visualization polish.
- **Command Baking as Standard Practice:** Because scientific compute workloads execute identical dispatch/transfer topologies hundreds of thousands of times, **pre-baking command buffers** (`bakeCommands` upfront, `submitCommands` in the hot loop) is the expected default pattern—avoid unnecessary per-step CPU command re-recording.
- **Static Library + Optimized Specialized Executables:**
  - `svk` is compiled as static library targets (`svk_core` without GLFW, `svk_window` with GLFW) shared across `rso`, `rso_headless`, `triangle`, and future executables, relying on LTO + section garbage collection (`-ffunction-sections -fdata-sections -Wl,--gc-sections`) so each final binary stays minimal, fast, and self-contained:
    - `rso_headless`: Pure compute & telemetry output (**no GLFW / windowing dependency**).
    - `rso`: Interactive graphical visualization (links `svk_window` / GLFW).
    - `rso_video` *(planned)*: Headless offscreen rendering piped to FFmpeg (**no GLFW dependency**).
- **Zero Heavy Runtime Bloat:** No VMA (`VulkanMemoryAllocator`), no Dear ImGui, no external asset/font loaders. Shaders (`.slang`) are compiled to SPIR-V at build time via `add_slang_shader_target` ([`cmake/svk_common.cmake`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/cmake/svk_common.cmake)) and embedded directly as C++ `uint32_t` array headers (`<shader_name>::smci`).

---

## 2. Agent Roles & Execution Workflow (Planner vs. Worker)

- **Role Division & Delegation Level:**
  - **Architecture Planner (`Carbon High`):** Responsible for high-level system design, Vulkan synchronization/memory contracts, Mermaid architecture specifications, and decomposing work into small, self-contained, single-issue tasks. **Do not micromanage workers with literal line-by-line diffs**—delegate by pointing a dedicated per-task worker to the issue, invariants, and proposed architectural solution, then review the resulting diff.
  - **Per-Task Worker (`Hydrogen` / `flash` subagent):** Spawn a fresh per-task worker per isolated issue. It inspects the relevant files, implements **strictly one atomic issue/task**, formats (`clang-format`), builds, runs static analysis (`clang-tidy`), and reports **only a concise summary at task completion** (no step-by-step intermediate chatter).
- **One Issue at a Time:** Never bundle unrelated refactors or multiple issues into a single step. Complete, format, build, and verify one atomic task before moving to the next.

---

## 3. Environment & Compiler (AlmaLinux 9 / GCC 13)

- **OS:** AlmaLinux 9 (RHEL 9 compatible). The default system compiler (`/usr/bin/g++`, GCC 11) is **too old** and will fail CMake configuration and `clang-tidy` checks.
- **Required Toolchain:** `gcc-toolset-13` (`/opt/rh/gcc-toolset-13/root/usr/bin/g++`).
- **CRITICAL Rule for Terminal Commands:** Each `run_command` execution spawns a **new, isolated shell session**—environment variables do **not** persist across calls. Whenever running `cmake`, `make`, `ninja`, `clang-tidy`, or `ctest`, **always** source the toolset in the same command invocation:
  ```bash
  source /opt/rh/gcc-toolset-13/enable && cmake -B build -DCMAKE_BUILD_TYPE=Release
  source /opt/rh/gcc-toolset-13/enable && cmake --build build --parallel
  ```
- **Offline / Sandbox Builds:** Third-party dependencies (`glm`, `vulkan-hpp`, `sgp4`, `googletest`, `slang`, `celestrak_tle`) are already populated in `_deps/`. When configuring a new build directory inside the sandbox (no network), pass `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` if needed, or reuse the existing `build/` directory.

---

## 4. Code Craft, Style & Formatting (`.clang-format`)

All C++ code under `src/` **must** conform to [`.clang-format`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/.clang-format) and project craft conventions:
- **Type Discipline for Indices & Sentinels:** Use `std::size_t` for host C++ container/array indexing (e.g., slot indices into `std::array` / `std::vector`) and explicit fixed-width Vulkan types (`uint32_t`, `vk::DeviceSize`) for Vulkan API indices/handles (e.g., `queueFamilyIndex`, descriptor bindings), with distinct named `static constexpr` sentinels—never conflate host container slots with Vulkan indices.
- **Documented Constants:** Every `static constexpr` constant, limit, or sentinel in class/header definitions must include a brief explanatory comment stating its meaning, unit, or Vulkan specification origin.
- **Member Helpers Over Static State-Passing:** Prefer private (`const` where applicable) member functions when operating on a class's own members (`m_physicalDevice`, `m_logger`, etc.) rather than `static` helpers that pass member references back into the class.
- **Formatting Rules:** Tabs (`UseTab: ForIndentation`, `TabWidth: 4`), `120`-column limit, custom Allman braces with short single-line blocks/functions/ifs allowed, left-aligned pointers/references (`Type* ptr`, `Type& ref`).
- **Mandatory Post-Edit Step:** After creating or modifying any `.cpp` or `.hpp` file in `src/`, always format it before finishing:
  ```bash
  clang-format -i <modified_files>
  ```

---

## 5. Include Hygiene (IWYU) & Precompiled Headers (PCH)

- **Strict Self-Contained Headers (Include What You Use):** Every `.hpp` and `.cpp` file must explicitly `#include` every header defining the types/functions it directly uses. Never rely on transitive includes or `SVK_PCH_HEADERS` to make a file compile.
- **PCH Role:** `SVK_PCH_HEADERS` in [`src/engine/CMakeLists.txt`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/src/engine/CMakeLists.txt) exists **solely** to accelerate compilation speed.
- **Static Analysis (`clang-tidy`):** Enforce [`.clang-tidy`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/.clang-tidy) rules and verify includes when modifying translation units:
  ```bash
  source /opt/rh/gcc-toolset-13/enable && clang-tidy -p build <path/to/file.cpp>
  source /opt/rh/gcc-toolset-13/enable && clang-tidy -p build -checks='-*,misc-include-cleaner' <path/to/file.cpp>
  ```

---

## 6. Vulkan 1.3, GPU Memory & Shader Portability Rules

1. **Modern C++20 & Vulkan 1.3 RAII:**
   - Use `vk::raii::*` handles exclusively for automatic deterministic resource cleanup.
   - Declare class member `vk::raii` handles in strict dependency order so reverse-order destruction is always valid (e.g., `DescriptorSetLayout` -> `PipelineLayout` -> `Pipeline`).
   - Use C++20 designated initializers (`.field = value`) for all Vulkan structs (`VULKAN_HPP_NO_CONSTRUCTORS` is defined project-wide).
   - Target Vulkan 1.3 core features: `synchronization2` (`vk::SubmitInfo2`, `vk::ImageMemoryBarrier2`, `vk::MemoryBarrier2`, `pipelineBarrier2`) and `dynamicRendering` (`cmd.beginRendering`, no `VkRenderPass` or `VkFramebuffer`).
2. **Polyglot CPU/GPU Shared Structs:**
   - Host-shared GPU structs (`UBO`, `AgentData`, vertex/storage structs) should be defined in a **polyglot header** (`.slangh` / shared C++ & Slang header) as the single source of truth, backed on the C++ side by explicit `alignas(16)` and `static_assert` size/offset checks so CPU and GPU layouts can never drift.
3. **Explicit Hardware Feature Verification & Broad GPU Support:**
   - Every Vulkan feature requested at device creation **must** be explicitly queried and verified via `getFeatures2()`—never blindly enable unused or optional features.
   - Do **not** rely on `shaderFloat16`, `shaderFloat64`, or hardware float atomic extensions (`VK_EXT_shader_atomic_float`) in shaders: the primary development target (`Intel(R) Iris(R) Xe Graphics (ADL GT2)`) has `shaderFloat64 = false`, lacks hardware float atomic add, and has buggy/unwanted `float16` behavior. Keep GPU compute in portable IEEE-754 `float32` and use `double` on the CPU for long-horizon accumulations.
   - Decouple queue selection cleanly: headless compute selects only `TRANSFER` + `COMPUTE` queues; graphical mode adds `GRAPHICS` + `PRESENT`.

---

## 7. Build Profiles Reference

See [`how_to_build.md`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/how_to_build.md) and [`cmake/build_config.cmake`](file:///home/pasha/build/RSO/RemoteSensingOptimizer/cmake/build_config.cmake) for full details:

| Build Type | Command | Notes |
| :--- | :--- | :--- |
| **Debug** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug` | `-O0 -g3`, Vulkan Validation Layers ON, ASan + UBSan ON |
| **Release** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build -DCMAKE_BUILD_TYPE=Release` | `-O3 -march=native -fno-plt`, LTO ON, Validation OFF |
| **Bench** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build_bench -DCMAKE_BUILD_TYPE=Bench` | Adds `-ffast-math -funroll-loops` (timing only; breaks strict IEEE 754) |
| **RelWithDebInfo** | `source /opt/rh/gcc-toolset-13/enable && cmake -B build_reldbg -DCMAKE_BUILD_TYPE=RelWithDebInfo` | `-O2 -g -fno-omit-frame-pointer` for `perf` profiling |
