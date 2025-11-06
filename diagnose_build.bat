@echo off
REM 诊断增量编译问题

echo ========================================
echo 诊断增量编译问题
echo ========================================
echo.

cd build

echo [步骤1] 检查assimp生成的头文件时间戳...
echo.
if exist "assimp\include\assimp\config.h" (
    dir "assimp\include\assimp\config.h" | find "config.h"
) else (
    echo config.h 不存在
)

echo.
echo [步骤2] 检查assimp库文件时间戳...
echo.
if exist "lib\Release\assimp-vc143-mt.lib" (
    dir "lib\Release\assimp-vc143-mt.lib" | find "assimp"
) else (
    echo assimp库不存在，需要先构建
)

echo.
echo [步骤3] 检查CMakeCache.txt...
echo.
findstr /C:"ASSIMP_BUILD" CMakeCache.txt | findstr /C:"ON" | find /C "ON"

echo.
echo [步骤4] 等待5秒后再次构建，观察哪些文件被重新编译...
echo.
timeout /t 5 /nobreak > nul

echo 开始增量编译测试...
echo.
cmake --build . --config Release --target assimp 2>&1 | find /I "Building\|Linking\|cpp\|已"

echo.
echo ========================================
echo 诊断完成！
echo ========================================
echo.
echo 如果看到很多.cpp文件被编译，说明不是增量编译。
echo 如果只看到"已更新"或者"up-to-date"，说明增量编译正常。
echo.

cd ..
pause

