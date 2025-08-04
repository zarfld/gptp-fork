# Windows build script for GPTP project
# This script handles the build directory properly for CI/CD environments

param(
    [string]$Configuration = "Release",
    [switch]$Clean = $false
)

# Set error action preference
$ErrorActionPreference = "Stop"

Write-Host "Building GPTP project..." -ForegroundColor Green
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow

# Handle build directory
if ($Clean -or (Test-Path "build")) {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Path "build" -Recurse -Force -ErrorAction SilentlyContinue
}

# Create build directory
Write-Host "Creating build directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path "build" -Force | Out-Null

# Change to build directory
Set-Location "build"

try {
    # Configure with CMake
    Write-Host "Configuring project with CMake..." -ForegroundColor Yellow
    cmake -G "Visual Studio 17 2022" -A x64 ..
    
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }
    
    # Build the project
    Write-Host "Building project..." -ForegroundColor Yellow
    cmake --build . --config $Configuration
    
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    
    Write-Host "Build completed successfully!" -ForegroundColor Green
    
    # Show the built executable
    $exePath = ".\$Configuration\gptp.exe"
    if (Test-Path $exePath) {
        Write-Host "Executable created: $exePath" -ForegroundColor Green
        $fileInfo = Get-Item $exePath
        Write-Host "Size: $($fileInfo.Length) bytes" -ForegroundColor Gray
        Write-Host "Modified: $($fileInfo.LastWriteTime)" -ForegroundColor Gray
    }
    
} catch {
    Write-Error "Build failed: $_"
    exit 1
} finally {
    # Return to original directory
    Set-Location ..
}
