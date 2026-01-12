# RenderEngine Prebuilt Library Packaging Script
# Used to generate prebuilt library packages for direct use

param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "prebuilt",
    [string]$Config = "Release",
    [string]$Arch = "x64"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RenderEngine Prebuilt Library Packager" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Get absolute paths before changing directory
$CurrentDir = Get-Location
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDirAbs = Join-Path $CurrentDir.Path $BuildDir
} else {
    $BuildDirAbs = $BuildDir
}
$BuildDirAbs = [System.IO.Path]::GetFullPath($BuildDirAbs)

# Check build directory
if (-not (Test-Path $BuildDirAbs)) {
    Write-Host "Error: Build directory does not exist: $BuildDirAbs" -ForegroundColor Red
    Write-Host "Please run build command first: cmake --build $BuildDir --config $Config" -ForegroundColor Yellow
    exit 1
}

# Create output directory (also use absolute path)
if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $CurrentDir.Path $OutputDir
}
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

$PackageName = "RenderEngine-prebuilt-$Config-$Arch"
$PackagePath = Join-Path $OutputDir $PackageName

if (Test-Path $PackagePath) {
    Write-Host "Cleaning old package directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $PackagePath
}

New-Item -ItemType Directory -Path $PackagePath -Force | Out-Null
Write-Host "Created package directory: $PackagePath" -ForegroundColor Green

# 1. Install library to temporary directory
# Use absolute path to ensure correct location
$InstallDir = Join-Path $BuildDirAbs "install"

Write-Host "`nStep 1/5: Installing library files..." -ForegroundColor Cyan
Write-Host "  Build directory: $BuildDirAbs" -ForegroundColor Gray
Write-Host "  Install directory: $InstallDir" -ForegroundColor Gray

# Save current directory (should be project root)
$ProjectRoot = Get-Location

# Check if CMake cache exists and configure if needed
Push-Location $BuildDirAbs
if (-not (Test-Path "CMakeCache.txt")) {
    Write-Host "  CMake not configured. Configuring with RENDER_ENGINE_INSTALL=ON..." -ForegroundColor Yellow
    cmake .. -DRENDER_ENGINE_INSTALL=ON -DCMAKE_BUILD_TYPE=$Config
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: CMake configuration failed" -ForegroundColor Red
        Pop-Location
        exit 1
    }
} else {
    # Reconfigure to ensure RENDER_ENGINE_INSTALL is ON
    Write-Host "  Ensuring RENDER_ENGINE_INSTALL is enabled..." -ForegroundColor Yellow
    cmake .. -DRENDER_ENGINE_INSTALL=ON -DCMAKE_BUILD_TYPE=$Config
}

# Install from build directory using absolute path
Write-Host "  Running cmake --install..." -ForegroundColor Yellow
cmake --install . --config $Config --prefix $InstallDir
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Installation failed" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

# Determine actual install directory (may be in build/build/install if cmake used relative path)
# CMake interprets --prefix relative to current directory when in build dir, so check both locations
$ExpectedDir = $InstallDir
$AltInstallDir = Join-Path (Join-Path $BuildDirAbs "build") "install"

Write-Host "  Checking installation locations..." -ForegroundColor Gray
Write-Host "    Expected: $ExpectedDir\lib" -ForegroundColor Gray
Write-Host "    Alternative: $AltInstallDir\lib" -ForegroundColor Gray

# Check which location actually has the installed files
$ActualInstallDir = $null
if (Test-Path (Join-Path $ExpectedDir "lib")) {
    $ActualInstallDir = $ExpectedDir
    Write-Host "  Found at expected location: $ActualInstallDir" -ForegroundColor Green
} elseif (Test-Path (Join-Path $AltInstallDir "lib")) {
    $ActualInstallDir = $AltInstallDir
    Write-Host "  Found at alternative location: $ActualInstallDir" -ForegroundColor Yellow
}

if (-not $ActualInstallDir) {
    Write-Host "  Error: Could not find installed files in any location" -ForegroundColor Red
    Write-Host "    Checked: $ExpectedDir\lib" -ForegroundColor Red
    Write-Host "    Checked: $AltInstallDir\lib" -ForegroundColor Red
    exit 1
}

# 2. Copy library files
Write-Host "`nStep 2/5: Copying library files..." -ForegroundColor Cyan
$LibSource = Join-Path $ActualInstallDir "lib"
$LibDest = Join-Path $PackagePath "lib"
if (Test-Path $LibSource) {
    Copy-Item -Recurse $LibSource $LibDest -Force
    Write-Host "  Copied library files to: $LibDest" -ForegroundColor Green
} else {
    Write-Host "  Warning: Library directory not found at: $LibSource" -ForegroundColor Yellow
    Write-Host "  This may indicate that installation did not complete successfully." -ForegroundColor Yellow
}

# 3. Copy header files
Write-Host "`nStep 3/5: Copying header files..." -ForegroundColor Cyan
$IncludeSource = Join-Path $ActualInstallDir "include"
$IncludeDest = Join-Path $PackagePath "include"
if (Test-Path $IncludeSource) {
    Copy-Item -Recurse $IncludeSource $IncludeDest -Force
    Write-Host "  Copied header files to: $IncludeDest" -ForegroundColor Green
} else {
    Write-Host "  Warning: Include directory not found at: $IncludeSource" -ForegroundColor Yellow
    Write-Host "  This may indicate that installation did not complete successfully." -ForegroundColor Yellow
}

# 4. Copy CMake configuration files
Write-Host "`nStep 4/5: Copying CMake configuration files..." -ForegroundColor Cyan
$CMakeSource = Join-Path $ActualInstallDir "lib\cmake\RenderEngine"
$CMakeDest = Join-Path $PackagePath "lib\cmake\RenderEngine"
if (Test-Path $CMakeSource) {
    New-Item -ItemType Directory -Path $CMakeDest -Force | Out-Null
    Copy-Item -Recurse $CMakeSource\* $CMakeDest -Force
    Write-Host "  Copied CMake configuration files to: $CMakeDest" -ForegroundColor Green
} else {
    Write-Host "  Warning: CMake configuration directory not found at: $CMakeSource" -ForegroundColor Yellow
    Write-Host "  This may indicate that installation did not complete successfully." -ForegroundColor Yellow
}

# 5. Copy shader files
Write-Host "`nStep 5/5: Copying shader files..." -ForegroundColor Cyan
$ShaderSource = Join-Path $ActualInstallDir "share\RenderEngine\shaders"
$ShaderDest = Join-Path $PackagePath "share\RenderEngine\shaders"
if (Test-Path $ShaderSource) {
    New-Item -ItemType Directory -Path $ShaderDest -Force | Out-Null
    Copy-Item -Recurse $ShaderSource\* $ShaderDest -Force
    Write-Host "  Copied shader files to: $ShaderDest" -ForegroundColor Green
} else {
    Write-Host "  Warning: Shader directory not found at: $ShaderSource" -ForegroundColor Yellow
    Write-Host "  This may indicate that installation did not complete successfully." -ForegroundColor Yellow
}

# 6. Create README file
Write-Host "`nCreating README documentation..." -ForegroundColor Cyan
$BuildDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

# Build content using StringBuilder to avoid encoding issues
$sb = New-Object System.Text.StringBuilder
$null = $sb.AppendLine("# RenderEngine Prebuilt Library")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("## Version Information")
$null = $sb.AppendLine("- Build Configuration: $Config")
$null = $sb.AppendLine("- Architecture: $Arch")
$null = $sb.AppendLine("- Build Date: $BuildDate")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("## Directory Structure")
$null = $sb.AppendLine('```')
$null = $sb.AppendLine("$PackageName/")
$null = $sb.AppendLine("  lib/                    # Library files")
$null = $sb.AppendLine("    cmake/")
$null = $sb.AppendLine("      RenderEngine/   # CMake configuration files")
$null = $sb.AppendLine("  include/                # Header files")
$null = $sb.AppendLine("    render/           # RenderEngine headers")
$null = $sb.AppendLine("    SDL3/             # SDL3 headers")
$null = $sb.AppendLine("    imgui.h           # ImGui headers")
$null = $sb.AppendLine("    backends/         # ImGui backend headers")
$null = $sb.AppendLine("    json/             # nlohmann/json headers")
$null = $sb.AppendLine("  share/")
$null = $sb.AppendLine("    RenderEngine/")
$null = $sb.AppendLine("      shaders/        # Shader files")
$null = $sb.AppendLine('```')
$null = $sb.AppendLine("")
$null = $sb.AppendLine("## Usage")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("### Method 1: Using CMake find_package (Recommended)")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("Add the following to your CMakeLists.txt:")
$null = $sb.AppendLine("")
$null = $sb.AppendLine('```cmake')
$null = $sb.AppendLine('# Set RenderEngine path')
$null = $sb.AppendLine("set(RenderEngine_DIR `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/cmake/RenderEngine`")")
$null = $sb.AppendLine('')
$null = $sb.AppendLine('# Find RenderEngine')
$null = $sb.AppendLine('find_package(RenderEngine REQUIRED)')
$null = $sb.AppendLine('')
$null = $sb.AppendLine('# Link to your target')
$null = $sb.AppendLine('target_link_libraries(your_target PRIVATE RenderEngine::RenderEngine)')
$null = $sb.AppendLine('```')
$null = $sb.AppendLine("")
$null = $sb.AppendLine("### Method 2: Direct Include")
$null = $sb.AppendLine("")
$null = $sb.AppendLine('```cmake')
$null = $sb.AppendLine('# Add header file paths')
$null = $sb.AppendLine('target_include_directories(your_target PRIVATE ')
$null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/include`"")
$null = $sb.AppendLine(')')
$null = $sb.AppendLine('')
$null = $sb.AppendLine('# Link library files')
$null = $sb.AppendLine('target_link_libraries(your_target PRIVATE ')
$null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/RenderEngine.lib`"")
$null = $sb.AppendLine(')')
$null = $sb.AppendLine('')
$null = $sb.AppendLine('# Link OpenMP (required for parallel processing)')
$null = $sb.AppendLine('find_package(OpenMP REQUIRED)')
$null = $sb.AppendLine('target_link_libraries(your_target PRIVATE OpenMP::OpenMP_CXX)')
$null = $sb.AppendLine('```')
$null = $sb.AppendLine("")
$null = $sb.AppendLine("## Dependencies")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("### Included Dependencies")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("The following dependencies are statically linked and their headers are included:")
$null = $sb.AppendLine("- SDL3 (library and headers)")
$null = $sb.AppendLine("- ImGui (library and headers)")
$null = $sb.AppendLine("- SDL3_image, SDL3_ttf, Assimp, Bullet Physics, etc. (statically linked)")
$null = $sb.AppendLine("- nlohmann/json (header-only)")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("You can directly use SDL3 and ImGui headers:")
$null = $sb.AppendLine('- `#include <SDL3/SDL.h>`')
$null = $sb.AppendLine('- `#include "imgui.h"` or `#include <imgui.h>`')
$null = $sb.AppendLine("")
$null = $sb.AppendLine("### Required External Dependencies")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("When using this prebuilt library, you still need the following dependencies:")
$null = $sb.AppendLine("- OpenGL 4.5+ driver")
$null = $sb.AppendLine("- C++20 compatible compiler")
$null = $sb.AppendLine("- OpenMP (for parallel processing)")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("### OpenMP Configuration")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("RenderEngine uses OpenMP for parallel processing. When using Method 1 (find_package), CMake will automatically handle OpenMP dependencies. For Method 2 (Direct Include), you need to link OpenMP manually:")
$null = $sb.AppendLine("")
$null = $sb.AppendLine('```cmake')
$null = $sb.AppendLine('# Find OpenMP')
$null = $sb.AppendLine('find_package(OpenMP REQUIRED)')
$null = $sb.AppendLine('')
$null = $sb.AppendLine('# Link OpenMP to your target')
$null = $sb.AppendLine('target_link_libraries(your_target PRIVATE OpenMP::OpenMP_CXX)')
$null = $sb.AppendLine('```')
$null = $sb.AppendLine("")
$null = $sb.AppendLine("Note: Third-party libraries (SDL3, Assimp, etc.) are statically linked into the RenderEngine library.")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("## License")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("Please refer to the LICENSE file of the original project.")

$ReadmePath = Join-Path $PackagePath "README.md"
# Write file using UTF-8 without BOM encoding
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($ReadmePath, $sb.ToString(), $utf8NoBom)
Write-Host "  Created README.md" -ForegroundColor Green

# 7. Create archive (optional)
Write-Host "`nPackaging complete!" -ForegroundColor Green
Write-Host "Prebuilt library location: $PackagePath" -ForegroundColor Cyan
Write-Host ""
Write-Host "To create a zip archive, run:" -ForegroundColor Yellow
Write-Host "  Compress-Archive -Path `"$PackagePath`" -DestinationPath `"$OutputDir\$PackageName.zip`"" -ForegroundColor Gray
