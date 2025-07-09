/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Main Implementation
  
  This is the main implementation file that ties together all platform-specific
  modules and provides the public HAL API.

******************************************************************************/

#include "../include/intel_ethernet_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-specific functions */
#ifdef INTEL_HAL_WINDOWS
extern intel_hal_result_t intel_windows_init_device(intel_device_t *device, uint16_t device_id);
extern void intel_windows_cleanup_device(intel_device_t *device);
extern intel_hal_result_t intel_windows_read_timestamp(intel_device_t *device, intel_timestamp_t *timestamp);
extern const char *intel_windows_get_last_error(void);
extern bool intel_windows_has_modern_ndis_support(void);
#endif

#ifdef INTEL_HAL_LINUX
extern intel_hal_result_t intel_linux_init_device(intel_device_t *device, uint16_t device_id);
extern void intel_linux_cleanup_device(intel_device_t *device);
extern intel_hal_result_t intel_linux_read_timestamp(intel_device_t *device, intel_timestamp_t *timestamp);
extern const char *intel_linux_get_last_error(void);
#endif

/* Common device functions */
extern intel_device_t *intel_device_create(uint16_t device_id);
extern void intel_device_destroy(intel_device_t *device);
extern bool intel_device_has_capability(intel_device_t *device, uint32_t capability);
extern void intel_device_print_capabilities(intel_device_t *device);
extern intel_hal_result_t intel_get_supported_devices(uint16_t *device_ids, uint32_t *count);

/* Global HAL state */
static bool hal_initialized = false;
static char hal_last_error[512] = {0};

/* Internal helper functions */
static void set_hal_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf_s(hal_last_error, sizeof(hal_last_error), _TRUNCATE, format, args);
    va_end(args);
}

static uint16_t parse_device_id(const char *device_id_str)
{
    if (!device_id_str) {
        return 0;
    }
    
    /* Try to parse as hex number */
    if (strncmp(device_id_str, "0x", 2) == 0) {
        return (uint16_t)strtoul(device_id_str, NULL, 16);
    }
    
    /* Try to parse as decimal */
    return (uint16_t)strtoul(device_id_str, NULL, 10);
}

/* Public API Implementation */

intel_hal_result_t intel_hal_init(void)
{
    if (hal_initialized) {
        return INTEL_HAL_SUCCESS;
    }
    
    printf("Intel Ethernet HAL v%d.%d.%d Initializing...\n",
           INTEL_ETHERNET_HAL_VERSION_MAJOR,
           INTEL_ETHERNET_HAL_VERSION_MINOR, 
           INTEL_ETHERNET_HAL_VERSION_PATCH);
    
#ifdef INTEL_HAL_WINDOWS
    printf("Platform: Windows\n");
    if (intel_windows_has_modern_ndis_support()) {
        printf("NDIS: Modern timestamp support available\n");
    } else {
        printf("NDIS: Legacy support mode\n");
    }
#endif

#ifdef INTEL_HAL_LINUX
    printf("Platform: Linux\n");
#endif
    
    hal_initialized = true;
    
    /* Print supported devices */
    uint16_t device_ids[32];
    uint32_t count = sizeof(device_ids) / sizeof(device_ids[0]);
    
    if (intel_get_supported_devices(device_ids, &count) == INTEL_HAL_SUCCESS) {
        printf("Supported devices: %u\n", count);
        for (uint32_t i = 0; i < count; i++) {
            printf("  - 0x%04x\n", device_ids[i]);
        }
    }
    
    printf("Intel Ethernet HAL initialized successfully\n");
    return INTEL_HAL_SUCCESS;
}

void intel_hal_cleanup(void)
{
    if (!hal_initialized) {
        return;
    }
    
    printf("Intel Ethernet HAL cleanup\n");
    hal_initialized = false;
}

intel_hal_result_t intel_hal_enumerate_devices(intel_device_info_t *devices, uint32_t *count)
{
    uint16_t device_ids[32];
    uint32_t device_count, i;
    intel_hal_result_t result;
    
    if (!hal_initialized) {
        set_hal_error("HAL not initialized");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    if (!count) {
        set_hal_error("Count parameter is NULL");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    /* Get supported device list */
    device_count = sizeof(device_ids) / sizeof(device_ids[0]);
    result = intel_get_supported_devices(device_ids, &device_count);
    if (result != INTEL_HAL_SUCCESS) {
        set_hal_error("Failed to get supported devices");
        return result;
    }
    
    /* Try to find each device on the system */
    uint32_t found_count = 0;
    uint32_t max_devices = *count;
    
    for (i = 0; i < device_count && found_count < max_devices; i++) {
        intel_device_t *device = intel_device_create(device_ids[i]);
        if (device) {
            /* Try platform-specific initialization to see if device exists */
#ifdef INTEL_HAL_WINDOWS
            if (intel_windows_init_device(device, device_ids[i]) == INTEL_HAL_SUCCESS) {
                if (devices) {
                    memcpy(&devices[found_count], &device->info, sizeof(intel_device_info_t));
                }
                found_count++;
                intel_windows_cleanup_device(device);
            }
#endif

#ifdef INTEL_HAL_LINUX
            if (intel_linux_init_device(device, device_ids[i]) == INTEL_HAL_SUCCESS) {
                if (devices) {
                    memcpy(&devices[found_count], &device->info, sizeof(intel_device_info_t));
                }
                found_count++;
                intel_linux_cleanup_device(device);
            }
#endif
            intel_device_destroy(device);
        }
    }
    
    *count = found_count;
    printf("HAL: Found %u Intel devices\n", found_count);
    
    return INTEL_HAL_SUCCESS;
}

intel_hal_result_t intel_hal_open_device(const char *device_id, intel_device_t **device)
{
    uint16_t device_id_num;
    intel_device_t *new_device;
    intel_hal_result_t result;
    
    if (!hal_initialized) {
        set_hal_error("HAL not initialized");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    if (!device_id || !device) {
        set_hal_error("Invalid parameters");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    /* Parse device ID */
    device_id_num = parse_device_id(device_id);
    if (device_id_num == 0) {
        set_hal_error("Invalid device ID: %s", device_id);
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    /* Create device instance */
    new_device = intel_device_create(device_id_num);
    if (!new_device) {
        set_hal_error("Failed to create device instance for 0x%04x", device_id_num);
        return INTEL_HAL_ERROR_NO_MEMORY;
    }
    
    /* Platform-specific initialization */
#ifdef INTEL_HAL_WINDOWS
    result = intel_windows_init_device(new_device, device_id_num);
    if (result != INTEL_HAL_SUCCESS) {
        set_hal_error("Windows device initialization failed: %s", intel_windows_get_last_error());
        intel_device_destroy(new_device);
        return result;
    }
#endif

#ifdef INTEL_HAL_LINUX
    result = intel_linux_init_device(new_device, device_id_num);
    if (result != INTEL_HAL_SUCCESS) {
        set_hal_error("Linux device initialization failed: %s", intel_linux_get_last_error());
        intel_device_destroy(new_device);
        return result;
    }
#endif
    
    new_device->is_open = true;
    *device = new_device;
    
    printf("HAL: Device 0x%04x opened successfully\n", device_id_num);
    intel_device_print_capabilities(new_device);
    
    return INTEL_HAL_SUCCESS;
}

void intel_hal_close_device(intel_device_t *device)
{
    if (!device) {
        return;
    }
    
    if (!device->is_open) {
        printf("Warning: Closing device that is not open\n");
        return;
    }
    
    printf("HAL: Closing device 0x%04x\n", device->info.device_id);
    
    /* Platform-specific cleanup */
#ifdef INTEL_HAL_WINDOWS
    intel_windows_cleanup_device(device);
#endif

#ifdef INTEL_HAL_LINUX
    intel_linux_cleanup_device(device);
#endif
    
    device->is_open = false;
    intel_device_destroy(device);
}

intel_hal_result_t intel_hal_get_device_info(intel_device_t *device, intel_device_info_t *info)
{
    if (!device || !info) {
        set_hal_error("Invalid parameters");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    memcpy(info, &device->info, sizeof(intel_device_info_t));
    return INTEL_HAL_SUCCESS;
}

intel_hal_result_t intel_hal_get_interface_info(intel_device_t *device, intel_interface_info_t *info)
{
    if (!device || !info) {
        set_hal_error("Invalid parameters");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    memset(info, 0, sizeof(intel_interface_info_t));
    
#ifdef INTEL_HAL_WINDOWS
    /* Get interface information from Windows */
    strncpy_s(info->name, sizeof(info->name), device->info.windows.adapter_name, _TRUNCATE);
    info->speed_mbps = 1000; /* Default to 1 Gbps, query actual speed if needed */
    info->link_up = true;    /* Assume link is up for now */
    info->timestamp_enabled = device->info.windows.has_native_timestamp;
#endif
    
#ifdef INTEL_HAL_LINUX
    /* Get interface information from Linux */
    strncpy_s(info->name, sizeof(info->name), device->info.linux.interface_name, _TRUNCATE);
    info->speed_mbps = 1000;
    info->link_up = true;
    info->timestamp_enabled = device->info.linux.has_phc;
#endif
    
    return INTEL_HAL_SUCCESS;
}

intel_hal_result_t intel_hal_enable_timestamping(intel_device_t *device, bool enable)
{
    if (!device) {
        set_hal_error("Invalid device");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    if (!intel_device_has_capability(device, INTEL_CAP_BASIC_1588)) {
        set_hal_error("Device does not support timestamping");
        return INTEL_HAL_ERROR_NOT_SUPPORTED;
    }
    
    printf("HAL: %s timestamping for device 0x%04x\n", 
           enable ? "Enabling" : "Disabling", device->info.device_id);
    
    /* Platform-specific timestamp enable/disable would go here */
    
    return INTEL_HAL_SUCCESS;
}

intel_hal_result_t intel_hal_read_timestamp(intel_device_t *device, intel_timestamp_t *timestamp)
{
    if (!device || !timestamp) {
        set_hal_error("Invalid parameters");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    if (!intel_device_has_capability(device, INTEL_CAP_BASIC_1588)) {
        set_hal_error("Device does not support timestamping");
        return INTEL_HAL_ERROR_NOT_SUPPORTED;
    }
    
#ifdef INTEL_HAL_WINDOWS
    return intel_windows_read_timestamp(device, timestamp);
#endif

#ifdef INTEL_HAL_LINUX
    return intel_linux_read_timestamp(device, timestamp);
#endif

    return INTEL_HAL_ERROR_NOT_SUPPORTED;
}

intel_hal_result_t intel_hal_set_timestamp(intel_device_t *device, const intel_timestamp_t *timestamp)
{
    if (!device || !timestamp) {
        set_hal_error("Invalid parameters");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    if (!intel_device_has_capability(device, INTEL_CAP_BASIC_1588)) {
        set_hal_error("Device does not support timestamping");
        return INTEL_HAL_ERROR_NOT_SUPPORTED;
    }
    
    printf("HAL: Setting timestamp to %lu.%09u for device 0x%04x\n",
           timestamp->seconds, timestamp->nanoseconds, device->info.device_id);
    
    /* Platform-specific timestamp setting would go here */
    
    return INTEL_HAL_SUCCESS;
}

intel_hal_result_t intel_hal_adjust_frequency(intel_device_t *device, int32_t ppb_adjustment)
{
    if (!device) {
        set_hal_error("Invalid device");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    if (!intel_device_has_capability(device, INTEL_CAP_BASIC_1588)) {
        set_hal_error("Device does not support frequency adjustment");
        return INTEL_HAL_ERROR_NOT_SUPPORTED;
    }
    
    printf("HAL: Adjusting frequency by %d ppb for device 0x%04x\n",
           ppb_adjustment, device->info.device_id);
    
    /* Platform-specific frequency adjustment would go here */
    
    return INTEL_HAL_SUCCESS;
}

intel_hal_result_t intel_hal_get_capabilities(intel_device_t *device, uint32_t *capabilities)
{
    if (!device || !capabilities) {
        set_hal_error("Invalid parameters");
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    *capabilities = device->info.capabilities;
    return INTEL_HAL_SUCCESS;
}

bool intel_hal_has_capability(intel_device_t *device, uint32_t capability)
{
    return intel_device_has_capability(device, capability);
}

const char *intel_hal_get_version(void)
{
    static char version_string[32];
    snprintf_s(version_string, sizeof(version_string), _TRUNCATE, "%d.%d.%d",
               INTEL_ETHERNET_HAL_VERSION_MAJOR,
               INTEL_ETHERNET_HAL_VERSION_MINOR,
               INTEL_ETHERNET_HAL_VERSION_PATCH);
    return version_string;
}

const char *intel_hal_get_last_error(void)
{
    return hal_last_error;
}
