# RenderEngine One-Click Build and Package Script
# Automatically completes build, install, and packaging process

param(
    [string]$BuildType = "Release",
    [string]$Arch = "x64",
    [switch]$SkipBuild = $false,
    [switch]$SkipPackage = $false
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RenderEngine Build and Package" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$BuildDir = "build"
$InstallDir = Join-Path $BuildDir "install"

# Step 1: Configure and build
if (-not $SkipBuild) {
    Write-Host "Step 1/3: Configuring and building project..." -ForegroundColor Cyan
    
    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    # Configure CMake
    Push-Location $BuildDir
    Write-Host "  Configuring CMake..." -ForegroundColor Yellow
    cmake .. -DCMAKE_BUILD_TYPE=$BuildType -DRENDER_ENGINE_INSTALL=ON
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: CMake configuration failed" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    
    # Build project
    Write-Host "  Building project (this may take some time)..." -ForegroundColor Yellow
    cmake --build . --config $BuildType -j $env:NUMBER_OF_PROCESSORS
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Build failed" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    
    Pop-Location
    Write-Host "  Build complete!" -ForegroundColor Green
} else {
    Write-Host "Step 1/3: Skipping build (using existing build)" -ForegroundColor Yellow
}

# Step 2: Install
Write-Host "`nStep 2/3: Installing library files..." -ForegroundColor Cyan
Push-Location $BuildDir
cmake --install . --config $BuildType --prefix $InstallDir
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Installation failed" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location
Write-Host "  Installation complete!" -ForegroundColor Green

# Step 3: Package
if (-not $SkipPackage) {
    Write-Host "`nStep 3/3: Packaging prebuilt library..." -ForegroundColor Cyan
    PowerShell -ExecutionPolicy Bypass -File ".\scripts\package_prebuilt.ps1" -BuildDir $BuildDir -Config $BuildType -Arch $Arch
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Packaging failed" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "`nStep 3/3: Skipping packaging" -ForegroundColor Yellow
}

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "Complete! Prebuilt library is ready" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Install directory: $InstallDir" -ForegroundColor Cyan
if (-not $SkipPackage) {
    Write-Host "Package directory: prebuilt\RenderEngine-prebuilt-$BuildType-$Arch" -ForegroundColor Cyan
}
Write-Host ""
Write-Host "Usage:" -ForegroundColor Yellow
Write-Host "  In your project, set:" -ForegroundColor Gray
Write-Host "  set(RenderEngine_DIR `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/RenderEngine/lib/cmake/RenderEngine`")" -ForegroundColor Gray
Write-Host "  find_package(RenderEngine REQUIRED)" -ForegroundColor Gray
Write-Host "  target_link_libraries(your_target PRIVATE RenderEngine::RenderEngine)" -ForegroundColor Gray
