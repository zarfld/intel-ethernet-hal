cmake_minimum_required(VERSION 3.12)

# Example programs
add_executable(hal_enumerate
    hal_enumerate.c
)

target_link_libraries(hal_enumerate intel-ethernet-hal)

add_executable(hal_timestamp_test
    hal_timestamp_test.c
)

target_link_libraries(hal_timestamp_test intel-ethernet-hal)

add_executable(hal_device_info
    hal_device_info.c
)

target_link_libraries(hal_device_info intel-ethernet-hal)

# Install examples
install(TARGETS hal_enumerate hal_timestamp_test hal_device_info
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)
