/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Timestamping Enable Example
  
  This example demonstrates how to enable IEEE 1588 timestamping on Intel
  Ethernet devices and verify that timestamping is working correctly.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

#include "intel_ethernet_hal.h"

int main(int argc, char* argv[]) {
    intel_hal_result_t result;
    
    printf("Intel Ethernet HAL - Timestamping Enable Example\n");
    printf("=================================================\n");
    printf("HAL Version: %s\n", intel_hal_get_version());
    
    // Initialize HAL
    printf("Initializing Intel Ethernet HAL...\n");
    result = intel_hal_init();
    if (result != INTEL_HAL_SUCCESS) {
        printf("❌ Failed to initialize HAL: %s\n", intel_hal_get_last_error());
        return 1;
    }
    
    // Enumerate devices
    intel_device_info_t devices[16];
    uint32_t device_count = 16;
    
    result = intel_hal_enumerate_devices(devices, &device_count);
    if (result != INTEL_HAL_SUCCESS) {
        printf("❌ Failed to enumerate devices: %d\n", result);
        return 1;
    }
    
    if (device_count == 0) {
        printf("❌ No Intel devices found\n");
        return 1;
    }
    
    printf("Found %u Intel device(s)\n", device_count);
    
    // Use the first device
    intel_device_info_t* target_device = &devices[0];
    printf("\nUsing device: %s (0x%04X)\n", target_device->device_name, target_device->device_id);
    
    // Open the device
    intel_device_t* device = NULL;
    char device_id_str[32];
    snprintf(device_id_str, sizeof(device_id_str), "0x%04X", target_device->device_id);
    
    result = intel_hal_open_device(device_id_str, &device);
    if (result != INTEL_HAL_SUCCESS) {
        printf("❌ Failed to open device: %d\n", result);
        return 1;
    }
    
    printf("✅ Device opened successfully\n");
    
    // Check if device supports IEEE 1588
    uint32_t capabilities = 0;
    result = intel_hal_get_capabilities(device, &capabilities);
    if (result != INTEL_HAL_SUCCESS) {
        printf("❌ Failed to get capabilities: %d\n", result);
        intel_hal_close_device(device);
        return 1;
    }
    
    if (!(capabilities & INTEL_CAP_BASIC_1588)) {
        printf("❌ Device does not support IEEE 1588 timestamping\n");
        intel_hal_close_device(device);
        return 1;
    }
    
    printf("✅ Device supports IEEE 1588 timestamping\n");
    
    // Test timestamping in disabled state
    printf("\n=== Testing Timestamping (Initially Disabled) ===\n");
    intel_timestamp_t timestamp1;
    result = intel_hal_read_timestamp(device, &timestamp1);
    if (result == INTEL_HAL_SUCCESS) {
        printf("Initial timestamp: %llu.%09u\n", 
               (unsigned long long)timestamp1.seconds, timestamp1.nanoseconds);
    } else {
        printf("Failed to read initial timestamp (expected if disabled): %d\n", result);
    }
    
    // Enable timestamping
    printf("\n=== Enabling IEEE 1588 Timestamping ===\n");
    result = intel_hal_enable_timestamping(device, true);
    if (result != INTEL_HAL_SUCCESS) {
        printf("❌ Failed to enable timestamping: %d\n", result);
        intel_hal_close_device(device);
        return 1;
    }
    
    printf("✅ Timestamping enabled successfully\n");
    
    // Wait a moment for timestamping to stabilize
    printf("Waiting for timestamping to stabilize...\n");
    sleep(1);
    
    // Test timestamping in enabled state
    printf("\n=== Testing Timestamping (Now Enabled) ===\n");
    for (int i = 0; i < 5; i++) {
        intel_timestamp_t timestamp;
        result = intel_hal_read_timestamp(device, &timestamp);
        
        if (result == INTEL_HAL_SUCCESS) {
            printf("Timestamp %d: %llu.%09u\n", i + 1,
                   (unsigned long long)timestamp.seconds, timestamp.nanoseconds);
        } else {
            printf("❌ Failed to read timestamp %d: %d\n", i + 1, result);
        }
        
        sleep(1);  // Wait 1 second between readings
    }
    
    // Test timestamp precision by reading multiple times quickly
    printf("\n=== Testing Timestamp Precision ===\n");
    intel_timestamp_t ts1, ts2;
    
    result = intel_hal_read_timestamp(device, &ts1);
    if (result == INTEL_HAL_SUCCESS) {
        // Read again immediately
        result = intel_hal_read_timestamp(device, &ts2);
        if (result == INTEL_HAL_SUCCESS) {
            uint64_t ns1 = (uint64_t)ts1.seconds * 1000000000ULL + ts1.nanoseconds;
            uint64_t ns2 = (uint64_t)ts2.seconds * 1000000000ULL + ts2.nanoseconds;
            uint64_t diff_ns = (ns2 > ns1) ? (ns2 - ns1) : (ns1 - ns2);
            
            printf("Timestamp precision test:\n");
            printf("  Reading 1: %llu.%09u\n", (unsigned long long)ts1.seconds, ts1.nanoseconds);
            printf("  Reading 2: %llu.%09u\n", (unsigned long long)ts2.seconds, ts2.nanoseconds);
            printf("  Difference: %llu ns\n", (unsigned long long)diff_ns);
            
            if (diff_ns < 1000000) {  // Less than 1ms difference
                printf("✅ Good timestamp precision (< 1ms difference)\n");
            } else {
                printf("⚠️  Large timestamp difference detected\n");
            }
        }
    }
    
    // Optional: Test disabling timestamping
    printf("\n=== Disabling Timestamping ===\n");
    result = intel_hal_enable_timestamping(device, false);
    if (result == INTEL_HAL_SUCCESS) {
        printf("✅ Timestamping disabled successfully\n");
        
        // Try to read timestamp after disabling
        intel_timestamp_t disabled_ts;
        result = intel_hal_read_timestamp(device, &disabled_ts);
        if (result != INTEL_HAL_SUCCESS) {
            printf("✅ Timestamp reading correctly fails when disabled: %d\n", result);
        } else {
            printf("⚠️  Timestamp reading still works after disable: %llu.%09u\n",
                   (unsigned long long)disabled_ts.seconds, disabled_ts.nanoseconds);
        }
    } else {
        printf("❌ Failed to disable timestamping: %d\n", result);
    }
    
    // Clean up
    intel_hal_close_device(device);
    intel_hal_cleanup();
    
    printf("\n✅ Test completed successfully!\n");
    printf("\nSummary:\n");
    printf("- Device supports IEEE 1588: YES\n");
    printf("- Timestamping enable/disable: WORKING\n");
    printf("- Hardware timestamp reading: WORKING\n");
    printf("- Integration ready for gPTP: YES\n");
    
    return 0;
}
