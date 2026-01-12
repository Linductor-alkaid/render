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

# 检测是否构建为动态库
$IsShared = $false
if (Test-Path (Join-Path $BuildDirAbs "CMakeCache.txt")) {
    $CacheContent = Get-Content (Join-Path $BuildDirAbs "CMakeCache.txt") -Raw
    if ($CacheContent -match "BUILD_SHARED_LIBS:BOOL=(ON|TRUE)") {
        $IsShared = $true
    }
}

$LibraryType = if ($IsShared) { "Shared" } else { "Static" }
$PackageName = "RenderEngine-prebuilt-$Config-$Arch-$LibraryType"
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
$BinSource = Join-Path $ActualInstallDir "bin"
$LibDest = Join-Path $PackagePath "lib"
$BinDest = Join-Path $PackagePath "bin"

# Copy library files
if (Test-Path $LibSource) {
    Copy-Item -Recurse $LibSource $LibDest -Force
    Write-Host "  Copied library files to: $LibDest" -ForegroundColor Green
} else {
    Write-Host "  Warning: Library directory not found at: $LibSource" -ForegroundColor Yellow
    Write-Host "  This may indicate that installation did not complete successfully." -ForegroundColor Yellow
}

# Copy third-party static libraries (for static library builds only)
# These libraries should be installed by CMake, so we copy them from the install directory
# This ensures all dependencies are available when linking against the prebuilt library
if (-not $IsShared) {
    Write-Host "  Copying third-party static libraries from install directory..." -ForegroundColor Cyan
    
    # List of third-party library files that should be in the install lib directory
    # These are installed by CMakeLists.txt install rules
    # Note: Assimp library filename may have MSVC version suffix (e.g., assimp-vc142-mt.lib)
    $ThirdPartyLibPatterns = @(
        @{ Pattern = "SDL3-static.lib"; Required = $true }
        @{ Pattern = "SDL3_image-static.lib"; Required = $true }
        @{ Pattern = "SDL3_ttf-static.lib"; Required = $true }
        @{ Pattern = "assimp*.lib"; Required = $true }  # Assimp may have version suffix
        @{ Pattern = "meshoptimizer.lib"; Required = $true }
        @{ Pattern = "BulletDynamics.lib"; Required = $true }
        @{ Pattern = "BulletCollision.lib"; Required = $true }
        @{ Pattern = "LinearMath.lib"; Required = $true }
    )
    
    $CopiedCount = 0
    $NotFoundLibs = @()
    $CopiedLibs = @()
    
    if (Test-Path $LibSource) {
        foreach ($libInfo in $ThirdPartyLibPatterns) {
            $pattern = $libInfo.Pattern
            $required = $libInfo.Required
            
            # Use wildcard search for patterns (like assimp*.lib)
            $matchingFiles = Get-ChildItem -Path $LibSource -Filter $pattern -ErrorAction SilentlyContinue
            
            if ($matchingFiles.Count -gt 0) {
                foreach ($file in $matchingFiles) {
                    $libName = $file.Name
                    $srcPath = $file.FullName
                    $destPath = Join-Path $LibDest $libName
                    
                    # For assimp, rename to assimp.lib if it has a suffix
                    if ($pattern -eq "assimp*.lib" -and $libName -ne "assimp.lib" -and $libName -ne "assimpd.lib") {
                        $destPath = Join-Path $LibDest "assimp.lib"
                    }
                    
                    Copy-Item $srcPath $destPath -Force
                    Write-Host "    Copied: $libName -> $([System.IO.Path]::GetFileName($destPath))" -ForegroundColor Green
                    $CopiedCount++
                    $CopiedLibs += [System.IO.Path]::GetFileName($destPath)
                }
            } else {
                if ($required) {
                    $NotFoundLibs += $pattern
                }
            }
        }
    }
    
    if ($CopiedCount -gt 0) {
        Write-Host "  Copied $CopiedCount third-party library file(s)" -ForegroundColor Green
        Write-Host "  Libraries copied: $($CopiedLibs -join ', ')" -ForegroundColor Gray
    }
    
    if ($NotFoundLibs.Count -gt 0) {
        Write-Host "  Warning: The following library patterns were not found in install directory:" -ForegroundColor Yellow
        foreach ($lib in $NotFoundLibs) {
            Write-Host "    - $lib" -ForegroundColor Yellow
        }
        Write-Host "  This may indicate that CMake install rules need to be updated." -ForegroundColor Yellow
    }
}

# Copy runtime files (DLL for Windows shared library)
if ($IsShared -and (Test-Path $BinSource)) {
    New-Item -ItemType Directory -Path $BinDest -Force | Out-Null
    Copy-Item -Recurse $BinSource\* $BinDest -Force
    Write-Host "  Copied runtime files (DLL) to: $BinDest" -ForegroundColor Green
} elseif ($IsShared) {
    Write-Host "  Warning: Bin directory not found for shared library: $BinSource" -ForegroundColor Yellow
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
$null = $sb.AppendLine("- Library Type: $LibraryType")
$null = $sb.AppendLine("- Build Date: $BuildDate")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("## Directory Structure")
$null = $sb.AppendLine('```')
$null = $sb.AppendLine("$PackageName/")
if ($IsShared) {
    $null = $sb.AppendLine("  bin/                    # Runtime files (DLL)")
    $null = $sb.AppendLine("    RenderEngine.dll      # Dynamic library")
}
$null = $sb.AppendLine("  lib/                    # Library files")
if ($IsShared) {
    $null = $sb.AppendLine("    RenderEngine.lib      # Import library (Windows)")
} else {
    $null = $sb.AppendLine("    RenderEngine.lib      # Static library (Windows)")
}
$null = $sb.AppendLine("    cmake/")
$null = $sb.AppendLine("      RenderEngine/   # CMake configuration files")
$null = $sb.AppendLine("  include/                # Header files")
$null = $sb.AppendLine("    render/           # RenderEngine headers")
$null = $sb.AppendLine("    SDL3/             # SDL3 headers")
$null = $sb.AppendLine("    glad/             # GLAD headers")
$null = $sb.AppendLine("    KHR/              # KHR platform headers")
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
if ($IsShared) {
    $null = $sb.AppendLine('target_link_libraries(your_target PRIVATE ')
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/RenderEngine.lib`"")
    $null = $sb.AppendLine(')')
    $null = $sb.AppendLine('')
    $null = $sb.AppendLine('# Copy DLL to output directory')
    $null = $sb.AppendLine('add_custom_command(TARGET your_target POST_BUILD')
    $null = $sb.AppendLine("    COMMAND `${CMAKE_COMMAND} -E copy_if_different")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/bin/RenderEngine.dll`"")
    $null = $sb.AppendLine('    $<TARGET_FILE_DIR:your_target>')
    $null = $sb.AppendLine(')')
} else {
    $null = $sb.AppendLine('# Static library: link all dependencies')
    $null = $sb.AppendLine('target_link_libraries(your_target PRIVATE ')
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/RenderEngine.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/SDL3-static.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/SDL3_image-static.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/SDL3_ttf-static.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/assimp.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/meshoptimizer.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/BulletDynamics.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/BulletCollision.lib`"")
    $null = $sb.AppendLine("    `"`${CMAKE_CURRENT_SOURCE_DIR}/path/to/$PackageName/lib/LinearMath.lib`"")
    $null = $sb.AppendLine(')')
}
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
$null = $sb.AppendLine("- GLAD (library and headers)")
$null = $sb.AppendLine("- ImGui (library and headers)")
$null = $sb.AppendLine("- SDL3_image, SDL3_ttf, Assimp, Bullet Physics, etc. (statically linked)")
$null = $sb.AppendLine("- nlohmann/json (header-only)")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("You can directly use SDL3, GLAD and ImGui headers:")
$null = $sb.AppendLine('- `#include <SDL3/SDL.h>`')
$null = $sb.AppendLine('- `#include <glad/glad.h>`')
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
if ($IsShared) {
    $null = $sb.AppendLine("### Runtime Requirements (Shared Library)")
    $null = $sb.AppendLine("")
    $null = $sb.AppendLine("This is a shared library (DLL) version. You need to ensure `RenderEngine.dll` is available at runtime.")
    $null = $sb.AppendLine("")
    $null = $sb.AppendLine("**Option 1**: Copy the DLL to your executable directory")
    $null = $sb.AppendLine("")
    $null = $sb.AppendLine("**Option 2**: Add the DLL directory to your PATH environment variable")
    $null = $sb.AppendLine("")
    $null = $sb.AppendLine("Third-party libraries (SDL3, Assimp, etc.) are statically linked into the RenderEngine DLL.")
} else {
    $null = $sb.AppendLine("### Static Library Dependencies")
    $null = $sb.AppendLine("")
    $null = $sb.AppendLine("This is a static library version. All required third-party static libraries are included in the `lib` directory:")
    $null = $sb.AppendLine("")
    $null = $sb.AppendLine("- `RenderEngine.lib` - Main RenderEngine library")
    $null = $sb.AppendLine("- `SDL3-static.lib` - SDL3 static library")
    $null = $sb.AppendLine("- `SDL3_image-static.lib` - SDL3_image static library")
    $null = $sb.AppendLine("- `SDL3_ttf-static.lib` - SDL3_ttf static library")
    $null = $sb.AppendLine("- `assimp.lib` - Assimp model loader library")
    $null = $sb.AppendLine("- `meshoptimizer.lib` - Mesh optimization library")
    $null = $sb.AppendLine("- `BulletDynamics.lib`, `BulletCollision.lib`, `LinearMath.lib` - Bullet Physics libraries")
    $null = $sb.AppendLine("")
    $null = $sb.AppendLine("**Important**: When linking against this static library, you need to link all these libraries together. See the usage documentation for examples.")
}
$null = $sb.AppendLine("")
$null = $sb.AppendLine("## Important Notes")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("### Using ImGui Backends")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("The ImGui backend files (`imgui_impl_sdl3.cpp` and `imgui_impl_opengl3.cpp`) are already compiled and statically linked into the RenderEngine library.")
$null = $sb.AppendLine("")
$null = $sb.AppendLine("**Do NOT** compile these files in your own project. Only include the header files:")
$null = $sb.AppendLine("")
$null = $sb.AppendLine('```cpp')
$null = $sb.AppendLine('#include "backends/imgui_impl_sdl3.h"')
$null = $sb.AppendLine('#include "backends/imgui_impl_opengl3.h"')
$null = $sb.AppendLine('```')
$null = $sb.AppendLine("")
$null = $sb.AppendLine("If you compile these files yourself, you will encounter SDL3 linking errors because your project will need to link SDL3 separately.")
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
