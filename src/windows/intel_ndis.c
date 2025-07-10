/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Windows NDIS Integration
  
  This module provides Windows-specific NDIS integration for Intel adapters,
  including native timestamp capabilities and driver interaction.

******************************************************************************/

#include "../include/intel_ethernet_hal.h"
#include "../intel_hal_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ntddndis.h>
#include <winioctl.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

/* Windows-specific error handling */
static char last_error_message[512] = {0};

static void set_last_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    _vsnprintf_s(last_error_message, sizeof(last_error_message), _TRUNCATE, format, args);
    va_end(args);
}

/**
 * @brief Query NDIS timestamp capabilities for an adapter
 */
static intel_hal_result_t query_ndis_timestamp_caps(intel_device_t *device)
{
    HANDLE adapter_handle = INVALID_HANDLE_VALUE;
    DWORD bytes_returned = 0;
    NDIS_OID oid_request;
    NDIS_TIMESTAMP_CAPABILITIES caps;
    intel_device_info_t *info = &device->info;
    
    /* Open adapter handle for NDIS queries */
    char adapter_path[512];
    _snprintf_s(adapter_path, sizeof(adapter_path), _TRUNCATE,
               "\\\\.\\Global\\NDIS_Adapter_%s", info->windows.adapter_name);
    
    adapter_handle = CreateFile(adapter_path, 
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
    
    if (adapter_handle == INVALID_HANDLE_VALUE) {
        set_last_error("Failed to open NDIS adapter handle: %lu", GetLastError());
        return INTEL_HAL_ERROR_ACCESS_DENIED;
    }
    
    /* Query timestamp capabilities */
    memset(&caps, 0, sizeof(caps));
    
    if (!DeviceIoControl(adapter_handle, IOCTL_NDIS_QUERY_GLOBAL_STATS,
                        &oid_request, sizeof(oid_request),
                        &caps, sizeof(caps),
                        &bytes_returned, NULL)) {
        DWORD error = GetLastError();
        CloseHandle(adapter_handle);
        
        if (error == ERROR_NOT_SUPPORTED) {
            /* Adapter doesn't support native timestamping */
            info->windows.has_native_timestamp = false;
            return INTEL_HAL_SUCCESS;
        }
        
        set_last_error("NDIS timestamp capability query failed: %lu", error);
        return INTEL_HAL_ERROR_OS_SPECIFIC;
    }
    
    /* Store timestamp capabilities */
    memcpy(&info->windows.timestamp_caps, &caps, sizeof(caps));
    info->windows.has_native_timestamp = true;
    info->windows.adapter_handle = adapter_handle;
    
    printf("Windows NDIS: Native timestamp support detected\n");
    printf("  Hardware timestamp: %s\n", caps.HardwareClockFrequencyHz ? "YES" : "NO");
    printf("  Hardware frequency: %I64u Hz\n", caps.HardwareClockFrequencyHz);
    printf("  Cross timestamp: %s\n", caps.CrossTimestamp ? "YES" : "NO");
    
    return INTEL_HAL_SUCCESS;
}

/**
 * @brief Find Intel adapter using Windows IP Helper API
 */
static intel_hal_result_t find_intel_adapter_by_device_id(uint16_t device_id, 
                                                         intel_device_info_t *info)
{
    HKEY network_class_key;
    char registry_path[] = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}";
    bool found = false;
    
    /* Open the network device class registry key */
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, registry_path, 0, KEY_READ, &network_class_key) != ERROR_SUCCESS) {
        set_last_error("Failed to open network device class registry key");
        return INTEL_HAL_ERROR_OS_SPECIFIC;
    }
    
    /* Enumerate all network device instances */
    for (DWORD index = 0; !found; index++) {
        char subkey_name[256];
        DWORD name_size = sizeof(subkey_name);
        
        /* Get next device subkey name */
        if (RegEnumKeyEx(network_class_key, index, subkey_name, &name_size, 
                        NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
            break; /* No more devices */
        }
        
        /* Skip non-numeric device keys (like "Properties") */
        if (!isdigit(subkey_name[0])) {
            continue;
        }
        
        /* Open this device's registry key */
        HKEY device_key;
        if (RegOpenKeyEx(network_class_key, subkey_name, 0, KEY_READ, &device_key) == ERROR_SUCCESS) {
            /* Check hardware ID */
            char hardware_id[512];
            DWORD data_size = sizeof(hardware_id);
            DWORD data_type;
            
            /* Try MatchingDeviceId first, then HardwareID */
            if (RegQueryValueEx(device_key, "MatchingDeviceId", NULL, &data_type,
                               (LPBYTE)hardware_id, &data_size) != ERROR_SUCCESS) {
                data_size = sizeof(hardware_id);
                RegQueryValueEx(device_key, "HardwareID", NULL, &data_type,
                               (LPBYTE)hardware_id, &data_size);
            }
            
            /* Check if this is an Intel device with matching device ID */
            if (strstr(hardware_id, "VEN_8086") && strstr(hardware_id, "DEV_")) {
                char *dev_id_str = strstr(hardware_id, "DEV_") + 4;
                uint16_t found_device_id = (uint16_t)strtoul(dev_id_str, NULL, 16);
                
                if (found_device_id == device_id) {
                    /* Found matching Intel adapter */
                    info->vendor_id = INTEL_VENDOR_ID;
                    info->device_id = device_id;
                    
                    /* Get device description */
                    data_size = sizeof(info->description);
                    if (RegQueryValueEx(device_key, "DriverDesc", NULL, &data_type,
                                       (LPBYTE)info->description, &data_size) != ERROR_SUCCESS) {
                        _snprintf_s(info->description, sizeof(info->description), _TRUNCATE,
                                   "Intel Device 0x%04X", device_id);
                    }
                    
                    /* Get NetCfgInstanceId for adapter name */
                    data_size = sizeof(info->windows.adapter_name);
                    if (RegQueryValueEx(device_key, "NetCfgInstanceId", NULL, &data_type,
                                       (LPBYTE)info->windows.adapter_name, &data_size) != ERROR_SUCCESS) {
                        _snprintf_s(info->windows.adapter_name, sizeof(info->windows.adapter_name), _TRUNCATE,
                                   "Intel_0x%04X", device_id);
                    }
                    
                    found = true;
                }
            }
            
            RegCloseKey(device_key);
        }
    }
    
    RegCloseKey(network_class_key);
    
    if (!found) {
        set_last_error("Intel device 0x%04X not found in system", device_id);
        return INTEL_HAL_ERROR_NO_DEVICE;
    
    printf("Windows: Found Intel adapter - %s\n", info->description);
    printf("  Adapter name: %s\n", info->windows.adapter_name);
    
    return INTEL_HAL_SUCCESS;
}
    printf("  Interface index: %lu\n", info->windows.adapter_index);
    
    return INTEL_HAL_SUCCESS;
}

/**
 * @brief Initialize Windows-specific functionality for Intel device
 */
intel_hal_result_t intel_windows_init_device(intel_device_t *device, uint16_t device_id)
{
    intel_hal_result_t result;
    
    if (!device) {
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    /* Find adapter using IP Helper API */
    result = find_intel_adapter_by_device_id(device_id, &device->info);
    if (result != INTEL_HAL_SUCCESS) {
        return result;
    }
    
    /* Query NDIS timestamp capabilities */
    result = query_ndis_timestamp_caps(device);
    if (result != INTEL_HAL_SUCCESS) {
        printf("Windows: Warning - NDIS timestamp query failed, using fallback methods\n");
        /* Continue with basic functionality */
        device->info.windows.has_native_timestamp = false;
    }
    
    /* Initialize Windows Sockets if needed */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        set_last_error("WSAStartup failed: %d", WSAGetLastError());
        return INTEL_HAL_ERROR_OS_SPECIFIC;
    }
    
    printf("Windows: Device initialization complete\n");
    return INTEL_HAL_SUCCESS;
}

/**
 * @brief Cleanup Windows-specific resources
 */
void intel_windows_cleanup_device(intel_device_t *device)
{
    if (!device) {
        return;
    }
    
    if (device->info.windows.adapter_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(device->info.windows.adapter_handle);
        device->info.windows.adapter_handle = INVALID_HANDLE_VALUE;
    }
    
    WSACleanup();
}

/**
 * @brief Read timestamp using Windows native methods
 */
intel_hal_result_t intel_windows_read_timestamp(intel_device_t *device, intel_timestamp_t *timestamp)
{
    if (!device || !timestamp) {
        return INTEL_HAL_ERROR_INVALID_PARAM;
    }
    
    if (device->info.windows.has_native_timestamp) {
        /* Use NDIS native timestamp if available */
        DWORD bytes_returned = 0;
        LARGE_INTEGER hw_timestamp = {0};
        
        if (DeviceIoControl(device->info.windows.adapter_handle,
                           IOCTL_NDIS_QUERY_GLOBAL_STATS,
                           NULL, 0,
                           &hw_timestamp, sizeof(hw_timestamp),
                           &bytes_returned, NULL)) {
            
            /* Convert Windows timestamp to Intel format */
            timestamp->seconds = hw_timestamp.QuadPart / 1000000000ULL;
            timestamp->nanoseconds = hw_timestamp.QuadPart % 1000000000ULL;
            timestamp->fractional_ns = 0;
            
            return INTEL_HAL_SUCCESS;
        }
    }
    
    /* Fallback to Windows performance counter */
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
        set_last_error("QueryPerformanceCounter failed");
        return INTEL_HAL_ERROR_OS_SPECIFIC;
    }
    
    /* Convert to nanoseconds */
    uint64_t ns_total = (counter.QuadPart * 1000000000ULL) / frequency.QuadPart;
    timestamp->seconds = ns_total / 1000000000ULL;
    timestamp->nanoseconds = ns_total % 1000000000ULL;
    timestamp->fractional_ns = 0;
    
    return INTEL_HAL_SUCCESS;
}

/**
 * @brief Get Windows-specific error message
 */
const char *intel_windows_get_last_error(void)
{
    return last_error_message;
}

/**
 * @brief Check if running on Windows 10 version 2004+ for best NDIS support
 */
bool intel_windows_has_modern_ndis_support(void)
{
    OSVERSIONINFOEX version_info;
    memset(&version_info, 0, sizeof(version_info));
    version_info.dwOSVersionInfoSize = sizeof(version_info);
    
    if (GetVersionEx((OSVERSIONINFO*)&version_info)) {
        /* Windows 10 version 2004 = build 19041 */
        return (version_info.dwMajorVersion > 10) ||
               (version_info.dwMajorVersion == 10 && version_info.dwBuildNumber >= 19041);
    }
    
    return false;
}
