---
applyTo: '**'
---

# Intel Ethernet HAL Copilot Instructions

## Component Overview

Intel Ethernet HAL is the Hardware Abstraction Layer that provides unified, cross-platform access to Intel Ethernet adapter timestamping and TSN capabilities. This component is the PRIMARY interface between Service Layer components (like gPTP) and Intel hardware.

**Architecture Position**: Hardware Abstraction Layer (Primary Intel Interface)
**Primary Responsibility**: Cross-platform abstraction for Intel I210/I219/I225/I226 timestamping and TSN features via intel-ethernet-hal.h API
**Critical Role**: Bridge between Service Layer protocols and intel_avb via intel.h API

## Working principles
-- ensure you understand the architecture and patterns before coding
-- No Fake, No Stubs, no Simulations, simplified code allowed in productive Code
-- no implementation based assumtions, use specification or analysis results (ask if required)
-- no false advertising, prove and ensure correctness
-- always use real hardware access patterns
-- use Intel hardware specifications for register access
-- code needs to compile before commit, no broken code
-- Always reference the exact Intel datasheet section or spec version when implementing register access.
-- Validate all hardware reads/writes with range checks or masks from the specification.
-- Every function must have a Doxygen comment explaining purpose, parameters, return values, and hardware context.
-- Use portable printf format macros (PRIu64, PRId64, PRIu32) to prevent Windows MSVC assertion dialogs
-- Never bypass intel_avb layer - always use intel.h API to intel_avb driver interface
-- This is the PRIMARY Intel hardware interface - Service Layer components should use intel-ethernet-hal.h API, NOT lib/common/hal/
-- no duplicate or redundant implementations to avoid inconsistencies and confusion; use centralized, reusable functions instead
-- no ad-hoc file copies (e.g., *_fixed, *_new, *_correct); refactor in place step-by-step to avoid breakage
-- Clean submit rules:
   - each commit compiles and passes checks
   - small, single-purpose, reviewable diffs (no WIP noise)
   - no dead or commented-out code; remove unused files
   - run formatter and static analysis before commit
   - update docs/tests and reference the spec/issue in the message
   - use feature flags or compatibility layers when incremental changes risk breakage
-- Avoid unnecessary duplication. Duplication is acceptable when it improves clarity, isolates modules, or is required for performance.
-- Avoid code that is difficult to understand. Prefer clear naming and structure over excessive comments or unnecessary helper variables.
-- Avoid unnecessary complexity. Keep required abstractions for maintainability, testability, or hardware safety
-- Design modules so that changes in one module do not require changes in unrelated modules. Avoid dependencies that cause single changes to break multiple areas.
-- Design components for reuse where practical, but prioritize correctness and domain fit over forced generalization.
-- Prefer incremental modification of existing code over reimplementation; adapt existing functions instead of creating redundant new ones

## CRITICAL: Architectural Position in OpenAvnu

### Hardware Integration Chain (CORRECT INTERFACES)
```
Service Layer (gPTP, AVDECC, etc.)
    ↓ (intel-ethernet-hal.h API)
Intel Ethernet HAL ← YOU ARE HERE
    ↓ (intel.h API)
intel_avb (Driver Interface)
    ↓ (avb_ioctl.h IOCTL-interface)
NDISIntelFilterDriver (Kernel Driver)
    ↓ (register access)
Intel NIC Hardware (I210/I219/I225/I226)
```

### PRIMARY Interface Requirements
- **Service Layer Integration**: gPTP and other components use intel-ethernet-hal.h API, not generic abstractions
- **NEVER bypass intel_avb**: All hardware access must go through intel.h API to intel_avb layer
- **Cross-platform consistency**: Provide identical intel-ethernet-hal.h API behavior on Windows NDIS and Linux PTP/PHC
- **Thread safety**: All public API functions must be thread-safe for multi-threaded Service Layer components

## Project Overview
- **Purpose:** Cross-platform hardware abstraction layer for Intel Ethernet adapters (I210, I219, I225, I226) with native Windows NDIS and Linux PTP integration.
- **Key Directories:**
  - `src/` — Core HAL implementation (see `common/`, `hal/`, `windows/`, `linux/`)
  - `include/` — Public API headers
  - `examples/` — Usage demos (e.g., `hal_device_info.c`)
  - `tests/` — Test suite and validation
  - `cmake/` — CMake configuration templates

## Architecture & Data Flow
- **Layered Design:**
  - Application Layer (OpenAvnu, gPTP, user apps)
  - Intel Ethernet HAL (cross-platform API)
  - OS Integration: Windows (NDIS), Linux (PTP/PHC)
  - Intel Hardware Drivers (e1dexpress, e1000e, igb)
  - Intel Ethernet Hardware (I210/I219/I225/I226)
- **Device Enumeration:** Use `intel_hal_enumerate_devices()` to discover supported adapters.
- **Timestamping:** Platform-specific enable/disable via `intel_hal_enable_timestamping()` and `intel_hal_read_timestamp()`.

## Build & Test Workflows
- **Windows:**
  - Build: `cmake .. -G "Visual Studio 16 2019" -A x64` in `build/` or `build-vs2022/`
  - Run example: `examples/Release/hal_device_info.exe`
- **Linux:**
  - Build: `cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)`
  - Run example: `sudo ./examples/hal_device_info`
- **Tests:**
  - Enable with `-DBUILD_TESTS=ON` during CMake configuration
  - Run: `./tests/hal_tests`

## Key Patterns & Conventions
- **Device IDs:** Use hex strings (e.g., `0x0DC7`) for device selection
- **Capabilities:** Query with `intel_hal_has_capability()` (e.g., `INTEL_CAP_TSN_TAS`, `INTEL_CAP_ENHANCED_TS`)
- **Thread Safety:** All public API functions are thread-safe
- **Platform Abstraction:**
  - Windows: NDIS, registry, performance counters
  - Linux: PTP hardware clock, ethtool, netlink
- **Integration:**
  - OpenAvnu: Add as submodule, link via CMake
  - Windows driver: Integrate with WinIo, Intel Driver Kit, or custom NDIS filter

## Build and Testing

### **🔧 Build Scripts:**
- **`build_windows.bat`** - Windows build automation with proper Visual Studio setup
- **CMake integration** - Standard CMake workflow for cross-platform builds

### **📁 Expected Directory Structure:**
```
intel-ethernet-hal/
├── src/
│   ├── hal/intel_hal.c          ← Core HAL implementation
│   └── windows/intel_ndis.c     ← Windows NDIS integration
├── include/intel_ethernet_hal.h  ← PRIMARY API (Service Layer interface)
├── intel.h                      ← intel_avb bridge API
├── build_windows.bat            ← Windows build script
├── examples/                    ← Integration examples
│   └── hal_enable_timestamping.c
├── tests/                       ← Validation tests
│   └── intel_hal_full_test.c
└── build/                       ← CMake build output
```

### **⚠️ Critical Build Dependencies:**
1. **intel_avb library** must be available for intel.h API
2. **Windows SDK** for NDIS OID operations  
3. **Portable format macros** throughout codebase (PRIu64, PRId64, PRIu32)

### **🔍 Testing Strategy:**
- **Hardware Validation**: All tests must run on actual Intel adapters
- **Cross-Platform**: Validate Windows and Linux implementations
- **API Consistency**: Ensure intel-ethernet-hal.h provides consistent interface
- **Error Handling**: Test failure modes and recovery patterns

## External Dependencies
- **CMake** (cross-platform build)
- **Windows:** Visual Studio 2019/2022, WinIo (optional)
- **Linux:** Kernel 3.15+ with PTP support

## Example Usage
```c
#include "intel_ethernet_hal.h"
intel_hal_init();
intel_device_info_t devices[16];
uint32_t count = 16;
intel_hal_enumerate_devices(devices, &count);
intel_hal_open_device("0x0DC7", &device);
intel_hal_enable_timestamping(device, true);
intel_hal_read_timestamp(device, &timestamp);
```

## References
- See `README.md` for hardware, platform, and integration details
- See `examples/` for practical usage
- See `tests/` for validation patterns

---
**Feedback:** If any section is unclear or missing, please specify so it can be improved for future AI agents.
