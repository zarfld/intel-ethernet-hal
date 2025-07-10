# Intel Ethernet HAL Integration with OpenAvnu

This document describes how to integrate the Intel Ethernet HAL with OpenAvnu components, particularly the gPTP daemon.

## Architecture Overview

The Intel Ethernet HAL serves as a **timestamp provider** for OpenAvnu components:

- **Windows**: Registers as NDIS timestamp provider for system-wide timestamp access
- **Linux**: Integrates with PTP Hardware Clock (`/dev/ptp*`) infrastructure
- **Integration**: gPTP, mrpd, and maap can query Intel HAL for precision timestamps
- **Fallback**: Standard OS timestamping when Intel HAL unavailable

### NDIS Provider Model (Windows)

The Intel HAL registers as an NDIS timestamp provider that OpenAvnu components can query:

```c
// Intel HAL registers as Windows timestamp provider
intel_ndis_provider_t *provider;
intel_register_ndis_provider(device_ctx, &provider);

// gPTP queries Intel timestamp provider
intel_timestamp_t timestamp;
if (intel_get_current_timestamp(&timestamp) == INTEL_TIMESTAMP_SUCCESS) {
    // Use Intel hardware timestamp
    gptp_process_timestamp(&timestamp);
} else {
    // Fall back to standard Windows timestamping
    fallback_get_timestamp(&timestamp);
}
```

## HAL Integration

The Intel Ethernet HAL provides a unified interface for Intel I210/I219/I225/I226 adapters across Windows and Linux platforms. It enables precise IEEE 1588 timestamping and TSN capabilities for OpenAvnu.

## CMake Integration

The HAL is automatically built when OpenAvnu is configured:

```bash
cd OpenAvnu
mkdir build && cd build
cmake .. -DOPENAVNU_BUILD_INTEL_HAL=ON
```

## Using Intel HAL with gPTP

### 1. Device Detection

```c
#include "intel_ethernet_hal.h"

// Initialize HAL
intel_hal_init();

// Find Intel adapters
intel_device_info_t devices[16];
uint32_t count = 16;
intel_hal_enumerate_devices(devices, &count);

// Look for suitable adapter for gPTP
for (uint32_t i = 0; i < count; i++) {
    if (intel_hal_has_capability(&devices[i], INTEL_CAP_BASIC_1588)) {
        printf("Found PTP-capable adapter: %s\n", devices[i].device_name);
        // Use this adapter for gPTP
        break;
    }
}
```

### 2. Timestamp Integration

```c
// Open Intel device
intel_device_t *device;
intel_hal_open_device("0x0DC7", &device);  // I219-LM

// Enable timestamping
intel_hal_enable_timestamping(device, true);

// In gPTP TX path
void gptp_send_packet(void *packet, size_t len) {
    // Send packet via normal network stack
    send_packet(packet, len);
    
    // Get TX timestamp from Intel HAL
    intel_timestamp_t tx_timestamp;
    if (intel_hal_read_timestamp(device, &tx_timestamp) == INTEL_HAL_SUCCESS) {
        // Use precise hardware timestamp for gPTP calculations
        gptp_process_tx_timestamp(&tx_timestamp);
    }
}

// In gPTP RX path
void gptp_receive_packet(void *packet, size_t len) {
    // Get RX timestamp from Intel HAL
    intel_timestamp_t rx_timestamp;
    if (intel_hal_read_timestamp(device, &rx_timestamp) == INTEL_HAL_SUCCESS) {
        // Use precise hardware timestamp for gPTP calculations
        gptp_process_rx_timestamp(&rx_timestamp);
    }
    
    // Process packet
    gptp_process_packet(packet, len);
}
```

### 3. Frequency Adjustment

```c
// Implement gPTP frequency adjustment using Intel HAL
void gptp_adjust_frequency(int32_t ppb_adjustment) {
    if (intel_hal_adjust_frequency(device, ppb_adjustment) == INTEL_HAL_SUCCESS) {
        printf("Frequency adjusted by %d ppb\n", ppb_adjustment);
    }
}
```

## Windows-Specific Integration

### NDIS Timestamp Capabilities

```c
// Check Windows NDIS timestamp support
intel_device_info_t info;
intel_hal_get_device_info(device, &info);

#ifdef INTEL_HAL_WINDOWS
if (info.windows.has_native_timestamp) {
    printf("Native Windows timestamping available\n");
    printf("Hardware clock: %s\n", 
           info.windows.timestamp_caps.HardwareClockFrequencyHz ? "YES" : "NO");
}
#endif
```

### Integration with Windows Sockets

```c
// Use Intel HAL with Windows Sockets for gPTP
SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

// Bind to Intel adapter interface
struct sockaddr_in addr;
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = adapter_ip;  // From Intel HAL interface info
bind(sock, (struct sockaddr*)&addr, sizeof(addr));

// Use Intel HAL for timestamping
intel_timestamp_t timestamp;
intel_hal_read_timestamp(device, &timestamp);
```

## Linux-Specific Integration

### PTP Hardware Clock Integration

```c
// Check Linux PTP support
intel_device_info_t info;
intel_hal_get_device_info(device, &info);

#ifdef INTEL_HAL_LINUX
if (info.linux.has_phc) {
    printf("PTP Hardware Clock available: %s\n", info.linux.interface_name);
    printf("Max frequency adjustment: %d ppb\n", info.linux.ptp_caps.max_adj);
}
#endif
```

## CMakeLists.txt Example for gPTP Integration

```cmake
# In thirdparty/gptp/CMakeLists.txt

# Check if Intel HAL is available
if(OPENAVNU_INTEL_HAL_AVAILABLE)
    message(STATUS "gPTP: Intel HAL support enabled")
    
    # Link Intel HAL
    target_link_libraries(gptp intel-ethernet-hal)
    
    # Add Intel HAL include directory
    target_include_directories(gptp PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/../intel-ethernet-hal/include)
    
    # Enable Intel HAL features
    target_compile_definitions(gptp PRIVATE INTEL_HAL_SUPPORT=1)
else()
    message(STATUS "gPTP: Intel HAL not available, using standard timestamping")
endif()
```

## Configuration Example

### OpenAvnu Configuration with Intel HAL

```ini
# openavnu.ini
[gptp]
intel_hal_enabled=1
intel_device_id=0x0DC7
timestamp_source=intel_hal
frequency_adjustment=intel_hal

[intel_hal]
device_preference=I219,I225,I226,I210
enable_enhanced_timestamping=1
enable_tsn_features=1
```

## Error Handling

```c
// Robust error handling with Intel HAL
intel_hal_result_t result = intel_hal_read_timestamp(device, &timestamp);

switch (result) {
    case INTEL_HAL_SUCCESS:
        // Use timestamp
        break;
    case INTEL_HAL_ERROR_NOT_SUPPORTED:
        // Fall back to OS timestamping
        get_os_timestamp(&timestamp);
        break;
    case INTEL_HAL_ERROR_HARDWARE:
        // Hardware issue, report error
        log_error("Intel HAL hardware error: %s", intel_hal_get_last_error());
        break;
    default:
        // Other error
        log_warning("Intel HAL error: %s", intel_hal_get_last_error());
        break;
}
```

## Performance Considerations

### High-Performance Timestamping

```c
// Optimize for low-latency timestamping
static intel_device_t *cached_device = NULL;
static intel_timestamp_t last_timestamp;

// Initialize once
if (!cached_device) {
    intel_hal_open_device("0x0DC7", &cached_device);
    intel_hal_enable_timestamping(cached_device, true);
}

// Fast timestamp read
static inline int get_fast_timestamp(intel_timestamp_t *ts) {
    return intel_hal_read_timestamp(cached_device, ts);
}
```

## Testing and Validation

### Hardware Timestamp Validation

```c
// Test Intel HAL timestamp accuracy
void test_timestamp_accuracy(void) {
    intel_timestamp_t ts1, ts2;
    
    intel_hal_read_timestamp(device, &ts1);
    usleep(1000);  // 1ms delay
    intel_hal_read_timestamp(device, &ts2);
    
    uint64_t diff_ns = (ts2.seconds - ts1.seconds) * 1000000000ULL + 
                       (ts2.nanoseconds - ts1.nanoseconds);
    
    printf("Timestamp diff: %lu ns (expected ~1000000)\n", diff_ns);
    
    if (diff_ns > 900000 && diff_ns < 1100000) {
        printf("✅ Timestamp accuracy test passed\n");
    } else {
        printf("❌ Timestamp accuracy test failed\n");
    }
}
```

This integration provides OpenAvnu with native Intel hardware support for precise timestamping and TSN capabilities, significantly improving gPTP performance on supported hardware.
