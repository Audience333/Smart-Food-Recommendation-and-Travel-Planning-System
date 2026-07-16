/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 — 程序入口 (main.cpp)
 * ============================================================================
 *
 * 命令行参数解析和子命令分发。
 * 接收一个子命令名称作为参数，调用 pipeline.cpp 中对应的 cmd_* 函数。
 *
 * 使用方法
 *   pipeline expand   搜索并扩充POI数据
 *   pipeline fill     补全缺失地址
 *   pipeline photos   获取门店照片URL
 *   pipeline roads    重新计算道路连接
 *   pipeline json     生成前端JSON文件
 *   pipeline all      按顺序执行全部步骤
 *
 * 编译
 *   g++ -std=c++17 -O2 main.cpp tools_cpp/pipeline.cpp -o tools_cpp/pipeline.exe
 *   依赖 curl 命令行工具在系统PATH中可用
 * ============================================================================
 */

#include <cstdio>
#include <string>
#include <cstdlib>

using namespace std;

int cmd_expand();
int cmd_fill();
int cmd_photos();
int cmd_roads();
int cmd_json();

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: pipeline <command>\n");
        printf("Commands: expand, fill, photos, roads, json, all\n");
        return 1;
    }

    string cmd = argv[1];

    if (cmd == "expand") { return cmd_expand(); }
    else if (cmd == "fill") { return cmd_fill(); }
    else if (cmd == "photos") { return cmd_photos(); }
    else if (cmd == "roads") { return cmd_roads(); }
    else if (cmd == "json") { return cmd_json(); }
    else if (cmd == "all") {
        printf("Running full pipeline...\n");
        if (cmd_expand() != 0) return 1;
        if (cmd_fill() != 0) return 1;
        if (cmd_photos() != 0) return 1;
        if (cmd_roads() != 0) return 1;
        if (cmd_json() != 0) return 1;
        printf("\nAll steps completed!\n");
        return 0;
    }
    else {
        printf("Unknown command: %s\n", cmd.c_str());
        return 1;
    }
}
