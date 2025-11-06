@echo off
REM 完全重新构建脚本 - 清理并重新配置CMake

echo ========================================
echo 清理并重新配置 RenderEngine...
echo ========================================

REM 删除旧的 build 目录
if exist build (
    echo 删除旧的 build 目录...
    rmdir /s /q build
)

REM 创建新的 build 目录
echo 创建新的 build 目录...
mkdir build
cd build

REM 运行 CMake 配置
echo 运行 CMake 配置...
cmake .. -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo CMake 配置失败！
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo 配置成功！
echo ========================================
echo.
echo 要构建项目，运行：
echo   cmake --build build --config Release
echo.
echo 或在 Visual Studio 中打开 build/RenderEngine.sln
echo.
echo 提示：现在项目应该支持增量编译了！
echo       只有修改的文件会被重新编译。
echo.

cd ..
pause

