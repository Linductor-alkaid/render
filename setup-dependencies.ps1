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
    $SDLDir = "SDL"
    $needsClone = $false
    
    if (Test-Path $SDLDir) {
        # Check if directory is empty
        $sdlItems = Get-ChildItem -Path $SDLDir -ErrorAction SilentlyContinue
        if ($null -eq $sdlItems -or $sdlItems.Count -eq 0) {
            Write-Host "SDL directory exists but is empty, removing and re-cloning..." -ForegroundColor Yellow
            Remove-Item -Path $SDLDir -Recurse -Force
            $needsClone = $true
        } else {
            Write-Host "SDL already exists and is complete, skipping clone" -ForegroundColor Green
        }
    } else {
        $needsClone = $true
    }
    
    if ($needsClone) {
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
    $SDLImageDir = "SDL_image"
    $needsClone = $false
    
    if (Test-Path $SDLImageDir) {
        # Check if directory is empty
        $sdlImageItems = Get-ChildItem -Path $SDLImageDir -ErrorAction SilentlyContinue
        if ($null -eq $sdlImageItems -or $sdlImageItems.Count -eq 0) {
            Write-Host "SDL_image directory exists but is empty, removing and re-cloning..." -ForegroundColor Yellow
            Remove-Item -Path $SDLImageDir -Recurse -Force
            $needsClone = $true
        } else {
            Write-Host "SDL_image already exists, checking completeness..." -ForegroundColor Yellow
        }
    } else {
        $needsClone = $true
    }
    
    if ($needsClone) {
        Write-Host "Cloning SDL_image..." -ForegroundColor Yellow
        git clone https://github.com/libsdl-org/SDL_image.git
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: SDL_image clone failed" -ForegroundColor Red
            Pop-Location
            exit 1
        }
    }
    
    # Check and setup SDL_image external submodules
    $SDLImageExternalDir = Join-Path $SDLImageDir "external"
    $GetGitModulesScript = Join-Path $SDLImageExternalDir "Get-GitModules.ps1"
    $needsGitModules = $false
    
    if (Test-Path $SDLImageExternalDir) {
        # Check the required submodules in SDL_image external
        # Common submodules: jpeg, libpng, zlib are essential
        $requiredSubmodules = @("jpeg", "libpng", "zlib")
        $missingSubmodules = @()
        $emptySubmodules = @()
        
        foreach ($submodule in $requiredSubmodules) {
            $submoduleDir = Join-Path $SDLImageExternalDir $submodule
            if (-not (Test-Path $submoduleDir)) {
                $missingSubmodules += $submodule
            } else {
                # Check if submodule directory is empty
                $submoduleItems = Get-ChildItem -Path $submoduleDir -ErrorAction SilentlyContinue
                if ($null -eq $submoduleItems -or $submoduleItems.Count -eq 0) {
                    $emptySubmodules += $submodule
                }
            }
        }
        
        if ($missingSubmodules.Count -gt 0) {
            Write-Host "  Missing submodules: $($missingSubmodules -join ', '), need to run Get-GitModules.ps1" -ForegroundColor Yellow
            $needsGitModules = $true
        }
        
        if ($emptySubmodules.Count -gt 0) {
            Write-Host "  Empty submodules: $($emptySubmodules -join ', '), need to run Get-GitModules.ps1" -ForegroundColor Yellow
            $needsGitModules = $true
        }
        
        if (-not $needsGitModules) {
            Write-Host "  All external submodules are present and non-empty" -ForegroundColor Green
        }
    } else {
        Write-Host "  External directory does not exist, need to run Get-GitModules.ps1" -ForegroundColor Yellow
        $needsGitModules = $true
    }
    
    # Run SDL_image's Get-GitModules.ps1 if needed
    if ($needsGitModules) {
        if (Test-Path $GetGitModulesScript) {
            Write-Host "Running SDL_image's Get-GitModules.ps1..." -ForegroundColor Yellow
            Push-Location $SDLImageExternalDir
            PowerShell -ExecutionPolicy Bypass -File ".\Get-GitModules.ps1"
            Pop-Location
        } else {
            Write-Host "  Warning: Get-GitModules.ps1 script not found" -ForegroundColor Yellow
        }
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
        Write-Host "SDL3_ttf-3.2.2 directory exists, checking completeness..." -ForegroundColor Yellow
        
        $SDLTTFExternalDir = Join-Path $SDLTTFDir "external"
        $GetGitModulesScript = Join-Path $SDLTTFExternalDir "Get-GitModules.ps1"
        $SDLTTFCMakeDir = Join-Path $SDLTTFDir "cmake"
        
        # Check if external directory exists and has required submodules
        $needsGitModules = $false
        if (Test-Path $SDLTTFExternalDir) {
            # Check the four required submodules: freetype, harfbuzz, plutosvg, plutovg
            $requiredSubmodules = @("freetype", "harfbuzz", "plutosvg", "plutovg")
            $missingSubmodules = @()
            $emptySubmodules = @()
            
            foreach ($submodule in $requiredSubmodules) {
                $submoduleDir = Join-Path $SDLTTFExternalDir $submodule
                if (-not (Test-Path $submoduleDir)) {
                    $missingSubmodules += $submodule
                } else {
                    # Check if submodule directory is empty
                    $submoduleItems = Get-ChildItem -Path $submoduleDir -ErrorAction SilentlyContinue
                    if ($null -eq $submoduleItems -or $submoduleItems.Count -eq 0) {
                        $emptySubmodules += $submodule
                    }
                }
            }
            
            if ($missingSubmodules.Count -gt 0) {
                Write-Host "  Missing submodules: $($missingSubmodules -join ', '), need to run Get-GitModules.ps1" -ForegroundColor Yellow
                $needsGitModules = $true
            }
            
            if ($emptySubmodules.Count -gt 0) {
                Write-Host "  Empty submodules: $($emptySubmodules -join ', '), need to run Get-GitModules.ps1" -ForegroundColor Yellow
                $needsGitModules = $true
            }
            
            if (-not $needsGitModules) {
                Write-Host "  All external submodules are present and non-empty" -ForegroundColor Green
            }
        } else {
            Write-Host "  External directory does not exist, need to run Get-GitModules.ps1" -ForegroundColor Yellow
            $needsGitModules = $true
        }
        
        # Check if required cmake files exist
        $requiredCMakeFiles = @(
            "GetGitRevisionDescription.cmake",
            "PkgConfigHelper.cmake",
            "PrivateSdlFunctions.cmake",
            "sdlcpu.cmake",
            "sdlplatform.cmake",
            "sdlmanpages.cmake"
        )
        
        $needsCMakeFiles = $false
        if (-not (Test-Path $SDLTTFCMakeDir)) {
            Write-Host "  CMake directory does not exist, need to copy cmake files" -ForegroundColor Yellow
            $needsCMakeFiles = $true
        } else {
            $missingFiles = @()
            foreach ($cmakeFile in $requiredCMakeFiles) {
                $cmakeFilePath = Join-Path $SDLTTFCMakeDir $cmakeFile
                if (-not (Test-Path $cmakeFilePath)) {
                    $missingFiles += $cmakeFile
                }
            }
            if ($missingFiles.Count -gt 0) {
                Write-Host "  Missing cmake files: $($missingFiles -join ', '), need to copy" -ForegroundColor Yellow
                $needsCMakeFiles = $true
            } else {
                Write-Host "  All required cmake files are present" -ForegroundColor Green
            }
        }
        
        # Run Get-GitModules.ps1 if needed
        if ($needsGitModules) {
            if (Test-Path $GetGitModulesScript) {
                Write-Host "Running SDL3_ttf's Get-GitModules.ps1..." -ForegroundColor Yellow
                Push-Location $SDLTTFExternalDir
                PowerShell -ExecutionPolicy Bypass -File ".\Get-GitModules.ps1"
                Pop-Location
            } else {
                Write-Host "  Warning: Get-GitModules.ps1 script not found" -ForegroundColor Yellow
            }
        }
        
        # Copy required cmake files if needed
        if ($needsCMakeFiles) {
            Write-Host "Copying required cmake files for SDL3_ttf..." -ForegroundColor Yellow
            $SDLCMakeDir = Join-Path "SDL" "cmake"
            $SDLImageCMakeDir = Join-Path "SDL_image" "cmake"
            
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
        
        if (-not $needsGitModules -and -not $needsCMakeFiles) {
            Write-Host "SDL3_ttf-3.2.2 is complete and ready" -ForegroundColor Green
        }
    }
} else {
    Write-Host "Skipping SDL3_ttf (using --SkipSDLTTF)" -ForegroundColor Gray
}

# 4. Clone nlohmann/json
if (-not $SkipJSON) {
    $JSONDir = "json"
    $needsClone = $false
    
    if (Test-Path $JSONDir) {
        # Check if directory is empty
        $jsonItems = Get-ChildItem -Path $JSONDir -ErrorAction SilentlyContinue
        if ($null -eq $jsonItems -or $jsonItems.Count -eq 0) {
            Write-Host "nlohmann/json directory exists but is empty, removing and re-cloning..." -ForegroundColor Yellow
            Remove-Item -Path $JSONDir -Recurse -Force
            $needsClone = $true
        } else {
            Write-Host "nlohmann/json already exists and is complete, skipping clone" -ForegroundColor Green
        }
    } else {
        $needsClone = $true
    }
    
    if ($needsClone) {
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
    $AssimpDir = "assimp"
    $needsClone = $false
    
    if (Test-Path $AssimpDir) {
        # Check if directory is empty
        $assimpItems = Get-ChildItem -Path $AssimpDir -ErrorAction SilentlyContinue
        if ($null -eq $assimpItems -or $assimpItems.Count -eq 0) {
            Write-Host "Assimp directory exists but is empty, removing and re-cloning..." -ForegroundColor Yellow
            Remove-Item -Path $AssimpDir -Recurse -Force
            $needsClone = $true
        } else {
            Write-Host "Assimp already exists and is complete, skipping clone" -ForegroundColor Green
        }
    } else {
        $needsClone = $true
    }
    
    if ($needsClone) {
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
    $MeshOptimizerDir = "meshoptimizer"
    $needsClone = $false
    
    if (Test-Path $MeshOptimizerDir) {
        # Check if directory is empty
        $meshOptimizerItems = Get-ChildItem -Path $MeshOptimizerDir -ErrorAction SilentlyContinue
        if ($null -eq $meshOptimizerItems -or $meshOptimizerItems.Count -eq 0) {
            Write-Host "meshoptimizer directory exists but is empty, removing and re-cloning..." -ForegroundColor Yellow
            Remove-Item -Path $MeshOptimizerDir -Recurse -Force
            $needsClone = $true
        } else {
            Write-Host "meshoptimizer already exists and is complete, skipping clone" -ForegroundColor Green
        }
    } else {
        $needsClone = $true
    }
    
    if ($needsClone) {
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

# 7. Clone bullet3
if (-not $SkipBullet3) {
    $Bullet3Dir = "bullet3"
    $needsClone = $false
    
    if (Test-Path $Bullet3Dir) {
        # Check if directory is empty
        $bullet3Items = Get-ChildItem -Path $Bullet3Dir -ErrorAction SilentlyContinue
        if ($null -eq $bullet3Items -or $bullet3Items.Count -eq 0) {
            Write-Host "bullet3 directory exists but is empty, removing and re-cloning..." -ForegroundColor Yellow
            Remove-Item -Path $Bullet3Dir -Recurse -Force
            $needsClone = $true
        } else {
            Write-Host "bullet3 already exists and is complete, skipping clone" -ForegroundColor Green
        }
    } else {
        $needsClone = $true
    }
    
    if ($needsClone) {
        Write-Host "Cloning bullet3..." -ForegroundColor Yellow
        git clone https://github.com/bulletphysics/bullet3.git

        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: bullet3 clone failed" -ForegroundColor Red
            Pop-Location
            exit 1
        }
    }
} else {
    Write-Host "Skipping bullet3 (using --SkipBullet3)" -ForegroundColor Gray
}


# 8. Download and extract Eigen3
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
