/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Device Information Example
  
  This example demonstrates how to use the Intel Ethernet HAL to detect
  and query information about Intel network adapters.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intel_ethernet_hal.h"

int main(int argc, char *argv[])
{
    intel_hal_result_t result;
    intel_device_info_t devices[16];
    uint32_t device_count = sizeof(devices) / sizeof(devices[0]);
    uint32_t i;
    
    printf("Intel Ethernet HAL - Device Information Example\n");
    printf("===============================================\n");
    printf("HAL Version: %s\n", intel_hal_get_version());
    printf("\n");
    
    /* Initialize HAL */
    result = intel_hal_init();
    if (result != INTEL_HAL_SUCCESS) {
        printf("ERROR: Failed to initialize HAL: %s\n", intel_hal_get_last_error());
        return 1;
    }
    
    /* Enumerate devices */
    printf("Enumerating Intel devices...\n");
    result = intel_hal_enumerate_devices(devices, &device_count);
    if (result != INTEL_HAL_SUCCESS) {
        printf("ERROR: Failed to enumerate devices: %s\n", intel_hal_get_last_error());
        intel_hal_cleanup();
        return 1;
    }
    
    if (device_count == 0) {
        printf("No Intel Ethernet devices found.\n");
        printf("\nSupported devices:\n");
        printf("  - I210 (0x1533, 0x1536, 0x1537)\n");
        printf("  - I219 (0x15B7, 0x15B8, 0x15D6, 0x15D7, 0x15D8, 0x0DC7)\n");
        printf("  - I225 (0x15F2, 0x15F3)\n");
        printf("  - I226 (0x125B, 0x125C)\n");
        intel_hal_cleanup();
        return 0;
    }
    
    printf("Found %u Intel device(s):\n\n", device_count);
    
    /* Display device information */
    for (i = 0; i < device_count; i++) {
        intel_device_info_t *info = &devices[i];
        
        printf("Device %u:\n", i + 1);
        printf("  Name: %s\n", info->device_name);
        printf("  Description: %s\n", info->description);
        printf("  Vendor ID: 0x%04X\n", info->vendor_id);
        printf("  Device ID: 0x%04X\n", info->device_id);
        printf("  Family: %s\n", 
               (info->family == INTEL_FAMILY_I210) ? "I210" :
               (info->family == INTEL_FAMILY_I219) ? "I219" :
               (info->family == INTEL_FAMILY_I225) ? "I225" :
               (info->family == INTEL_FAMILY_I226) ? "I226" : "Unknown");
        
        printf("  Capabilities (0x%08X):\n", info->capabilities);
        if (info->capabilities & INTEL_CAP_BASIC_1588)    printf("    ✅ Basic IEEE 1588\n");
        if (info->capabilities & INTEL_CAP_ENHANCED_TS)   printf("    ✅ Enhanced Timestamping\n");
        if (info->capabilities & INTEL_CAP_TSN_TAS)       printf("    ✅ TSN Time Aware Shaping\n");
        if (info->capabilities & INTEL_CAP_TSN_FP)        printf("    ✅ TSN Frame Preemption\n");
        if (info->capabilities & INTEL_CAP_PCIe_PTM)      printf("    ✅ PCIe Precision Time Measurement\n");
        if (info->capabilities & INTEL_CAP_2_5G)          printf("    ✅ 2.5 Gbps Speed\n");
        if (info->capabilities & INTEL_CAP_MMIO)          printf("    ✅ Memory-mapped I/O\n");
        if (info->capabilities & INTEL_CAP_MDIO)          printf("    ✅ MDIO PHY Access\n");
        if (info->capabilities & INTEL_CAP_DMA)           printf("    ✅ Direct Memory Access\n");
        if (info->capabilities & INTEL_CAP_NATIVE_OS)     printf("    ✅ Native OS Integration\n");
        
#ifdef INTEL_HAL_WINDOWS
        printf("  Windows Info:\n");
        printf("    Adapter Name: %s\n", info->windows.adapter_name);
        printf("    Interface Index: %lu\n", info->windows.adapter_index);
        printf("    Native Timestamp: %s\n", info->windows.has_native_timestamp ? "YES" : "NO");
        
        if (info->windows.has_native_timestamp) {
            printf("    Hardware Clock: %s\n", 
                   info->windows.timestamp_caps.HardwareClockFrequencyHz ? "YES" : "NO");
            printf("    Hardware Frequency: %I64u Hz\n",
                   info->windows.timestamp_caps.HardwareClockFrequencyHz);
            printf("    Cross Timestamp: %s\n",
                   info->windows.timestamp_caps.CrossTimestamp ? "YES" : "NO");
        }
#endif

#ifdef INTEL_HAL_LINUX
        printf("  Linux Info:\n");
        printf("    Interface: %s\n", info->linux.interface_name);
        printf("    PTP Hardware Clock: %s\n", info->linux.has_phc ? "YES" : "NO");
        
        if (info->linux.has_phc) {
            printf("    Max Adjustments: %d\n", info->linux.ptp_caps.max_adj);
            printf("    Number of Alarms: %d\n", info->linux.ptp_caps.n_alarm);
            printf("    Number of External Timestamps: %d\n", info->linux.ptp_caps.n_ext_ts);
            printf("    Number of Periodic Outputs: %d\n", info->linux.ptp_caps.n_per_out);
            printf("    Number of Pins: %d\n", info->linux.ptp_caps.n_pins);
        }
#endif
        
        printf("\n");
    }
    
    /* Test opening a device if one was found */
    if (device_count > 0) {
        printf("Testing device open/close...\n");
        
        char device_id_str[16];
        snprintf(device_id_str, sizeof(device_id_str), "0x%04X", devices[0].device_id);
        
        intel_device_t *device = NULL;
        result = intel_hal_open_device(device_id_str, &device);
        if (result == INTEL_HAL_SUCCESS) {
            printf("✅ Successfully opened device 0x%04X\n", devices[0].device_id);
            
            /* Test interface info */
            intel_interface_info_t interface_info;
            if (intel_hal_get_interface_info(device, &interface_info) == INTEL_HAL_SUCCESS) {
                printf("  Interface: %s\n", interface_info.name);
                printf("  Speed: %u Mbps\n", interface_info.speed_mbps);
                printf("  Link Up: %s\n", interface_info.link_up ? "YES" : "NO");
                printf("  Timestamping: %s\n", interface_info.timestamp_enabled ? "ENABLED" : "DISABLED");
            }
            
            /* Test timestamp reading if supported */
            if (intel_hal_has_capability(device, INTEL_CAP_BASIC_1588)) {
                printf("  Testing timestamp read...\n");
                intel_timestamp_t timestamp;
                if (intel_hal_read_timestamp(device, &timestamp) == INTEL_HAL_SUCCESS) {
                    printf("  ✅ Current timestamp: %I64u.%09u\n", 
                           timestamp.seconds, timestamp.nanoseconds);
                } else {
                    printf("  ⚠️  Timestamp read failed: %s\n", intel_hal_get_last_error());
                }
            }
            
            intel_hal_close_device(device);
        } else {
            printf("❌ Failed to open device: %s\n", intel_hal_get_last_error());
        }
    }
    
    /* Cleanup */
    intel_hal_cleanup();
    
    printf("\nExample completed successfully!\n");
    return 0;
}
