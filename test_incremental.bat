@echo off
REM 快速测试增量编译

echo ========================================
echo 测试增量编译
echo ========================================
echo.

cd build

echo [1] 首次构建 RenderEngine 库...
cmake --build . --config Release --target RenderEngine

if %ERRORLEVEL% NEQ 0 (
    echo 构建失败！
    cd ..
    pause
    exit /b 1
)

echo.
echo [2] 等待 2 秒后再次构建...
timeout /t 2 /nobreak > nul
echo.

echo [3] 第二次构建（应该很快，几乎不编译任何东西）...
echo.

cmake --build . --config Release --target RenderEngine

echo.
echo ========================================
echo 测试完成！
echo ========================================
echo.
echo 如果第二次构建显示 "0 failed" 且几乎立即完成，
echo 说明增量编译工作正常！
echo.

cd ..
pause

