// intel_hal_full_test.c
// Vollständiger Test für alle unterstützten Intel-Adapter
// Erkennt, prüft und loggt alle relevanten HAL-Funktionen pro Adapter

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Verhindert Konflikte mit winsock.h
#ifdef _WINSOCKAPI_
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include "intel_ethernet_hal.h"

#define LOGFILE "intel_hal_test_log.txt"

void log_device_info(FILE* log, const intel_device_info_t* info, int idx) {
    fprintf(log, "Device %d:\n", idx + 1);
    fprintf(log, "  Name: %s\n", info->device_name);
    fprintf(log, "  Description: %s\n", info->description);
    fprintf(log, "  Vendor ID: 0x%04X\n", info->vendor_id);
    fprintf(log, "  Device ID: 0x%04X\n", info->device_id);
    fprintf(log, "  Family: %d\n", info->family);
    fprintf(log, "  Capabilities: 0x%08X\n", info->capabilities);
}

int main(void) {
    FILE* log = fopen(LOGFILE, "w");
    if (!log) {
        printf("[FAIL] Konnte Logfile nicht öffnen!\n");
        return 1;
    }
    fprintf(log, "Intel Ethernet HAL - Vollständiger Systemtest\n");
    fprintf(log, "============================================\n");
    fprintf(log, "HAL Version: %s\n\n", intel_hal_get_version());

    // HAL initialisieren
    if (intel_hal_init() != INTEL_HAL_SUCCESS) {
        fprintf(log, "[FAIL] HAL-Initialisierung fehlgeschlagen: %s\n", intel_hal_get_last_error());
        fclose(log);
        return 2;
    }

    // Geräte auflisten
    intel_device_info_t devices[16];
    uint32_t device_count = 16;
    if (intel_hal_enumerate_devices(devices, &device_count) != INTEL_HAL_SUCCESS) {
        fprintf(log, "[FAIL] Geräteerkennung fehlgeschlagen: %s\n", intel_hal_get_last_error());
        intel_hal_cleanup();
        fclose(log);
        return 3;
    }
    fprintf(log, "Gefundene Geräte: %u\n\n", device_count);
    if (device_count == 0) {
        fprintf(log, "[WARN] Keine unterstützten Intel-Adapter gefunden.\n");
        intel_hal_cleanup();
        fclose(log);
        return 0;
    }

    // Für jedes Gerät: Infos, Capabilities, Timestamping testen
    for (uint32_t i = 0; i < device_count; ++i) {
        log_device_info(log, &devices[i], i);
        intel_device_t* dev = NULL;
        char dev_id[32];
        snprintf(dev_id, sizeof(dev_id), "0x%04X", devices[i].device_id);
        if (intel_hal_open_device(dev_id, &dev) != INTEL_HAL_SUCCESS) {
            fprintf(log, "  [FAIL] Gerät konnte nicht geöffnet werden: %s\n\n", intel_hal_get_last_error());
            continue;
        }
        // Interface-Info
        intel_interface_info_t iface;
        if (intel_hal_get_interface_info(dev, &iface) == INTEL_HAL_SUCCESS) {
            fprintf(log, "  Interface: %s\n", iface.name);
            fprintf(log, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                iface.mac_address[0], iface.mac_address[1], iface.mac_address[2],
                iface.mac_address[3], iface.mac_address[4], iface.mac_address[5]);
            fprintf(log, "  Speed: %u Mbps\n", iface.speed_mbps);
            fprintf(log, "  Link: %s\n", iface.link_up ? "UP" : "DOWN");
        } else {
            fprintf(log, "  [WARN] Interface-Info nicht lesbar: %s\n", intel_hal_get_last_error());
        }
        // Capabilities prüfen
        uint32_t caps = 0;
        if (intel_hal_get_capabilities(dev, &caps) == INTEL_HAL_SUCCESS) {
            fprintf(log, "  Capabilities: 0x%08X\n", caps);
            if (caps & INTEL_CAP_BASIC_1588) {
                fprintf(log, "    [OK] IEEE 1588 Timestamping unterstützt\n");
                // Timestamping aktivieren
                if (intel_hal_enable_timestamping(dev, true) == INTEL_HAL_SUCCESS) {
                    fprintf(log, "    [OK] Timestamping aktiviert\n");
                    // Mehrfaches Auslesen des Timestamps im Abstand von 200ms
                    intel_timestamp_t ts[5];
                    bool ts_ok = true;
                    for (int t = 0; t < 5; ++t) {
                        if (intel_hal_read_timestamp(dev, &ts[t]) == INTEL_HAL_SUCCESS) {
                            fprintf(log, "    [OK] Timestamp[%d]: %llu.%09u\n",
                                t, (unsigned long long)ts[t].seconds, ts[t].nanoseconds);
                        } else {
                            fprintf(log, "    [FAIL] Timestamp[%d] konnte nicht gelesen werden: %s\n", t, intel_hal_get_last_error());
                            ts_ok = false;
                        }
                        if (t < 4) {
#ifdef _WIN32
                            Sleep(200); // 200ms Pause
#else
                            usleep(200000);
#endif
                        }
                    }
                    // Prüfen, ob sich die Timestamps ändern
                    bool ts_changed = false;
                    for (int t = 1; t < 5; ++t) {
                        if (ts[t].seconds != ts[t-1].seconds || ts[t].nanoseconds != ts[t-1].nanoseconds) {
                            ts_changed = true;
                            break;
                        }
                    }
                    if (ts_ok && ts_changed) {
                        fprintf(log, "    [OK] Timestamps ändern sich wie erwartet.\n");
                    } else if (ts_ok) {
                        fprintf(log, "    [FAIL] Timestamps bleiben konstant!\n");
                    }
                } else {
                    fprintf(log, "    [FAIL] Timestamping konnte nicht aktiviert werden: %s\n", intel_hal_get_last_error());
                }
            } else {
                fprintf(log, "    [WARN] IEEE 1588 Timestamping nicht unterstützt\n");
            }
        } else {
            fprintf(log, "  [WARN] Capabilities nicht lesbar: %s\n", intel_hal_get_last_error());
        }
        intel_hal_close_device(dev);
        fprintf(log, "\n");
    }
    intel_hal_cleanup();
    fprintf(log, "[DONE] Test abgeschlossen.\n");
    fclose(log);
    printf("Test abgeschlossen. Ergebnisse in %s\n", LOGFILE);
    return 0;
}
