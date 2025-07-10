cmake_minimum_required(VERSION 3.12)

# Example programs
add_executable(hal_device_info
    hal_device_info.c
)

add_executable(hal_enable_timestamping
    hal_enable_timestamping.c
)

target_link_libraries(hal_device_info intel-ethernet-hal-static)
target_link_libraries(hal_enable_timestamping intel-ethernet-hal-static)

# Install examples
install(TARGETS hal_device_info hal_enable_timestamping
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)
