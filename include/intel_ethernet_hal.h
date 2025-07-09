/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Main Public API
  
  This is the main header file for the Intel Ethernet Hardware Abstraction Layer.
  It provides a unified interface for Intel I210/I219/I225/I226 adapters across
  Windows and Linux platforms with native OS integration.

******************************************************************************/

#ifndef INTEL_ETHERNET_HAL_H
#define INTEL_ETHERNET_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Version Information */
#define INTEL_ETHERNET_HAL_VERSION_MAJOR    1
#define INTEL_ETHERNET_HAL_VERSION_MINOR    0
#define INTEL_ETHERNET_HAL_VERSION_PATCH    0

/* Platform Detection */
#ifdef _WIN32
#define INTEL_HAL_WINDOWS
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ntddndis.h>
#else
#define INTEL_HAL_LINUX
#include <sys/types.h>
#include <linux/ptp_clock.h>
#endif

/* Intel Vendor ID */
#define INTEL_VENDOR_ID                     0x8086

/* Supported Intel Device IDs */
#define INTEL_DEVICE_I210_1533             0x1533  /* I210 Copper */
#define INTEL_DEVICE_I210_1536             0x1536  /* I210-T1 */
#define INTEL_DEVICE_I210_1537             0x1537  /* I210-IS */
#define INTEL_DEVICE_I219_15B7             0x15B7  /* I219-LM */
#define INTEL_DEVICE_I219_15B8             0x15B8  /* I219-V */
#define INTEL_DEVICE_I219_15D6             0x15D6  /* I219-V */
#define INTEL_DEVICE_I219_15D7             0x15D7  /* I219-LM */
#define INTEL_DEVICE_I219_15D8             0x15D8  /* I219-V */
#define INTEL_DEVICE_I219_0DC7             0x0DC7  /* I219-LM (Gen 22) */
#define INTEL_DEVICE_I225_15F2             0x15F2  /* I225-LM */
#define INTEL_DEVICE_I225_15F3             0x15F3  /* I225-V */
#define INTEL_DEVICE_I226_125B             0x125B  /* I226-LM */
#define INTEL_DEVICE_I226_125C             0x125C  /* I226-V */

/* Device Families */
typedef enum {
    INTEL_FAMILY_UNKNOWN = 0,
    INTEL_FAMILY_I210,
    INTEL_FAMILY_I219,
    INTEL_FAMILY_I225,
    INTEL_FAMILY_I226
} intel_device_family_t;

/* HAL Capabilities */
#define INTEL_CAP_BASIC_1588               (1 << 0)   /* Basic IEEE 1588 */
#define INTEL_CAP_ENHANCED_TS              (1 << 1)   /* Enhanced timestamping */
#define INTEL_CAP_TSN_TAS                  (1 << 2)   /* TSN Time Aware Shaping */
#define INTEL_CAP_TSN_FP                   (1 << 3)   /* TSN Frame Preemption */
#define INTEL_CAP_PCIe_PTM                 (1 << 4)   /* PCIe Precision Time Measurement */
#define INTEL_CAP_2_5G                     (1 << 5)   /* 2.5 Gbps speed */
#define INTEL_CAP_MMIO                     (1 << 6)   /* Memory-mapped I/O access */
#define INTEL_CAP_MDIO                     (1 << 7)   /* MDIO PHY access */
#define INTEL_CAP_DMA                      (1 << 8)   /* Direct Memory Access */
#define INTEL_CAP_NATIVE_OS                (1 << 9)   /* Native OS integration */

/* Error Codes */
typedef enum {
    INTEL_HAL_SUCCESS = 0,
    INTEL_HAL_ERROR_INVALID_PARAM = -1,
    INTEL_HAL_ERROR_NO_DEVICE = -2,
    INTEL_HAL_ERROR_NOT_SUPPORTED = -3,
    INTEL_HAL_ERROR_NO_MEMORY = -4,
    INTEL_HAL_ERROR_ACCESS_DENIED = -5,
    INTEL_HAL_ERROR_DEVICE_BUSY = -6,
    INTEL_HAL_ERROR_TIMEOUT = -7,
    INTEL_HAL_ERROR_HARDWARE = -8,
    INTEL_HAL_ERROR_OS_SPECIFIC = -9
} intel_hal_result_t;

/* Timestamp Structure */
typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
    uint32_t fractional_ns;  /* Sub-nanosecond precision */
} intel_timestamp_t;

/* Device Handle (Opaque) */
typedef struct intel_device intel_device_t;

/* Platform-specific context */
#ifdef INTEL_HAL_WINDOWS
typedef struct {
    HANDLE adapter_handle;
    LUID adapter_luid;
    DWORD adapter_index;
    char adapter_name[256];
    NDIS_TIMESTAMP_CAPABILITIES timestamp_caps;
    bool has_native_timestamp;
} intel_windows_context_t;
#endif

#ifdef INTEL_HAL_LINUX
typedef struct {
    int ptp_fd;
    int socket_fd;
    char interface_name[IFNAMSIZ];
    struct ptp_clock_caps ptp_caps;
    bool has_phc;
} intel_linux_context_t;
#endif

/* Device Information */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    intel_device_family_t family;
    uint32_t capabilities;
    char device_name[64];
    char description[256];
    
#ifdef INTEL_HAL_WINDOWS
    intel_windows_context_t windows;
#endif
    
#ifdef INTEL_HAL_LINUX
    intel_linux_context_t linux;
#endif
} intel_device_info_t;

/* Network Interface Information */
typedef struct {
    char name[64];
    uint8_t mac_address[6];
    uint32_t speed_mbps;
    bool link_up;
    bool timestamp_enabled;
} intel_interface_info_t;

/**
 * @brief Initialize the Intel Ethernet HAL
 * 
 * This must be called before any other HAL functions.
 * 
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_init(void);

/**
 * @brief Cleanup the Intel Ethernet HAL
 * 
 * This should be called when HAL is no longer needed.
 */
void intel_hal_cleanup(void);

/**
 * @brief Enumerate all supported Intel devices
 * 
 * @param[out] devices Array to store device information
 * @param[in,out] count Input: size of devices array, Output: actual count
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_enumerate_devices(intel_device_info_t *devices, uint32_t *count);

/**
 * @brief Open an Intel device for hardware access
 * 
 * @param[in] device_id PCI device ID or interface name
 * @param[out] device Pointer to store device handle
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_open_device(const char *device_id, intel_device_t **device);

/**
 * @brief Close an Intel device
 * 
 * @param[in] device Device handle to close
 */
void intel_hal_close_device(intel_device_t *device);

/**
 * @brief Get device information
 * 
 * @param[in] device Device handle
 * @param[out] info Device information structure
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_get_device_info(intel_device_t *device, intel_device_info_t *info);

/**
 * @brief Get network interface information
 * 
 * @param[in] device Device handle
 * @param[out] info Interface information structure
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_get_interface_info(intel_device_t *device, intel_interface_info_t *info);

/**
 * @brief Enable IEEE 1588 timestamping
 * 
 * @param[in] device Device handle
 * @param[in] enable True to enable, false to disable
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_enable_timestamping(intel_device_t *device, bool enable);

/**
 * @brief Read current timestamp from hardware
 * 
 * @param[in] device Device handle
 * @param[out] timestamp Current hardware timestamp
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_read_timestamp(intel_device_t *device, intel_timestamp_t *timestamp);

/**
 * @brief Set hardware timestamp
 * 
 * @param[in] device Device handle
 * @param[in] timestamp Timestamp to set
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_set_timestamp(intel_device_t *device, const intel_timestamp_t *timestamp);

/**
 * @brief Adjust hardware timestamp frequency
 * 
 * @param[in] device Device handle
 * @param[in] ppb_adjustment Parts per billion adjustment
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_adjust_frequency(intel_device_t *device, int32_t ppb_adjustment);

/**
 * @brief Get supported capabilities for device
 * 
 * @param[in] device Device handle
 * @param[out] capabilities Bitfield of supported capabilities
 * @return INTEL_HAL_SUCCESS on success, error code otherwise
 */
intel_hal_result_t intel_hal_get_capabilities(intel_device_t *device, uint32_t *capabilities);

/**
 * @brief Check if device supports specific capability
 * 
 * @param[in] device Device handle
 * @param[in] capability Capability to check (INTEL_CAP_*)
 * @return true if supported, false otherwise
 */
bool intel_hal_has_capability(intel_device_t *device, uint32_t capability);

/**
 * @brief Get HAL version string
 * 
 * @return Version string (e.g., "1.0.0")
 */
const char *intel_hal_get_version(void);

/**
 * @brief Get last error message
 * 
 * @return Human-readable error message
 */
const char *intel_hal_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* INTEL_ETHERNET_HAL_H */
