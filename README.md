# Intel Ethernet Hardware Abstraction Layer (HAL)

A comprehensive, cross-platform hardware abstraction layer for Intel Ethernet adapters with native Windows NDIS and Linux PTP integration.

## Overview

This library provides a unified API for Intel I210, I219, I225, and I226 Ethernet adapters across Windows and Linux platforms. It includes native OS integration for precise timestamping and TSN (Time-Sensitive Networking) capabilities.

## Supported Hardware

### Intel I210 Family
- **I210** (0x1533) - Basic IEEE 1588, MMIO, DMA
- **I210-T1** (0x1536) - Single port variant  
- **I210-IS** (0x1537) - Industrial variant

### Intel I219 Family  
- **I219-LM** (0x15B7, 0x15D7, 0x0DC7) - Corporate/vPro variants
- **I219-V** (0x15B8, 0x15D6, 0x15D8) - Consumer variants
- Features: Basic IEEE 1588, MDIO PHY access

### Intel I225 Family
- **I225-LM** (0x15F2) - Corporate 2.5 Gbps
- **I225-V** (0x15F3) - Consumer 2.5 Gbps  
- Features: Enhanced timestamping, TSN TAS/FP, PCIe PTM

### Intel I226 Family
- **I226-LM** (0x125B) - Next-gen corporate 2.5 Gbps
- **I226-V** (0x125C) - Next-gen consumer 2.5 Gbps
- Features: All I225 features + improvements

## Platform Support

### Windows
- **NDIS Integration**: Native `NDIS_TIMESTAMP_CAPABILITIES` queries
- **iphlpapi**: Adapter enumeration and management
- **Registry Access**: Hardware configuration queries
- **Performance Counters**: High-resolution timing
- **Requirements**: Windows 10 version 2004+ recommended

### Linux
- **PTP Hardware Clock**: `/dev/ptp*` device integration  
- **ethtool**: Interface configuration and capabilities
- **netlink**: Real-time interface monitoring
- **Requirements**: Linux kernel 3.15+ with PTP support

## Features

- **🎯 Precise Timestamping**: IEEE 1588 hardware timestamp support
- **🔧 Native OS APIs**: Platform-optimized implementations
- **📊 Capability Detection**: Runtime feature discovery
- **🚀 Zero-Copy DMA**: High-performance packet processing (I210/I225/I226)
- **🔗 TSN Support**: Time Aware Shaping and Frame Preemption (I225/I226)
- **⚡ Low Latency**: Direct hardware register access
- **🛡️ Thread Safe**: Multi-threaded application support

## Quick Start

### Windows Build
```powershell
# Clone repository
git clone https://github.com/zarfld/intel-ethernet-hal.git
cd intel-ethernet-hal

# Build with Visual Studio
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# Run example
.\examples\Release\hal_device_info.exe
```

### Linux Build
```bash
# Clone repository
git clone https://github.com/zarfld/intel-ethernet-hal.git
cd intel-ethernet-hal

# Build with GCC
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run example (may need sudo for hardware access)
sudo ./examples/hal_device_info
```

## API Usage

### Basic Device Detection
```c
#include "intel_ethernet_hal.h"

// Initialize HAL
intel_hal_init();

// Enumerate devices
intel_device_info_t devices[16];
uint32_t count = 16;
intel_hal_enumerate_devices(devices, &count);

printf("Found %u Intel devices\n", count);
```

### Device Opening and Timestamping
```c
// Open specific device (I219-LM)
intel_device_t *device;
intel_hal_open_device("0x0DC7", &device);

// Enable timestamping
intel_hal_enable_timestamping(device, true);

// Read current timestamp
intel_timestamp_t timestamp;
intel_hal_read_timestamp(device, &timestamp);
printf("Time: %lu.%09u\n", timestamp.seconds, timestamp.nanoseconds);

// Cleanup
intel_hal_close_device(device);
intel_hal_cleanup();
```

### Capability Checking
```c
// Check device capabilities
if (intel_hal_has_capability(device, INTEL_CAP_TSN_TAS)) {
    printf("Device supports TSN Time Aware Shaping\n");
}

if (intel_hal_has_capability(device, INTEL_CAP_ENHANCED_TS)) {
    printf("Device supports enhanced timestamping\n");
}
```

## Integration with OpenAvnu

This HAL is designed for seamless integration with OpenAvnu gPTP:

### Add as Submodule
```bash
cd /path/to/OpenAvnu
git submodule add https://github.com/zarfld/intel-ethernet-hal.git thirdparty/intel-hal
git submodule init && git submodule update
```

### CMake Integration
```cmake
# In your OpenAvnu CMakeLists.txt
add_subdirectory(thirdparty/intel-hal)
target_link_libraries(gptp intel-ethernet-hal)
```

### gPTP Usage
```c
// In gPTP daemon
#include "intel_ethernet_hal.h"

// Use HAL for precise timestamping
intel_device_t *device;
intel_hal_open_device("0x0DC7", &device);

// Get TX/RX timestamps for PTP packets
intel_timestamp_t tx_timestamp, rx_timestamp;
intel_hal_read_timestamp(device, &tx_timestamp);
```

## Windows Driver Integration

For production use on Windows, the HAL can integrate with:

1. **WinIo Library**: Direct hardware access
2. **Intel Driver Kit**: Official Intel APIs  
3. **Custom NDIS Filter**: Advanced timestamping
4. **WDK Integration**: Kernel-mode components

## Development and Testing

### Running Tests
```bash
# Build with tests enabled
cmake .. -DBUILD_TESTS=ON
make

# Run test suite
./tests/hal_tests
```

### Hardware Validation
```bash
# Test with real I219 hardware
./examples/hal_device_info

# Expected output for I219-LM:
# Found 1 Intel device(s):
# Device 1:
#   Name: I219-LM
#   Device ID: 0x0DC7
#   Capabilities: Basic IEEE 1588, MDIO PHY Access, Native OS Integration
```

## Architecture

```
┌─────────────────────────────────────┐
│           Application Layer          │
│     (OpenAvnu, gPTP, User Apps)     │
├─────────────────────────────────────┤
│         Intel Ethernet HAL          │
│      (Cross-platform API)           │
├─────────────────┬───────────────────┤
│   Windows NDIS  │   Linux PTP/PHC   │
│   Integration   │   Integration     │
├─────────────────┼───────────────────┤
│       Intel Hardware Drivers       │
│   (e1dexpress,  │  (e1000e, igb,   │
│    Windows)     │   Linux)         │
├─────────────────┴───────────────────┤
│         Intel Ethernet Hardware      │
│      (I210/I219/I225/I226)          │
└─────────────────────────────────────┘
```

## License

BSD 3-Clause License. See [LICENSE](LICENSE) for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality  
5. Submit a pull request

## Support

- **Issues**: [GitHub Issues](https://github.com/zarfld/intel-ethernet-hal/issues)
- **Documentation**: [Wiki](https://github.com/zarfld/intel-ethernet-hal/wiki)
- **Examples**: See `examples/` directory

## Changelog

### v1.0.0 (2025-01-09)
- Initial release
- Windows NDIS integration  
- Linux PTP support
- I210/I219/I225/I226 device support
- Cross-platform timestamp API
- OpenAvnu integration ready
