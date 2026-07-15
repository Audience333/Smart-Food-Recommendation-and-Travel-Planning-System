@echo off
g++ -std=c++17 -O2 pipeline.cpp -o pipeline.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful: pipeline.exe
) else (
    echo Build FAILED!
    exit /b 1
)
