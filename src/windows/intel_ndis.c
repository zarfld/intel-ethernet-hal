/******************************************************************************

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Intel Ethernet HAL - Windows NDIS Integration
  
  This module provides Windows-specific NDIS integration for Intel adapters,
  including native timestamp capabilities and driver interaction.

******************************************************************************/

#include "../include/intel_ethernet_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    vsnprintf_s(last_error_message, sizeof(last_error_message), _TRUNCATE, format, args);
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
    snprintf_s(adapter_path, sizeof(adapter_path), _TRUNCATE,
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
    printf("  Software timestamp: %s\n", caps.SoftwareTimestampFlags ? "YES" : "NO");
    printf("  Cross timestamp: %s\n", caps.CrossTimestamp ? "YES" : "NO");
    
    return INTEL_HAL_SUCCESS;
}

/**
 * @brief Find Intel adapter using Windows IP Helper API
 */
static intel_hal_result_t find_intel_adapter_by_device_id(uint16_t device_id, 
                                                         intel_device_info_t *info)
{
    PIP_ADAPTER_ADDRESSES adapters = NULL;
    PIP_ADAPTER_ADDRESSES current_adapter;
    ULONG buffer_length = 0;
    DWORD result;
    bool found = false;
    
    /* Get required buffer size */
    result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, 
                                 adapters, &buffer_length);
    
    if (result != ERROR_BUFFER_OVERFLOW) {
        set_last_error("GetAdaptersAddresses size query failed: %lu", result);
        return INTEL_HAL_ERROR_OS_SPECIFIC;
    }
    
    /* Allocate buffer */
    adapters = (PIP_ADAPTER_ADDRESSES)malloc(buffer_length);
    if (!adapters) {
        set_last_error("Failed to allocate adapter buffer");
        return INTEL_HAL_ERROR_NO_MEMORY;
    }
    
    /* Get adapter information */
    result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL,
                                 adapters, &buffer_length);
    
    if (result != NO_ERROR) {
        free(adapters);
        set_last_error("GetAdaptersAddresses failed: %lu", result);
        return INTEL_HAL_ERROR_OS_SPECIFIC;
    }
    
    /* Search for Intel adapter with matching device ID */
    for (current_adapter = adapters; current_adapter; current_adapter = current_adapter->Next) {
        /* Check if this is an Ethernet adapter */
        if (current_adapter->IfType != IF_TYPE_ETHERNET_CSMACD) {
            continue;
        }
        
        /* Get PCI information from registry */
        HKEY adapter_key;
        char registry_path[512];
        snprintf_s(registry_path, sizeof(registry_path), _TRUNCATE,
                   "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}\\%04lu",
                   current_adapter->IfIndex);
        
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, registry_path, 0, KEY_READ, &adapter_key) == ERROR_SUCCESS) {
            char hardware_id[256];
            DWORD data_size = sizeof(hardware_id);
            DWORD data_type;
            
            if (RegQueryValueEx(adapter_key, "MatchingDeviceId", NULL, &data_type,
                               (LPBYTE)hardware_id, &data_size) == ERROR_SUCCESS) {
                
                /* Parse hardware ID for vendor and device */
                if (strstr(hardware_id, "VEN_8086") && 
                    strstr(hardware_id, "DEV_") &&
                    strtoul(strstr(hardware_id, "DEV_") + 4, NULL, 16) == device_id) {
                    
                    /* Found matching Intel adapter */
                    strncpy_s(info->windows.adapter_name, sizeof(info->windows.adapter_name),
                             current_adapter->AdapterName, _TRUNCATE);
                    info->windows.adapter_index = current_adapter->IfIndex;
                    info->windows.adapter_luid = current_adapter->Luid;
                    
                    /* Copy basic information */
                    info->vendor_id = INTEL_VENDOR_ID;
                    info->device_id = device_id;
                    
                    /* Get adapter description */
                    WideCharToMultiByte(CP_UTF8, 0, current_adapter->Description, -1,
                                       info->description, sizeof(info->description), NULL, NULL);
                    
                    found = true;
                    RegCloseKey(adapter_key);
                    break;
                }
            }
            RegCloseKey(adapter_key);
        }
    }
    
    free(adapters);
    
    if (!found) {
        set_last_error("Intel adapter with device ID 0x%04x not found", device_id);
        return INTEL_HAL_ERROR_NO_DEVICE;
    }
    
    printf("Windows: Found Intel adapter - %s\n", info->description);
    printf("  Adapter name: %s\n", info->windows.adapter_name);
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
        DWORD bytes_returned;
        LARGE_INTEGER hw_timestamp;
        
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
