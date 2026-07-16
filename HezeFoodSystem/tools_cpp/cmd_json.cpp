/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 - C++ 数据处理管道 (cmd_json)
 * ============================================================================
 *
 * 【命令功能】
 *   pipeline json — 生成Web前端JSON文件命令（替代 gen_web_json.py）
 *
 * 【输入文件】
 *   data/food.txt    - 美食数据（管道符分隔）
 *   data/spot.txt    - 景点数据（管道符分隔）
 *
 * 【输出文件】
 *   web/data/food.json   - 美食JSON数组
 *   web/data/spot.json   - 景点JSON数组
 *   web/data/route.json  - 推荐路线JSON
 *
 * 【处理流程】
 *   1) 读取 food.txt，按 | 分隔各字段
 *   2) 智能识别地址、营业时间、照片、标签等字段位置
 *      由于历史数据格式可能存在差异，通过关键词启发式判断字段含义
 *   3) 组装为JSON对象并写入JSON数组
 *   4) 同样处理 spot.txt
 *   5) 生成一条预设的"菏泽美食文化一日游"路线JSON
 *
 * 【字段识别策略（美食）】
 *   利用启发式规则判断地址（含"路""街""区""市""县"等）、
 *   营业时间（含":"和"-"或"全天"）、照片（含"http"或";"）、
 *   标签（最后一个字段）。标签字段进一步按逗号拆分为数组。
 *
 * 【本文件依赖 pipeline_util.cpp】
 *   所有工具函数均在其中定义，本文件仅通过 extern 声明引用。
 * ============================================================================
 */

#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>

using namespace std;

// ---- 外部工具函数声明 (定义在 pipeline_util.cpp) ----
extern bool validate_workdir();
extern vector<string> split(const string& s, char delim);
extern string json_escape(const string& s);
extern double haversine(double lng1, double lat1, double lng2, double lat2);

// ---- 全局常量（定义在 pipeline_util.cpp）----
extern const string FOOD_TXT;
extern const string SPOT_TXT;
extern const string WEB_DATA_DIR;

// ====================================================================
// cmd_json - 生成Web前端JSON文件命令
// ====================================================================
int cmd_json() {
    if (!validate_workdir()) return 1;
    printf("============================================================\n");
    printf("  Generate Web JSON from TXT\n");
    printf("============================================================\n");

    auto write_json_file = [](const string& path, const string& content) {
        string mkdir_cmd = "mkdir \"" + string("web\\data") + "\" 2>nul";
        system(mkdir_cmd.c_str());
        ofstream f(path);
        f << content;
        return true;
    };

    // ===== 解析美食数据 =====
    vector<string> food_json_items;
    ifstream ffood(FOOD_TXT);
    string line;
    while (getline(ffood, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        if (parts.size() < 8) continue;
        try {
            int fid = stoi(parts[0]);
            string name = parts[1];
            double lng = stod(parts[2]);
            double lat = stod(parts[3]);
            double price = stod(parts[4]);
            double score = stod(parts[5]);
            string category = parts[6];

            string address = "-";
            string opentime = "-";
            string tags_part = "";

            int part8_idx = 7;
            if ((int)parts.size() > part8_idx) {
                string p8 = parts[part8_idx];
                bool is_addr = (p8.find("区") != string::npos || p8.find("路") != string::npos ||
                               p8.find("街") != string::npos || p8.find("市") != string::npos ||
                               p8.find("县") != string::npos || p8 == "-" || p8 == "无法获取");
                if (is_addr) {
                    address = p8;
                    if ((int)parts.size() > part8_idx + 1) {
                        string p9 = parts[part8_idx + 1];
                        bool is_time = (p9.find(":") != string::npos || p9.find("-") != string::npos ||
                                       p9.find("全天") != string::npos);
                        if (is_time) {
                            opentime = p9;
                            if ((int)parts.size() > part8_idx + 2) tags_part = parts[part8_idx + 2];
                            else tags_part = p9;
                        } else {
                            if ((int)parts.size() > part8_idx + 1) tags_part = parts[part8_idx + 1];
                            else tags_part = p8;
                        }
                    } else {
                        tags_part = p8;
                    }
                } else {
                    tags_part = p8;
                }
            }

            tags_part = parts.back();

            string photos_str = parts.size() >= 2 ? parts[parts.size() - 2] : "-";
            bool photos_is_tag = true;
            if (!photos_str.empty() && (photos_str.find("http") != string::npos || photos_str.find(";") != string::npos || photos_str == "-"))
                photos_is_tag = false;
            if (photos_is_tag) photos_str = "-";

            vector<string> photos;
            if (photos_str != "-") {
                auto purls = split(photos_str, ';');
                for (auto& pu : purls) if (!pu.empty() && pu != "-") photos.push_back(pu);
            }
            if (photos.empty()) photos.push_back("-");

            vector<string> tags;
            auto tlist = split(tags_part, ',');
            for (auto& t : tlist) if (!t.empty()) tags.push_back(t);

            stringstream ss;
            ss << "  {";
            ss << "\"id\":" << fid;
            ss << ",\"name\":\"" << json_escape(name) << "\"";
            ss << ",\"lng\":" << fixed << setprecision(6) << lng;
            ss << ",\"lat\":" << fixed << setprecision(6) << lat;
            ss << ",\"price\":" << fixed << setprecision(0) << price;
            ss << ",\"score\":" << fixed << setprecision(1) << score;
            ss << ",\"category\":\"" << json_escape(category) << "\"";
            ss << ",\"address\":\"" << json_escape(address) << "\"";
            ss << ",\"opentime\":\"" << json_escape(opentime) << "\"";
            ss << ",\"photos\":[";
            for (size_t i = 0; i < photos.size(); i++) {
                if (i > 0) ss << ",";
                ss << "\"" << photos[i] << "\"";
            }
            ss << "]";
            ss << ",\"tags\":[";
            for (size_t i = 0; i < tags.size(); i++) {
                if (i > 0) ss << ",";
                ss << "\"" << tags[i] << "\"";
            }
            ss << "]}";
            food_json_items.push_back(ss.str());
        } catch (...) {}
    }
    ffood.close();

    string food_json = "[\n";
    for (size_t i = 0; i < food_json_items.size(); i++) {
        if (i > 0) food_json += ",\n";
        food_json += food_json_items[i];
    }
    food_json += "\n]\n";
    write_json_file(WEB_DATA_DIR + "food.json", food_json);
    printf("Parsed foods: %zu -> web/data/food.json\n", food_json_items.size());

    // ===== 解析景点数据 =====
    vector<string> spot_json_items;
    ifstream fspot(SPOT_TXT);
    while (getline(fspot, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        if (parts.size() < 13) continue;
        try {
            int sid = stoi(parts[0]);
            string name = parts[1];
            string description = parts[2];
            string address = parts[3];
            double lng = stod(parts[4]);
            double lat = stod(parts[5]);
            string type = parts[6];
            string ticketInfo = parts[7];
            string openingTime = parts[8];
            string recommendDuration = parts[9];
            string bestSeason = parts[10];
            double score = stod(parts[11]);

            string photos_str = "-";
            vector<string> tag_fields;
            for (size_t i = 12; i < parts.size(); i++) {
                if (photos_str == "-" && (parts[i].find(";") != string::npos || parts[i].find("http") != string::npos))
                    photos_str = parts[i];
                else
                    tag_fields.push_back(parts[i]);
            }

            vector<string> photos;
            if (photos_str != "-") {
                auto purls = split(photos_str, ';');
                for (auto& pu : purls) if (!pu.empty() && pu != "-") photos.push_back(pu);
            }

            stringstream ss;
            ss << "  {";
            ss << "\"id\":" << sid;
            ss << ",\"name\":\"" << json_escape(name) << "\"";
            ss << ",\"description\":\"" << json_escape(description) << "\"";
            ss << ",\"address\":\"" << json_escape(address) << "\"";
            ss << ",\"lng\":" << fixed << setprecision(6) << lng;
            ss << ",\"lat\":" << fixed << setprecision(6) << lat;
            ss << ",\"type\":\"" << json_escape(type) << "\"";
            ss << ",\"ticketInfo\":\"" << json_escape(ticketInfo) << "\"";
            ss << ",\"openingTime\":\"" << json_escape(openingTime) << "\"";
            ss << ",\"recommendDuration\":\"" << json_escape(recommendDuration) << "\"";
            ss << ",\"bestSeason\":\"" << json_escape(bestSeason) << "\"";
            ss << ",\"score\":" << fixed << setprecision(1) << score;
            ss << ",\"photos\":[";
            for (size_t i = 0; i < photos.size(); i++) {
                if (i > 0) ss << ",";
                ss << "\"" << photos[i] << "\"";
            }
            ss << "]";
            ss << ",\"tags\":[";
            for (size_t i = 0; i < tag_fields.size(); i++) {
                if (i > 0) ss << ",";
                ss << "\"" << tag_fields[i] << "\"";
            }
            ss << "]}";
            spot_json_items.push_back(ss.str());
        } catch (...) {}
    }
    fspot.close();

    string spot_json = "[\n";
    for (size_t i = 0; i < spot_json_items.size(); i++) {
        if (i > 0) spot_json += ",\n";
        spot_json += spot_json_items[i];
    }
    spot_json += "\n]\n";
    write_json_file(WEB_DATA_DIR + "spot.json", spot_json);
    printf("Parsed spots: %zu -> web/data/spot.json\n", spot_json_items.size());

    // ===== 生成推荐路线JSON =====
    string route_json = "{\n";
    route_json += "  \"name\":\"菏泽美食文化一日游路线\",\n";
    route_json += "  \"found\":true,\n";
    route_json += "  \"path\":[[115.497379,35.279915],[115.500907,35.241715],[115.499334,35.246949],[115.437718,35.251241],[115.476401,35.235636]],\n";
    route_json += "  \"waypoints\":[\n";
    route_json += "    {\"type\":\"spot\",\"lng\":115.497379,\"lat\":35.279915,\"name\":\"曹州牡丹园\"},\n";
    route_json += "    {\"type\":\"food\",\"lng\":115.500907,\"lat\":35.241715,\"name\":\"菏泽烧牛肉\"},\n";
    route_json += "    {\"type\":\"food\",\"lng\":115.499334,\"lat\":35.246949,\"name\":\"胡辣汤\"},\n";
    route_json += "    {\"type\":\"food\",\"lng\":115.437718,\"lat\":35.251241,\"name\":\"羊肉汤\"},\n";
    route_json += "    {\"type\":\"spot\",\"lng\":115.476401,\"lat\":35.235636,\"name\":\"天香公园\"}\n";
    route_json += "  ],\n";

    double total_dist = 0;
    total_dist += haversine(115.497379,35.279915,115.500907,35.241715);
    total_dist += haversine(115.500907,35.241715,115.499334,35.246949);
    total_dist += haversine(115.499334,35.246949,115.437718,35.251241);
    total_dist += haversine(115.437718,35.251241,115.476401,35.235636);

    stringstream rt;
    rt << ",\"totalDistance\":" << fixed << setprecision(1) << total_dist;
    rt << ",\"totalTime\":" << fixed << setprecision(1) << (total_dist / (40.0 * 1000 / 60));
    route_json += rt.str();
    route_json += "\n}\n";
    write_json_file(WEB_DATA_DIR + "route.json", route_json);
    printf("Generated route.json\n");
    printf("Route distance: %.1f km\n", total_dist / 1000);

    printf("\nDone!\n");
    return 0;
}
