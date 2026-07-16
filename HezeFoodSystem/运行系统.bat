@echo off
chcp 65001 >nul
title 智慧美食推荐与漫游规划系统

echo.
echo  ============================================
echo   智慧美食推荐与漫游规划系统
echo  ============================================
echo.
echo  启动中...
echo.

cd /d "%~dp0web"

:: 检查 Python 是否可用
python --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    start http://localhost:8080
    echo  浏览器已打开，如未自动打开请访问 http://localhost:8080
    echo  按 Ctrl+C 停止服务器
    echo.
    python -m http.server 8080
) else (
    python3 --version >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        start http://localhost:8080
        python3 -m http.server 8080
    ) else (
        echo  [错误] 未找到 Python，请安装 Python 3 后重试
        echo  下载地址: https://www.python.org/downloads/
        pause
        exit /b 1
    )
)
