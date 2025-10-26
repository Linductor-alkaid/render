@echo off
REM 构建脚本 - Windows (Visual Studio)

echo ========================================
echo Building RenderEngine...
echo ========================================

REM 创建 build 目录
if not exist build mkdir build
cd build

REM 运行 CMake 配置
cmake .. -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Configuration successful!
echo ========================================
echo.
echo To build the project, run:
echo   cmake --build build --config Release
echo.
echo Or open build/RenderEngine.sln in Visual Studio
echo.

cd ..
pause

