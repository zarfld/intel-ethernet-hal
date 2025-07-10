/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Common Device Abstraction
  
  This module provides cross-platform device abstraction for Intel adapters,
  including device identification and capability mapping.

******************************************************************************/

#include "../include/intel_ethernet_hal.h"
#include "../intel_hal_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Device capability database */
typedef struct {
    uint16_t device_id;
    intel_device_family_t family;
    uint32_t capabilities;
    const char *name;
    const char *description;
} intel_device_entry_t;

static const intel_device_entry_t intel_device_database[] = {
    /* I210 Family */
    { INTEL_DEVICE_I210_1533, INTEL_FAMILY_I210,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MMIO | INTEL_CAP_DMA | INTEL_CAP_NATIVE_OS,
      "I210", "Intel I210 Gigabit Network Connection" },
    { INTEL_DEVICE_I210_1536, INTEL_FAMILY_I210,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MMIO | INTEL_CAP_DMA | INTEL_CAP_NATIVE_OS,
      "I210-T1", "Intel I210-T1 Gigabit Network Connection" },
    { INTEL_DEVICE_I210_1537, INTEL_FAMILY_I210,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MMIO | INTEL_CAP_DMA | INTEL_CAP_NATIVE_OS,
      "I210-IS", "Intel I210-IS Gigabit Network Connection" },
    
    /* I219 Family */
    { INTEL_DEVICE_I219_15B7, INTEL_FAMILY_I219,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MDIO | INTEL_CAP_NATIVE_OS,
      "I219-LM", "Intel I219-LM Gigabit Network Connection" },
    { INTEL_DEVICE_I219_15B8, INTEL_FAMILY_I219,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MDIO | INTEL_CAP_NATIVE_OS,
      "I219-V", "Intel I219-V Gigabit Network Connection" },
    { INTEL_DEVICE_I219_15D6, INTEL_FAMILY_I219,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MDIO | INTEL_CAP_NATIVE_OS,
      "I219-V", "Intel I219-V Gigabit Network Connection" },
    { INTEL_DEVICE_I219_15D7, INTEL_FAMILY_I219,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MDIO | INTEL_CAP_NATIVE_OS,
      "I219-LM", "Intel I219-LM Gigabit Network Connection" },
    { INTEL_DEVICE_I219_15D8, INTEL_FAMILY_I219,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MDIO | INTEL_CAP_NATIVE_OS,
      "I219-V", "Intel I219-V Gigabit Network Connection" },
    { INTEL_DEVICE_I219_0DC7, INTEL_FAMILY_I219,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_MDIO | INTEL_CAP_NATIVE_OS,
      "I219-LM", "Intel I219-LM Gigabit Network Connection (Gen 22)" },
    
    /* I225 Family */
    { INTEL_DEVICE_I225_15F2, INTEL_FAMILY_I225,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_ENHANCED_TS | INTEL_CAP_TSN_TAS | 
      INTEL_CAP_TSN_FP | INTEL_CAP_PCIe_PTM | INTEL_CAP_2_5G | 
      INTEL_CAP_MMIO | INTEL_CAP_DMA | INTEL_CAP_NATIVE_OS,
      "I225-LM", "Intel I225-LM 2.5 Gigabit Network Connection" },
    { INTEL_DEVICE_I225_15F3, INTEL_FAMILY_I225,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_ENHANCED_TS | INTEL_CAP_TSN_TAS | 
      INTEL_CAP_TSN_FP | INTEL_CAP_PCIe_PTM | INTEL_CAP_2_5G | 
      INTEL_CAP_MMIO | INTEL_CAP_DMA | INTEL_CAP_NATIVE_OS,
      "I225-V", "Intel I225-V 2.5 Gigabit Network Connection" },
    
    /* I226 Family */
    { INTEL_DEVICE_I226_125B, INTEL_FAMILY_I226,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_ENHANCED_TS | INTEL_CAP_TSN_TAS | 
      INTEL_CAP_TSN_FP | INTEL_CAP_PCIe_PTM | INTEL_CAP_2_5G | 
      INTEL_CAP_MMIO | INTEL_CAP_DMA | INTEL_CAP_NATIVE_OS,
      "I226-LM", "Intel I226-LM 2.5 Gigabit Network Connection" },
    { INTEL_DEVICE_I226_125C, INTEL_FAMILY_I226,
      INTEL_CAP_BASIC_1588 | INTEL_CAP_ENHANCED_TS | INTEL_CAP_TSN_TAS | 
      INTEL_CAP_TSN_FP | INTEL_CAP_PCIe_PTM | INTEL_CAP_2_5G | 
      INTEL_CAP_MMIO | INTEL_CAP_DMA | INTEL_CAP_NATIVE_OS,
      "I226-V", "Intel I226-V 2.5 Gigabit Network Connection" },
    
    /* Terminator */
    { 0, INTEL_FAMILY_UNKNOWN, 0, NULL, NULL }
};

/**
 * @brief Lookup device information by device ID
 */
static const intel_device_entry_t *intel_lookup_device(uint16_t device_id)
{
    const intel_device_entry_t *entry;
    
    for (entry = intel_device_database; entry->device_id != 0; entry++) {
        if (entry->device_id == device_id) {
            return entry;
        }
    }
    
    return NULL;
}

/**
 * @brief Initialize device info structure from device database
 */
static intel_hal_result_t intel_init_device_info(intel_device_t *device, uint16_t device_id)
{
    const intel_device_entry_t *entry;
    intel_device_info_t *info;
    
    if (!device) {
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    entry = intel_lookup_device(device_id);
    if (!entry) {
        return INTEL_HAL_ERROR_NOT_SUPPORTED;
    }
    
    info = &device->info;
    memset(info, 0, sizeof(*info));
    
    /* Set basic device information */
    info->vendor_id = INTEL_VENDOR_ID;
    info->device_id = device_id;
    info->family = entry->family;
    info->capabilities = entry->capabilities;
    
    strncpy_s(info->device_name, sizeof(info->device_name), entry->name, _TRUNCATE);
    strncpy_s(info->description, sizeof(info->description), entry->description, _TRUNCATE);
    
    printf("Device: Initialized %s (0x%04x)\n", info->device_name, device_id);
    printf("  Family: %d, Capabilities: 0x%08x\n", info->family, info->capabilities);
    
    return INTEL_HAL_SUCCESS;
}

/**
 * @brief Create new device instance
 */
intel_device_t *intel_device_create(uint16_t device_id)
{
    intel_device_t *device;
    intel_hal_result_t result;
    
    device = (intel_device_t *)calloc(1, sizeof(intel_device_t));
    if (!device) {
        return NULL;
    }
    
    result = intel_init_device_info(device, device_id);
    if (result != INTEL_HAL_SUCCESS) {
        free(device);
        return NULL;
    }
    
    device->is_open = false;
    device->ref_count = 1;
    device->platform_data = NULL;
    
    return device;
}

/**
 * @brief Destroy device instance
 */
void intel_device_destroy(intel_device_t *device)
{
    if (!device) {
        return;
    }
    
    if (device->ref_count > 1) {
        device->ref_count--;
        return;
    }
    
    if (device->is_open) {
        printf("Warning: Destroying device that is still open\n");
    }
    
    if (device->platform_data) {
        free(device->platform_data);
    }
    
    free(device);
}

/**
 * @brief Check if device supports specific capability
 */
bool intel_device_has_capability(intel_device_t *device, uint32_t capability)
{
    if (!device) {
        return false;
    }
    
    return (device->info.capabilities & capability) != 0;
}

/**
 * @brief Get device family name string
 */
const char *intel_device_family_name(intel_device_family_t family)
{
    switch (family) {
        case INTEL_FAMILY_I210: return "I210";
        case INTEL_FAMILY_I219: return "I219";
        case INTEL_FAMILY_I225: return "I225";
        case INTEL_FAMILY_I226: return "I226";
        default: return "Unknown";
    }
}

/**
 * @brief Get capability description string
 */
const char *intel_capability_name(uint32_t capability)
{
    switch (capability) {
        case INTEL_CAP_BASIC_1588: return "Basic IEEE 1588";
        case INTEL_CAP_ENHANCED_TS: return "Enhanced Timestamping";
        case INTEL_CAP_TSN_TAS: return "TSN Time Aware Shaping";
        case INTEL_CAP_TSN_FP: return "TSN Frame Preemption";
        case INTEL_CAP_PCIe_PTM: return "PCIe Precision Time Measurement";
        case INTEL_CAP_2_5G: return "2.5 Gbps Speed";
        case INTEL_CAP_MMIO: return "Memory-mapped I/O";
        case INTEL_CAP_MDIO: return "MDIO PHY Access";
        case INTEL_CAP_DMA: return "Direct Memory Access";
        case INTEL_CAP_NATIVE_OS: return "Native OS Integration";
        default: return "Unknown Capability";
    }
}

/**
 * @brief Print device capabilities in human-readable format
 */
void intel_device_print_capabilities(intel_device_t *device)
{
    uint32_t caps;
    int cap_bit;
    
    if (!device) {
        return;
    }
    
    caps = device->info.capabilities;
    printf("Device Capabilities for %s:\n", device->info.device_name);
    
    for (cap_bit = 0; cap_bit < 32; cap_bit++) {
        uint32_t capability = 1U << cap_bit;
        if (caps & capability) {
            printf("  ✅ %s\n", intel_capability_name(capability));
        }
    }
}

/**
 * @brief Get all supported device IDs
 */
intel_hal_result_t intel_get_supported_devices(uint16_t *device_ids, uint32_t *count)
{
    const intel_device_entry_t *entry;
    uint32_t supported_count = 0;
    uint32_t max_count;
    
    if (!count) {
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    max_count = *count;
    *count = 0;
    
    /* Count supported devices */
    for (entry = intel_device_database; entry->device_id != 0; entry++) {
        if (device_ids && supported_count < max_count) {
            device_ids[supported_count] = entry->device_id;
        }
        supported_count++;
    }
    
    *count = supported_count;
    
    if (device_ids && supported_count > max_count) {
        return INTEL_HAL_ERROR_NO_MEMORY;  /* Buffer too small */
    }
    
    return INTEL_HAL_SUCCESS;
}
