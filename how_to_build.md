# SVK / RSO Build Guide

Comprehensive build and configuration guide for **Remote Sensing Optimizer (RSO)** and **SVK**.

---

## Prerequisites

- **C++ Compiler:** GCC 13+ (or Clang 17+) supporting C++20.
  - On AlmaLinux / RHEL 9, enable `gcc-toolset-13`:
    ```bash
    source /opt/rh/gcc-toolset-13/enable
    ```
- **Vulkan SDK:** System package installed (required for Vulkan headers and validation layers).
- **Slang Shader Compiler:** Detected from `PATH` or automatically downloaded via CMake FetchContent (`v2026.18.3`).
- **CMake & Build Tools:** CMake 3.16+ and `ninja` or `make`.

---

## Build Profiles Summary

| Build Type | Optimization | Vulkan Validation | Sanitizers | LTO | Target Use Case |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **Debug** | `-O0 -g3` | **ON** | ASan + UBSan | ❌ | Development, debugging, bug hunting |
| **Release** | `-O3 -march=native` | OFF | ❌ | **ON** | Standard production runs |
| **Bench** | `-O3 -march=native -ffast-math -funroll-loops` | OFF | ❌ | **ON** | Maximum performance benchmarking |
| **RelWithDebInfo** | `-O2 -g -march=native` | OFF | ❌ | ❌ | Profiling (`perf`, call stacks) |

---

## Build Configurations

### 1. Debug
Enables Vulkan validation layers, AddressSanitizer (ASan), and UndefinedBehaviorSanitizer (UBSan) with zero optimization for maximum diagnostic accuracy.

```bash
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug -j$(nproc)

# Run interactive or headless
./build_debug/bin/rso
./build_debug/bin/rso_headless
```

---

### 2. Release
Standard production build with high optimization (`-O3`), native CPU architecture targeting (`-march=native`), Link-Time Optimization (LTO), and validation layers disabled.

```bash
cmake -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release -j$(nproc)

# Run
./build_release/bin/rso
```

---

### 3. Bench (Maximum Aggression)
Designed strictly for performance measurement. Adds aggressive loop unrolling and `-ffast-math`.

> [!WARNING]
> `-ffast-math` breaks strict IEEE 754 compliance (reassociation, no-NaN/Inf assumptions). Orbital mechanics calculations (SGP4, sun vectors) may produce subtle numerical differences. **Use only for timing and throughput measurement, never for simulation correctness validation.**

```bash
cmake -B build_bench -DCMAKE_BUILD_TYPE=Bench
cmake --build build_bench -j$(nproc)

# Run benchmark
./build_bench/bin/rso_headless
```

---

### 4. RelWithDebInfo (Profiling)
Optimized build (`-O2`) preserving debug symbols (`-g`) and frame pointers (`-fno-omit-frame-pointer`). Ideal for sampling with profilers like `perf`.

```bash
cmake -B build_reldbg -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build_reldbg -j$(nproc)

# Profile with Linux perf
perf record ./build_reldbg/bin/rso_headless
perf report
```

---

## 🔬 Profile-Guided Optimization (PGO) Workflow

> **Compiler:** GCC &bull; **Build Type:** `Bench`

Profile-Guided Optimization instruments the binary to gather real execution path statistics during a workload run, then feeds the profile back into a second build for optimized code layout and branch prediction.

```bash
# Step 1: Build instrumented binary
cmake -B build_pgo_gen -DCMAKE_BUILD_TYPE=Bench -DENABLE_PGO_GENERATE=ON
cmake --build build_pgo_gen -j$(nproc)

# Step 2: Run workload to generate profile data (.gcda files)
./build_pgo_gen/bin/rso_headless

# Step 3: Rebuild using collected profile feedback
cmake -B build_pgo -DCMAKE_BUILD_TYPE=Bench -DENABLE_PGO_GENERATE=OFF
cmake --build build_pgo -j$(nproc)
```

---

## 🎛️ CMake Configuration Options

Configure these options by passing `-D<OPTION>=<VALUE>` to the `cmake -B ...` configuration step.

### Optimization & Sanitizers

| Option | Values | Default | Description |
| :--- | :---: | :---: | :--- |
| `ENABLE_LTO` | `ON` / `OFF` | `ON` | Link-Time Optimization (applies to `Release` and `Bench` builds). |
| `ENABLE_ASAN` | `ON` / `OFF` | `ON` | AddressSanitizer (`Debug` only). |
| `ENABLE_UBSAN` | `ON` / `OFF` | `ON` | UndefinedBehaviorSanitizer (`Debug` only). |
| `ENABLE_TSAN` | `ON` / `OFF` | `OFF` | ThreadSanitizer (`Debug` only; mutually exclusive with ASan/UBSan). |

### Synthetic Constellation Parameters

| Option | Type / Unit | Default | Description |
| :--- | :---: | :---: | :--- |
| `SYNTH_SAT_COUNT` | Integer | `100` | Number of synthetic satellites generated in the test constellation. |
| `SYNTH_RAAN_START` | Degrees (`0`–`360`) | `0` | Starting Right Ascension of the Ascending Node (RAAN). |
| `SYNTH_RAAN_SPAN` | Degrees (`0`–`360`) | `45` | Total RAAN span across the generated constellation planes. |

#### Example: Debug build with ThreadSanitizer and custom constellation size
```bash
cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TSAN=ON \
  -DSYNTH_SAT_COUNT=250 \
  -DSYNTH_RAAN_SPAN=90
cmake --build build_tsan -j$(nproc)
```
