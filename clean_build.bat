@echo off
REM 清理构建缓存并重新配置
REM 这会删除所有 CMake 缓存，确保完全重新配置

echo ========================================
echo 清理 CMake 构建缓存...
echo ========================================

REM 删除整个 build 目录
if exist build (
    echo 删除 build 目录...
    rmdir /s /q build
)

REM 重新创建 build 目录
echo 创建新的 build 目录...
mkdir build
cd build

echo.
echo ========================================
echo 运行 CMake 配置...
echo ========================================

REM 运行 CMake 配置
cmake .. -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo CMake 配置失败！
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo 配置成功完成！
echo ========================================
echo.
echo 现在可以构建项目了：
echo   cmake --build . --config Release
echo.
echo 或者打开 Visual Studio 解决方案：
echo   build\RenderEngine.sln
echo.

cd ..
pause

