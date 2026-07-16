/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 - C++ 数据处理管道 (cmd_photos)
 * ============================================================================
 *
 * 【命令功能】
 *   pipeline photos — 获取POI照片命令（替代 fetch_photos.py）
 *
 * 【输入文件】
 *   data/food.txt, data/spot.txt - 含POI名称和坐标的数据文件
 *   config/amap_config.txt       - 高德API Key
 *
 * 【输出文件】
 *   data/food.txt, data/spot.txt - 覆盖更新（在标签字段前插入照片URL字段）
 *
 * 【API调用】
 *   第一步：高德 /v3/place/text 文本搜索 → 找到匹配POI的ID
 *   第二步：高德 /v3/place/detail 详情查询 → 获取照片URL
 *
 * 【算法流程】
 *   1) 逐行读取数据文件
 *   2) 对每条数据调用 find_amap_id() 获取POI ID
 *   3) 用ID调用 fetch_photos_by_id() 获取照片URL
 *   4) 将照片URL插入到倒数第二个字段位置
 *   5) 逐条处理后覆盖写回
 *
 * 【速率限制】
 *   每条POI处理后有150ms延迟（含两次API调用）
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
#include <chrono>
#include <thread>

using namespace std;

// ---- 外部工具函数声明 (定义在 pipeline_util.cpp) ----
extern bool validate_workdir();
extern map<string, string> load_config();
extern vector<string> split(const string& s, char delim);
extern string find_amap_id(const string& key, const string& name, double lng, double lat);
extern string fetch_photos_by_id(const string& key, const string& amap_id);

// ---- 全局常量（定义在 pipeline_util.cpp）----
extern const string FOOD_TXT;
extern const string SPOT_TXT;

// ====================================================================
// cmd_photos - 获取POI照片命令
// ====================================================================
int cmd_photos() {
    if (!validate_workdir()) return 1;
    printf("============================================================\n");
    printf("  Fetch POI Photos\n");
    printf("============================================================\n");

    auto config = load_config();
    string key = config.count("AMAP_KEY") ? config["AMAP_KEY"] : config["API_KEY"];
    if (key.empty()) {
        printf("ERROR: API Key not found\n");
        return 1;
    }
    printf("API Key: %.8s...\n", key.c_str());

    auto process_file = [&](const string& path, int lng_idx, int lat_idx, const string& label,
                             const string& key) {
        printf("\n=== Processing %s: %s ===\n", label.c_str(), path.c_str());
        vector<string> lines;
        ifstream fin(path);
        string line;
        while (getline(fin, line)) lines.push_back(line);
        fin.close();

        int total = 0, success = 0;
        vector<string> output;

        for (auto& orig_line : lines) {
            string stripped = orig_line;
            stripped.erase(0, stripped.find_first_not_of(" \t\r\n"));
            stripped.erase(stripped.find_last_not_of(" \t\r\n") + 1);

            if (stripped.empty() || stripped[0] == '#') {
                output.push_back(orig_line);
                continue;
            }

            auto parts = split(stripped, '|');
            if ((int)parts.size() < max(lng_idx, lat_idx) + 1) {
                output.push_back(orig_line);
                continue;
            }

            try {
                string name = parts[1];
                double lng = stod(parts[lng_idx]);
                double lat = stod(parts[lat_idx]);
                total++;
                printf("[%d/%d] %s...", total, -1, name.c_str());
                fflush(stdout);

                string amap_id = find_amap_id(key, name, lng, lat);
                string photos = fetch_photos_by_id(key, amap_id);
                if (photos != "-") success++;

                parts.insert(parts.end() - 1, photos);
                string new_line;
                for (size_t i = 0; i < parts.size(); i++) {
                    if (i > 0) new_line += "|";
                    new_line += parts[i];
                }
                output.push_back(new_line);

                this_thread::sleep_for(chrono::milliseconds(150));
                printf(" %s\n", photos != "-" ? "got photos" : "no photos");
            } catch (...) {
                output.push_back(orig_line);
            }
        }

        ofstream fout(path);
        for (auto& l : output) fout << l << "\n";

        printf("\n%s complete: %d/%d have photos\n", label.c_str(), success, total);
    };

    process_file(FOOD_TXT, 2, 3, "foods", key);
    process_file(SPOT_TXT, 4, 5, "spots", key);

    printf("\nDone!\n");
    return 0;
}
