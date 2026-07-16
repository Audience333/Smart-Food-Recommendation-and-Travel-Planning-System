/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 - C++ 数据处理管道 (cmd_fill)
 * ============================================================================
 *
 * 【命令功能】
 *   pipeline fill — 填充缺失地址命令（替代 fill_addresses.py 和 fetch_amap_data.py）
 *
 * 【输入文件】
 *   data/food.txt    - 美食数据（含经纬度）
 *   data/spot.txt    - 景点数据（含经纬度）
 *   config/amap_config.txt - API Key
 *
 * 【输出文件】
 *   data/food.txt    - 覆盖更新（地址字段被填充）
 *   data/spot.txt    - 覆盖更新（地址字段被填充）
 *
 * 【算法流程】
 *   1) 读取全部数据行（保留注释和空行）
 *   2) 对每条数据检查地址字段是否有有效值
 *   3) 若地址缺失，调用逆地理编码获取地址
 *   4) 使用坐标缓存（5位小数精度）避免重复查询同一坐标
 *   5) 每次API调用后延迟100ms（遵守限流）
 *   6) 全部更新后覆盖写回原文件
 *
 * 【本文件依赖 pipeline_util.cpp】
 *   所有工具函数均在其中定义，本文件仅通过 extern 声明引用。
 * ============================================================================
 */

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace std;

// ---- 外部工具函数声明 (定义在 pipeline_util.cpp) ----
extern bool validate_workdir();
extern map<string, string> load_config();
extern vector<string> split(const string& s, char delim);
extern string reverse_geocode(const string& key, double lng, double lat);

// ---- 全局常量（定义在 pipeline_util.cpp）----
extern const string FOOD_TXT;
extern const string SPOT_TXT;

// ====================================================================
// cmd_fill - 填充缺失地址命令
// ====================================================================
int cmd_fill() {
    if (!validate_workdir()) return 1;
    printf("============================================================\n");
    printf("  Fill Addresses via Reverse Geocoding\n");
    printf("============================================================\n");

    auto config = load_config();
    string key = config.count("AMAP_KEY") ? config["AMAP_KEY"] : config["API_KEY"];
    if (key.empty()) {
        printf("ERROR: API Key not found\n");
        return 1;
    }
    printf("API Key: %.8s...\n", key.c_str());

    auto process_file = [&](const string& path, int lng_idx, int lat_idx, int addr_idx, const string& label) {
        printf("\n[%s] Processing %s...\n", label.c_str(), path.c_str());
        vector<string> lines;
        ifstream fin(path);
        string line;
        while (getline(fin, line)) lines.push_back(line);
        fin.close();

        map<string, string> addr_cache;
        int updated = 0;
        vector<string> new_lines;

        for (auto& orig_line : lines) {
            string stripped = orig_line;
            stripped.erase(0, stripped.find_first_not_of(" \t\r\n"));
            stripped.erase(stripped.find_last_not_of(" \t\r\n") + 1);

            if (stripped.empty() || stripped[0] == '#') {
                new_lines.push_back(orig_line);
                continue;
            }

            auto parts = split(stripped, '|');
            if ((int)parts.size() <= max({lng_idx, lat_idx, addr_idx})) {
                new_lines.push_back(orig_line);
                continue;
            }

            string cur_addr = parts[addr_idx];
            if (cur_addr != "-" && cur_addr != "" && cur_addr != "无法获取") {
                new_lines.push_back(orig_line);
                continue;
            }

            try {
                double lng = stod(parts[lng_idx]);
                double lat = stod(parts[lat_idx]);

                char cache_key[64];
                snprintf(cache_key, sizeof(cache_key), "%.5f,%.5f", lng, lat);
                string ck(cache_key);

                string addr;
                if (addr_cache.count(ck)) {
                    addr = addr_cache[ck];
                } else {
                    printf("  Geocoding %s (id=%s)...\n", parts[1].c_str(), parts[0].c_str());
                    addr = reverse_geocode(key, lng, lat);
                    if (addr.empty()) addr = "无法获取";
                    addr_cache[ck] = addr;
                    this_thread::sleep_for(chrono::milliseconds(100));
                }

                parts[addr_idx] = addr;
                string new_line;
                for (size_t i = 0; i < parts.size(); i++) {
                    if (i > 0) new_line += "|";
                    new_line += parts[i];
                }
                new_lines.push_back(new_line);
                updated++;
                if (updated % 10 == 0) printf("  Updated %d %s entries...\n", updated, label.c_str());
            } catch (...) {
                new_lines.push_back(orig_line);
            }
        }

        if (updated > 0) {
            ofstream fout(path);
            for (auto& l : new_lines) fout << l << "\n";
            printf("  Done: updated %d entries\n", updated);
        }
        return updated;
    };

    int food_updates = process_file(FOOD_TXT, 2, 3, 7, "foods");
    int spot_updates = process_file(SPOT_TXT, 4, 5, 3, "spots");

    printf("\nTotal updated: foods=%d, spots=%d\n", food_updates, spot_updates);
    printf("Done! Next: pipeline json\n");
    return 0;
}
