#!/bin/bash
# ============================================
#   智慧美食推荐与漫游规划系统 — 一键启动脚本
# ============================================

echo ""
echo "  ============================================"
echo "   智慧美食推荐与漫游规划系统"
echo "  ============================================"
echo ""

cd "$(dirname "$0")/web"

# 自动打开浏览器
if command -v xdg-open &> /dev/null; then
    xdg-open http://localhost:8080 &
elif command -v open &> /dev/null; then
    open http://localhost:8080 &
fi

echo "  浏览器已打开，如未自动打开请访问 http://localhost:8080"
echo "  按 Ctrl+C 停止服务器"
echo ""

# 启动 HTTP 服务器
if command -v python3 &> /dev/null; then
    python3 -m http.server 8080
elif command -v python &> /dev/null; then
    python -m http.server 8080
else
    echo "  [错误] 未找到 Python，请安装 Python 3 后重试"
    exit 1
fi
