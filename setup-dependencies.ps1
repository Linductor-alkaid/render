# RenderEngine Dependency Setup Script
# This script automatically downloads and configures all required third-party libraries

param(
    [switch]$SkipEigen = $false,
    [switch]$SkipSDL = $false,
    [switch]$SkipSDLImage = $false,
    [switch]$SkipSDLTTF = $false,
    [switch]$SkipJSON = $false,
    [switch]$SkipAssimp = $false,
    [switch]$SkipMeshOptimizer = $false
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ThirdPartyDir = Join-Path $ScriptDir "third_party"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RenderEngine Dependency Setup Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check required tools
Write-Host "Checking required tools..." -ForegroundColor Yellow
$toolsOk = $true

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "  Error: Git not found, please install Git first" -ForegroundColor Red
    $toolsOk = $false
} else {
    Write-Host "  [OK] Git is installed" -ForegroundColor Green
}

if (-not $toolsOk) {
    Write-Host ""
    Write-Host "Please install missing tools before running this script" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Ensure third_party directory exists
if (-not (Test-Path $ThirdPartyDir)) {
    Write-Host "Creating third_party directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $ThirdPartyDir | Out-Null
}

Push-Location $ThirdPartyDir

# 1. Clone SDL3
if (-not $SkipSDL) {
    if (Test-Path "SDL") {
        Write-Host "SDL already exists, skipping clone" -ForegroundColor Green
    } else {
        Write-Host "Cloning SDL3..." -ForegroundColor Yellow
        git clone https://github.com/libsdl-org/SDL.git
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: SDL clone failed" -ForegroundColor Red
            Pop-Location
            exit 1
        }
    }
} else {
    Write-Host "Skipping SDL (using --SkipSDL)" -ForegroundColor Gray
}

# 2. Clone SDL_image
if (-not $SkipSDLImage) {
    if (Test-Path "SDL_image") {
        Write-Host "SDL_image already exists, skipping clone" -ForegroundColor Green
    } else {
        Write-Host "Cloning SDL_image..." -ForegroundColor Yellow
        git clone https://github.com/libsdl-org/SDL_image.git
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: SDL_image clone failed" -ForegroundColor Red
            Pop-Location
            exit 1
        }
    }
    
    # Run SDL_image's Get-GitModules.ps1
    $SDLImageExternalDir = Join-Path "SDL_image" "external"
    $GetGitModulesScript = Join-Path $SDLImageExternalDir "Get-GitModules.ps1"
    if (Test-Path $GetGitModulesScript) {
        Write-Host "Running SDL_image's Get-GitModules.ps1..." -ForegroundColor Yellow
        Push-Location $SDLImageExternalDir
        PowerShell -ExecutionPolicy Bypass -File ".\Get-GitModules.ps1"
        Pop-Location
    }
} else {
    Write-Host "Skipping SDL_image (using --SkipSDLImage)" -ForegroundColor Gray
}

# 3. Setup SDL3_ttf (if directory exists)
if (-not $SkipSDLTTF) {
    $SDLTTFDir = "SDL3_ttf-3.2.2"
    if (-not (Test-Path $SDLTTFDir)) {
        Write-Host "Warning: SDL3_ttf-3.2.2 directory does not exist, please manually download and extract to third_party directory" -ForegroundColor Yellow
    } else {
        Write-Host "SDL3_ttf-3.2.2 already exists" -ForegroundColor Green
        
        # Run SDL3_ttf's Get-GitModules.ps1
        $SDLTTFExternalDir = Join-Path $SDLTTFDir "external"
        $GetGitModulesScript = Join-Path $SDLTTFExternalDir "Get-GitModules.ps1"
        if (Test-Path $GetGitModulesScript) {
            Write-Host "Running SDL3_ttf's Get-GitModules.ps1..." -ForegroundColor Yellow
            Push-Location $SDLTTFExternalDir
            PowerShell -ExecutionPolicy Bypass -File ".\Get-GitModules.ps1"
            Pop-Location
        }
        
        # Copy required cmake files for SDL3_ttf
        Write-Host "Copying required cmake files for SDL3_ttf..." -ForegroundColor Yellow
        $SDLCMakeDir = Join-Path "SDL" "cmake"
        $SDLImageCMakeDir = Join-Path "SDL_image" "cmake"
        $SDLTTFCMakeDir = Join-Path $SDLTTFDir "cmake"
        
        if (-not (Test-Path $SDLCMakeDir)) {
            Write-Host "Error: SDL cmake directory does not exist" -ForegroundColor Red
            Pop-Location
            exit 1
        }
        
        if (-not (Test-Path $SDLTTFCMakeDir)) {
            Write-Host "Creating SDL3_ttf cmake directory..." -ForegroundColor Yellow
            New-Item -ItemType Directory -Path $SDLTTFCMakeDir | Out-Null
        }
        
        $CMakeFiles = @(
            @{Source = "GetGitRevisionDescription.cmake"; Dest = "GetGitRevisionDescription.cmake"},
            @{Source = "PkgConfigHelper.cmake"; Dest = "PkgConfigHelper.cmake"},
            @{Source = "sdlcpu.cmake"; Dest = "sdlcpu.cmake"},
            @{Source = "sdlplatform.cmake"; Dest = "sdlplatform.cmake"},
            @{Source = "sdlmanpages.cmake"; Dest = "sdlmanpages.cmake"}
        )
        
        foreach ($file in $CMakeFiles) {
            $sourcePath = Join-Path $SDLCMakeDir $file.Source
            $destPath = Join-Path $SDLTTFCMakeDir $file.Dest
            if (Test-Path $sourcePath) {
                Copy-Item -Path $sourcePath -Destination $destPath -Force
                Write-Host "  Copied: $($file.Dest)" -ForegroundColor Gray
            } else {
                Write-Host "  Warning: Source file does not exist: $sourcePath" -ForegroundColor Yellow
            }
        }
        
        # Copy PrivateSdlFunctions.cmake
        $privateFunctionsSource = Join-Path $SDLImageCMakeDir "PrivateSdlFunctions.cmake"
        $privateFunctionsDest = Join-Path $SDLTTFCMakeDir "PrivateSdlFunctions.cmake"
        if (Test-Path $privateFunctionsSource) {
            Copy-Item -Path $privateFunctionsSource -Destination $privateFunctionsDest -Force
            Write-Host "  Copied: PrivateSdlFunctions.cmake" -ForegroundColor Gray
        } else {
            Write-Host "  Warning: PrivateSdlFunctions.cmake does not exist" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "Skipping SDL3_ttf (using --SkipSDLTTF)" -ForegroundColor Gray
}

# 4. Clone nlohmann/json
if (-not $SkipJSON) {
    if (Test-Path "json") {
        Write-Host "nlohmann/json already exists, skipping clone" -ForegroundColor Green
    } else {
        Write-Host "Cloning nlohmann/json..." -ForegroundColor Yellow
        git clone https://github.com/nlohmann/json.git
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: nlohmann/json clone failed" -ForegroundColor Red
            Pop-Location
            exit 1
        }
    }
} else {
    Write-Host "Skipping nlohmann/json (using --SkipJSON)" -ForegroundColor Gray
}

# 5. Clone Assimp
if (-not $SkipAssimp) {
    if (Test-Path "assimp") {
        Write-Host "Assimp already exists, skipping clone" -ForegroundColor Green
    } else {
        Write-Host "Cloning Assimp..." -ForegroundColor Yellow
        git clone https://github.com/assimp/assimp.git
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: Assimp clone failed" -ForegroundColor Red
            Pop-Location
            exit 1
        }
    }
} else {
    Write-Host "Skipping Assimp (using --SkipAssimp)" -ForegroundColor Gray
}

# 6. Clone meshoptimizer
if (-not $SkipMeshOptimizer) {
    if (Test-Path "meshoptimizer") {
        Write-Host "meshoptimizer already exists, skipping clone" -ForegroundColor Green
    } else {
        Write-Host "Cloning meshoptimizer..." -ForegroundColor Yellow
        git clone https://github.com/zeux/meshoptimizer.git
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: meshoptimizer clone failed" -ForegroundColor Red
            Pop-Location
            exit 1
        }
    }
} else {
    Write-Host "Skipping meshoptimizer (using --SkipMeshOptimizer)" -ForegroundColor Gray
}

# 7. Download and extract Eigen3
if (-not $SkipEigen) {
    $EigenDir = "eigen-3.4.0"
    $EigenCoreFile = Join-Path $EigenDir "Eigen\Core"
    
    # Check if Eigen directory exists and if Core file exists (verify completeness)
    $needsDownload = $false
    if (Test-Path $EigenDir) {
        if (Test-Path $EigenCoreFile) {
            Write-Host "Eigen3 already exists and is complete, skipping download" -ForegroundColor Green
        } else {
            Write-Host "Eigen3 directory exists but Core file is missing, removing incomplete directory..." -ForegroundColor Yellow
            Remove-Item -Path $EigenDir -Recurse -Force
            $needsDownload = $true
        }
    } else {
        $needsDownload = $true
    }
    
    if ($needsDownload) {
        Write-Host "Downloading Eigen3..." -ForegroundColor Yellow
        $EigenZip = "eigen-3.4.0.zip"
        $EigenUrl = "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip"
        
        try {
            Invoke-WebRequest -Uri $EigenUrl -OutFile $EigenZip -UseBasicParsing
            Write-Host "Extracting Eigen3..." -ForegroundColor Yellow
            Expand-Archive -Path $EigenZip -DestinationPath "." -Force
            Remove-Item -Path $EigenZip -Force
            
            # Verify extraction by checking Core file
            if (Test-Path $EigenCoreFile) {
                Write-Host "Eigen3 download and extraction completed successfully" -ForegroundColor Green
            } else {
                Write-Host "Warning: Eigen3 extraction completed but Core file not found, extraction may be incomplete" -ForegroundColor Yellow
            }
        } catch {
            Write-Host "Error: Eigen3 download failed: $_" -ForegroundColor Red
            Write-Host "Please manually download: $EigenUrl" -ForegroundColor Yellow
            Pop-Location
            exit 1
        }
    }
} else {
    Write-Host "Skipping Eigen3 (using --SkipEigen)" -ForegroundColor Gray
}

Pop-Location

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Dependency setup completed!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Run cmake to configure the project" -ForegroundColor White
Write-Host "  2. Build the project" -ForegroundColor White
Write-Host ""
Write-Host "Example commands:" -ForegroundColor Yellow
Write-Host "  mkdir build; cd build" -ForegroundColor White
Write-Host "  cmake .." -ForegroundColor White
Write-Host "  cmake --build . --config Release" -ForegroundColor White
Write-Host ""
