/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Private/Internal Definitions
  
  This header contains internal definitions shared between HAL source files
  but not exposed in the public API.

******************************************************************************/

#ifndef INTEL_ETHERNET_HAL_PRIVATE_H
#define INTEL_ETHERNET_HAL_PRIVATE_H

#include "../include/intel_ethernet_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal device structure definition */
struct intel_device {
    intel_device_info_t info;
    bool is_open;
    void *platform_data;
    uint32_t ref_count;
};

/* Platform-specific function declarations */
#ifdef INTEL_HAL_WINDOWS
intel_hal_result_t intel_windows_init_device(intel_device_t *device, uint16_t device_id);
void intel_windows_cleanup_device(intel_device_t *device);
intel_hal_result_t intel_windows_read_timestamp(intel_device_t *device, intel_timestamp_t *timestamp);
const char *intel_windows_get_last_error(void);
bool intel_windows_has_modern_ndis_support(void);
intel_hal_result_t find_intel_adapter_by_device_id(uint16_t device_id, intel_device_info_t *info);
#endif

#ifdef INTEL_HAL_LINUX
intel_hal_result_t intel_linux_init_device(intel_device_t *device, uint16_t device_id);
void intel_linux_cleanup_device(intel_device_t *device);
intel_hal_result_t intel_linux_read_timestamp(intel_device_t *device, intel_timestamp_t *timestamp);
const char *intel_linux_get_last_error(void);
#endif

/* Common device functions */
intel_device_t *intel_device_create(uint16_t device_id);
void intel_device_destroy(intel_device_t *device);
bool intel_device_has_capability(intel_device_t *device, uint32_t capability);
void intel_device_print_capabilities(intel_device_t *device);
intel_hal_result_t intel_get_supported_devices(uint16_t *device_ids, uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* INTEL_ETHERNET_HAL_PRIVATE_H */
