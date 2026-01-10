# Scripts Usage Guide

## Scripts Overview

All scripts are written in English to avoid encoding issues.

- `package_prebuilt.ps1` - Package prebuilt library
- `build_and_package.ps1` - One-click build and package script

## Usage

### Build and Package (Recommended)

```powershell
PowerShell -ExecutionPolicy Bypass -File ".\scripts\build_and_package.ps1"
```

### Package Only

If you already have a built project:

```powershell
PowerShell -ExecutionPolicy Bypass -File ".\scripts\package_prebuilt.ps1"
```

### Custom Options

```powershell
# Build Debug version
PowerShell -ExecutionPolicy Bypass -File ".\scripts\build_and_package.ps1" -BuildType Debug

# Skip build, only package
PowerShell -ExecutionPolicy Bypass -File ".\scripts\build_and_package.ps1" -SkipBuild

# Skip package, only build and install
PowerShell -ExecutionPolicy Bypass -File ".\scripts\build_and_package.ps1" -SkipPackage
```

## Output

After running the scripts, you will get:

- **Install directory**: `build/install/` - Contains installed library files
- **Package directory**: `prebuilt/RenderEngine-prebuilt-{Config}-{Arch}/` - Contains packaged prebuilt library

The package includes:
- Library files (`lib/`)
- Header files (`include/`)
- CMake configuration files (`lib/cmake/RenderEngine/`)
- Shader files (`share/RenderEngine/shaders/`)
- README.md with usage instructions

## Notes

- All scripts use UTF-8 encoding for file output
- Generated README.md files are in English
- Scripts are compatible with both Windows PowerShell and PowerShell Core
