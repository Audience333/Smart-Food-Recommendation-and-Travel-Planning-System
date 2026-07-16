#!/bin/bash
# ============================================================================
# 菏泽美食旅游推荐系统 - C++ 数据处理管道 编译脚本 (Linux/Mac)
# ============================================================================
# 使用 g++ 编译 main.cpp 和所有模块文件为可执行文件 pipeline
# C++17 标准，-O2 优化，零外部库依赖（仅需 curl 命令行工具）
# ============================================================================

g++ -std=c++17 -O2 ../main.cpp pipeline_util.cpp cmd_expand.cpp cmd_fill.cpp cmd_photos.cpp cmd_roads.cpp cmd_json.cpp -o pipeline

if [ $? -eq 0 ]; then
    echo "Build successful: pipeline"
else
    echo "Build FAILED!"
    exit 1
fi
