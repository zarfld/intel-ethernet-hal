// hal_device_test.c
// Testet die wichtigsten Funktionen der Intel Ethernet HAL für I219, I210, I225
// Kompiliert als eigenständiges Testprogramm

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Annahme: Diese Header sind Teil der HAL und verfügbar
#include "hal_device.h"
#include "hal_timestamp.h"

void print_device_info(const hal_device_info_t* info) {
    printf("Device Info:\n");
    printf("  Name: %s\n", info->name);
    printf("  PCI-ID: %s\n", info->pci_id);
    printf("  Driver: %s\n", info->driver_version);
    printf("  Firmware: %s\n", info->firmware_version);
    printf("  Supports HW Timestamping: %s\n", info->supports_hw_timestamping ? "YES" : "NO");
    printf("  PTP Capabilities: %s\n", info->ptp_capabilities);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <device_name>\n", argv[0]);
        printf("Example: %s i225\n", argv[0]);
        return 1;
    }

    const char* dev_name = argv[1];
    hal_device_handle_t dev = hal_open_device(dev_name);
    if (!dev) {
        printf("[FAIL] Device '%s' konnte nicht geöffnet werden!\n", dev_name);
        return 2;
    }

    hal_device_info_t info;
    if (hal_get_device_info(dev, &info) != 0) {
        printf("[FAIL] Device-Info konnte nicht gelesen werden!\n");
        hal_close_device(dev);
        return 3;
    }
    print_device_info(&info);

    // Timestamping aktivieren
    if (hal_enable_hw_timestamping(dev) != 0) {
        printf("[FAIL] Hardware-Timestamping konnte nicht aktiviert werden!\n");
    } else {
        printf("[OK] Hardware-Timestamping aktiviert.\n");
    }

    // Test: Registerzugriff (z.B. MAC-Adresse lesen)
    uint8_t mac[6] = {0};
    if (hal_read_mac_address(dev, mac) == 0) {
        printf("[OK] MAC-Adresse: %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        printf("[WARN] MAC-Adresse konnte nicht gelesen werden.\n");
    }

    // Optional: Teste weitere Register (z.B. Statusregister)
    uint32_t status = 0;
    if (hal_read_register(dev, 0x00008, &status) == 0) { // Beispiel: Status-Register
        printf("[OK] Status-Register (0x00008): 0x%08X\n", status);
    } else {
        printf("[WARN] Status-Register konnte nicht gelesen werden.\n");
    }

    hal_close_device(dev);
    printf("[DONE] Test abgeschlossen.\n");
    return 0;
}
