@echo off
title Smart Food & Travel System

echo.
echo ============================================
echo   Smart Food Recommendation & Travel Planning
echo ============================================
echo.
echo   Starting server...
echo.

cd /d "%~dp0web"

python --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    start http://localhost:8080
    echo   Browser opened: http://localhost:8080
    echo   Press Ctrl+C to stop
    echo.
    python -m http.server 8080
) else (
    python3 --version >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        start http://localhost:8080
        python3 -m http.server 8080
    ) else (
        echo   [ERROR] Python 3 not found.
        echo   Download: https://www.python.org/downloads/
        pause
        exit /b 1
    )
)
