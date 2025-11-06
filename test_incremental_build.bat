@echo off
REM 测试增量编译脚本

echo ========================================
echo 测试增量编译
echo ========================================
echo.

REM 确保 build 目录存在
if not exist build (
    echo 错误：build 目录不存在！请先运行 build.bat 或 rebuild.bat
    pause
    exit /b 1
)

cd build

echo 第一次完整构建（这会编译所有文件）...
echo.
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo 构建失败！
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo 等待 3 秒...
timeout /t 3 /nobreak > nul
echo.

echo 第二次构建（应该很快完成，因为没有文件变化）...
echo 如果增量编译工作正常，应该看到类似 "0 个项目已更新" 的消息
echo.

REM 记录开始时间
set start_time=%time%

cmake --build . --config Release

REM 记录结束时间
set end_time=%time%

echo.
echo ========================================
echo 测试完成！
echo ========================================
echo.
echo 如果第二次构建几乎立即完成（少于5秒），
echo 那么增量编译工作正常。
echo.
echo 如果第二次构建花费了很长时间，
echo 请检查是否有文件被意外修改。
echo.

cd ..
pause

