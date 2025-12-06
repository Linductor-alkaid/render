# ============================================
# C++ Project Copyright Header Batch Addition Script
# ============================================

# Configuration Area - Please modify the following information
$COPYRIGHT_HOLDER = "Li Chaoyu" 
$PROJECT_NAME = "Render" 
$YEAR = (Get-Date).Year
$EMAIL = "2052046346@qq.com" 

# Project root directory (defaults to script directory)
$PROJECT_ROOT = $PSScriptRoot
if ([string]::IsNullOrEmpty($PROJECT_ROOT)) {
    $PROJECT_ROOT = Get-Location
}

# Excluded directories
$EXCLUDED_DIRS = @("third_party", "build", ".git", ".vs", "bin", "obj", "Debug", "Release")

# Copyright header template
$COPYRIGHT_HEADER = @"
/*
 * Copyright (c) $YEAR $COPYRIGHT_HOLDER
 * 
 * This file is part of $PROJECT_NAME.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: $EMAIL
 */

"@

# Function: Check if file already has copyright header
function Test-HasCopyright {
    param([string]$FilePath)
    
    $content = Get-Content $FilePath -Raw -ErrorAction SilentlyContinue
    if ($null -eq $content) { return $false }
    
    # Check if contains copyright keywords
    return ($content -match "Copyright.*$COPYRIGHT_HOLDER" -or 
            $content -match "GNU Affero General Public License")
}

# Function: Add copyright header
function Add-CopyrightHeader {
    param([string]$FilePath)
    
    try {
        # Read original content
        $originalContent = Get-Content $FilePath -Raw -Encoding UTF8
        
        # If file starts with BOM or special characters, preserve them
        $newContent = $COPYRIGHT_HEADER + $originalContent
        
        # Write back to file (using UTF8 without BOM)
        [System.IO.File]::WriteAllText($FilePath, $newContent, [System.Text.UTF8Encoding]($false))
        
        return $true
    }
    catch {
        Write-Warning "Failed to process file: $FilePath - $_"
        return $false
    }
}

# Function: Check if path should be excluded
function Test-ShouldExclude {
    param([string]$Path)
    
    foreach ($excluded in $EXCLUDED_DIRS) {
        if ($Path -like "*\$excluded\*" -or $Path -like "*/$excluded/*") {
            return $true
        }
    }
    return $false
}

# Main logic
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "C++ Project Copyright Header Batch Addition Tool" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Project root directory: $PROJECT_ROOT" -ForegroundColor Yellow
Write-Host "Copyright holder: $COPYRIGHT_HOLDER" -ForegroundColor Yellow
Write-Host "Project name: $PROJECT_NAME" -ForegroundColor Yellow
Write-Host "Excluded directories: $($EXCLUDED_DIRS -join ', ')" -ForegroundColor Yellow
Write-Host ""

# Confirm operation
$confirm = Read-Host "Continue? This will modify all .cpp and .h files (y/n)"
if ($confirm -ne 'y' -and $confirm -ne 'Y') {
    Write-Host "Operation cancelled" -ForegroundColor Red
    exit
}

Write-Host ""
Write-Host "Starting to process files..." -ForegroundColor Green
Write-Host ""

# Find all .cpp and .h files
$files = Get-ChildItem -Path $PROJECT_ROOT -Include *.cpp,*.h,*.hpp,*.cc,*.cxx -Recurse -File

$totalFiles = 0
$processedFiles = 0
$skippedFiles = 0
$errorFiles = 0

foreach ($file in $files) {
    $totalFiles++
    
    # Check if in excluded directory
    if (Test-ShouldExclude -Path $file.FullName) {
        Write-Host "[Skipped-Excluded] $($file.FullName)" -ForegroundColor DarkGray
        $skippedFiles++
        continue
    }
    
    # Check if already has copyright header
    if (Test-HasCopyright -FilePath $file.FullName) {
        Write-Host "[Skipped-Already exists] $($file.FullName)" -ForegroundColor Yellow
        $skippedFiles++
        continue
    }
    
    # Add copyright header
    if (Add-CopyrightHeader -FilePath $file.FullName) {
        Write-Host "[Added] $($file.FullName)" -ForegroundColor Green
        $processedFiles++
    }
    else {
        Write-Host "[Failed] $($file.FullName)" -ForegroundColor Red
        $errorFiles++
    }
}

# Output statistics
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Processing complete!" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Total files: $totalFiles" -ForegroundColor White
Write-Host "Files with headers added: $processedFiles" -ForegroundColor Green
Write-Host "Skipped files: $skippedFiles" -ForegroundColor Yellow
Write-Host "Failed files: $errorFiles" -ForegroundColor Red
Write-Host ""

if ($processedFiles -gt 0) {
    Write-Host "Suggestion: Please use Git to check if modifications are correct, then commit changes" -ForegroundColor Cyan
}

# Wait for user key press to exit
Write-Host ""
Write-Host "Press any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")