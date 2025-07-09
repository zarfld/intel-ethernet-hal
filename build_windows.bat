@echo off
REM Intel Ethernet HAL - Windows Build Script
REM Builds the HAL library and examples using MinGW or Visual Studio

echo Intel Ethernet HAL - Windows Build
echo ==================================

REM Check if we're in the right directory
if not exist "include\intel_ethernet_hal.h" (
    echo ERROR: This script must be run from the intel-ethernet-hal root directory
    exit /b 1
)

echo Building Intel Ethernet HAL...

REM Try to compile with MinGW/GCC if available
where gcc >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using MinGW/GCC compiler
    
    REM Create build directory
    if not exist build mkdir build
    cd build
    
    echo Compiling sources...
    
    REM Compile common sources
    gcc -c -I../include -DINTEL_HAL_WINDOWS -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN ^
        ../src/common/intel_device.c -o intel_device.o
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Failed to compile intel_device.c
        cd ..
        exit /b 1
    )
    
    REM Compile HAL main
    gcc -c -I../include -DINTEL_HAL_WINDOWS -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN ^
        ../src/hal/intel_hal.c -o intel_hal.o
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Failed to compile intel_hal.c
        cd ..
        exit /b 1
    )
    
    REM Compile Windows NDIS
    gcc -c -I../include -DINTEL_HAL_WINDOWS -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN ^
        ../src/windows/intel_ndis.c -o intel_ndis.o
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Failed to compile intel_ndis.c
        cd ..
        exit /b 1
    )
    
    echo Creating static library...
    ar rcs libintel-ethernet-hal.a intel_device.o intel_hal.o intel_ndis.o
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Failed to create static library
        cd ..
        exit /b 1
    )
    
    echo Compiling example program...
    gcc -I../include -DINTEL_HAL_WINDOWS -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN ^
        ../examples/hal_device_info.c libintel-ethernet-hal.a ^
        -liphlpapi -lws2_32 -lsetupapi -ladvapi32 ^
        -o hal_device_info.exe
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Failed to compile example program
        cd ..
        exit /b 1
    )
    
    cd ..
    
    echo.
    echo ✅ Build completed successfully!
    echo.
    echo Files created:
    echo   build\libintel-ethernet-hal.a  - Static library
    echo   build\hal_device_info.exe      - Example program
    echo.
    echo To test with your I219 hardware:
    echo   cd build
    echo   .\hal_device_info.exe
    echo.
    
) else (
    echo ERROR: GCC compiler not found in PATH
    echo.
    echo Please install one of:
    echo   - MinGW-w64 (https://www.mingw-w64.org/)
    echo   - MSYS2 (https://www.msys2.org/)
    echo   - Visual Studio with C++ tools
    echo.
    exit /b 1
)
