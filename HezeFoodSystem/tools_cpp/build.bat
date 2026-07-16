@echo off
REM ============================================================================
REM 菏泽美食旅游推荐系统 - C++ 数据处理管道 编译脚本 (Windows)
REM ============================================================================
REM 使用 g++ (MinGW/Cygwin) 编译 main.cpp 和 pipeline.cpp 为可执行文件 pipeline.exe
REM C++17 标准，-O2 优化，零外部库依赖（仅需 curl 命令行工具）
REM ============================================================================

g++ -std=c++17 -O2 ../main.cpp pipeline.cpp -o pipeline.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful: pipeline.exe
) else (
    echo Build FAILED!
    exit /b 1
)
