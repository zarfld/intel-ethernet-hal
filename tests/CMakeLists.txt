cmake_minimum_required(VERSION 3.10)
project(IntelEthernetHalTests C)

# Tests für Intel Ethernet HAL
# Nur der vollständige Test wird gebaut (Legacy-Test entfernt)
enable_testing()

add_executable(intel_hal_full_test intel_hal_full_test.c)
target_include_directories(intel_hal_full_test PRIVATE ../include)
target_link_libraries(intel_hal_full_test PRIVATE intel-ethernet-hal-static)
