@echo off
REM Setup Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" >nul 2>&1
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" >nul 2>&1
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" >nul 2>&1
)

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Change to build directory
cd build

REM Run cmake
cmake ..
if errorlevel 1 (
    echo CMake failed!
    exit /b 1
)

REM Run msbuild
msbuild /p:Configuration=Release gptp.sln
if errorlevel 1 (
    echo MSBuild failed!
    exit /b 1
)

echo Build completed successfully!
