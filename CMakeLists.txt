cmake_minimum_required(VERSION 3.12)

project(intel-ethernet-hal 
    VERSION 1.0.0
    DESCRIPTION "Intel Ethernet Hardware Abstraction Layer"
    LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
    set(INTEL_HAL_WINDOWS ON)
    add_definitions(-DINTEL_HAL_WINDOWS)
elseif(UNIX AND NOT APPLE)
    set(INTEL_HAL_LINUX ON)
    add_definitions(-DINTEL_HAL_LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Compiler-specific options
if(MSVC)
    # Windows/MSVC options
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
else()
    # GCC/Clang options
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Intel AVB integration - include the required source files
set(INTEL_AVB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/intel_avb)
set(INTEL_AVB_SOURCES
    ${INTEL_AVB_DIR}/lib/intel_i210.c
    ${INTEL_AVB_DIR}/lib/intel_i217.c
    ${INTEL_AVB_DIR}/lib/intel_i219.c
    ${INTEL_AVB_DIR}/lib/intel_i225.c
    ${INTEL_AVB_DIR}/lib/intel_i226.c
    ${INTEL_AVB_DIR}/lib/intel_common.c
    ${INTEL_AVB_DIR}/lib/intel.c
    ${INTEL_AVB_DIR}/lib/intel_windows.c
)

# Include Intel AVB headers
include_directories(${INTEL_AVB_DIR}/lib)
include_directories(${INTEL_AVB_DIR}/include)

# Common source files
set(COMMON_SOURCES
    src/common/intel_device.c
    src/hal/intel_hal.c
    ${INTEL_AVB_SOURCES}
)

# Platform-specific sources
if(INTEL_HAL_WINDOWS)
    set(PLATFORM_SOURCES
        src/windows/intel_ndis.c
    )
    
    # Windows-specific libraries
    set(PLATFORM_LIBRARIES
        iphlpapi
        ws2_32
        setupapi
        advapi32
    )
elseif(INTEL_HAL_LINUX)
    set(PLATFORM_SOURCES
        src/linux/intel_ptp.c
        src/linux/intel_ethtool.c
    )
    
    # Linux-specific libraries
    set(PLATFORM_LIBRARIES
        pthread
        rt
    )
endif()

# Create the main library
add_library(intel-ethernet-hal SHARED
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
)

# Link platform-specific libraries
target_link_libraries(intel-ethernet-hal ${PLATFORM_LIBRARIES})

# Set library properties
set_target_properties(intel-ethernet-hal PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "include/intel_ethernet_hal.h"
)

# Static library variant
add_library(intel-ethernet-hal-static STATIC
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(intel-ethernet-hal-static ${PLATFORM_LIBRARIES})

set_target_properties(intel-ethernet-hal-static PROPERTIES
    OUTPUT_NAME intel-ethernet-hal-static
)

# Installation
include(GNUInstallDirs)

install(TARGETS intel-ethernet-hal intel-ethernet-hal-static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/intel-ethernet-hal-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/intel-ethernet-hal-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/intel-ethernet-hal
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/intel-ethernet-hal-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/intel-ethernet-hal-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/intel-ethernet-hal-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/intel-ethernet-hal
)

# Print configuration summary
message(STATUS "")
message(STATUS "Intel Ethernet HAL Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
if(INTEL_HAL_WINDOWS)
    message(STATUS "  Windows NDIS Support: Enabled")
endif()
if(INTEL_HAL_LINUX)
    message(STATUS "  Linux PTP Support: Enabled")
endif()
message(STATUS "")
