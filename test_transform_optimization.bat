@echo off
echo ========================================
echo Transform渲染队列矩阵预计算优化测试
echo ========================================
echo.

REM 设置构建目录
set BUILD_DIR=build_transform_optimization
set BUILD_TYPE=Release

echo 清理旧构建...
if exist %BUILD_DIR% (
    rmdir /s /q %BUILD_DIR%
)

echo 创建构建目录...
mkdir %BUILD_DIR%
cd %BUILD_DIR%

echo.
echo 配置CMake (禁用优化)...
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DENABLE_TRANSFORM_QUEUE_PRECOMPUTE=OFF ^
    -DENABLE_OPENMP=ON ^
    ..

echo.
echo 编译 (禁用优化)...
cmake --build . --config %BUILD_TYPE% --target 47_transform_render_queue_optimization --parallel 8

echo.
echo 运行测试 (禁用优化)...
echo 禁用优化测试结果 > transform_optimization_disabled.txt
cd %BUILD_TYPE%
47_transform_render_queue_optimization.exe >> ..\transform_optimization_disabled.txt
cd ..

echo.
echo.
echo ========================================
echo 重新编译 (启用优化)...
echo ========================================

echo 清理旧构建...
rmdir /s /q %BUILD_DIR%
mkdir %BUILD_DIR%
cd %BUILD_DIR%

echo 配置CMake (启用优化)...
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DENABLE_TRANSFORM_QUEUE_PRECOMPUTE=ON ^
    -DENABLE_OPENMP=ON ^
    ..

echo.
echo 编译 (启用优化)...
cmake --build . --config %BUILD_TYPE% --target 47_transform_render_queue_optimization --parallel 8

echo.
echo 运行测试 (启用优化)...
echo 启用优化测试结果 > transform_optimization_enabled.txt
cd %BUILD_TYPE%
47_transform_render_queue_optimization.exe >> ..\transform_optimization_enabled.txt
cd ..

echo.
echo.
echo ========================================
echo 生成测试报告
echo ========================================
echo.
echo 禁用优化结果:
type transform_optimization_disabled.txt
echo.
echo 启用优化结果:
type transform_optimization_enabled.txt

echo.
echo.
echo 测试完成！请查看上述输出以比较优化效果。
echo 按任意键继续...
pause > nul

echo.
echo 清理构建目录...
cd ..
rmdir /s /q %BUILD_DIR%

echo.
echo Transform渲染队列矩阵预计算优化测试完成！
echo.
echo ========================================
echo 优化说明:
echo 1. 查看性能改进百分比
echo 2. 查看矩阵计算节省次数
echo 3. 查看内存开销情况
echo 4. 根据测试结果决定是否在生产环境启用优化
echo ========================================
echo.
pause