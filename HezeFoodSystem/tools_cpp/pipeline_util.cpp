/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 - C++ 数据处理管道 (共享工具函数)
 * ============================================================================
 *
 * 【本文件的职责】
 *   包含所有 cmd_* 命令函数共用的工具函数、全局常量、数据结构。
 *   不包含任何 main() 函数或 cmd_* 入口函数，仅提供底层能力。
 *
 * 【包含内容】
 *   - 标准库 #include 指令
 *   - 平台适配宏 (_WIN32, M_PI)
 *   - 工作目录验证 (validate_workdir)
 *   - JSON字符串转义/提取工具 (json_escape, json_str, json_num, json_str_array)
 *   - HTTP 网络请求 (url_encode, http_get)
 *   - 全局路径常量 (DATA_DIR, FOOD_TXT, SPOT_TXT, etc.)
 *   - Haversine 距离计算
 *   - 配置文件加载 (load_config)
 *   - 字符串分割 (split)
 *   - 已有数据去重加载 (load_existing_names, load_food_locations, load_spot_locations)
 *   - POI 去重判断 (name_similarity, is_duplicate, PoiLoc)
 *   - 安全数值提取 (safe_float_from_json, safe_str)
 *   - 美食/景点类别推断 (infer_food_category, infer_spot_type)
 *   - 营业时间提取与默认值 (extract_opentime)
 *   - 12维度美食标签生成 (generate_food_tags)
 *   - 坐标解析 (parse_location)
 *   - 高德API POI搜索 (amap_search_poi)
 *   - 数据行格式化 (format_food_line, format_spot_line)
 *   - 逆地理编码 (reverse_geocode)
 *   - 高德POI ID查找 (find_amap_id)
 *   - 高德照片获取 (fetch_photos_by_id)
 *   - 节点图加载 (load_nodes_to_graph)
 *   - 距离→时间换算 (time_from_distance)
 *   - 高德驾车路径规划 (amap_driving)
 *
 * 【API依赖】
 *   所有网络请求通过命令行curl调用高德地图Web API，需curl在PATH中可访问。
 * ============================================================================
 */

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <cstring>
#include <random>
#include <iomanip>

#include "../include/adjacency_graph.h"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

// ===================== 全局路径常量 =====================
extern const string DATA_DIR = "data/";                    // 数据文件目录
extern const string CONFIG_PATH = "config/amap_config.txt"; // 高德API配置文件
extern const string FOOD_TXT = "data/food.txt";             // 美食数据文件
extern const string SPOT_TXT = "data/spot.txt";             // 景点数据文件
extern const string ROAD_TXT = "data/road.txt";             // 道路连接数据文件
extern const string WEB_DATA_DIR = "web/data/";             // Web前端数据输出目录

// ====================================================================
// validate_workdir - 验证工作目录是否正确
// ====================================================================
// 目的：验证 data/ 和 web/ 目录存在，防止从 tools_cpp 子目录误运行。
// 返回：true=工作目录正确，false=目录缺失（已打印错误提示）
bool validate_workdir() {
    std::ifstream dtest("data/food.txt"), wtest("web/index.html");
    bool ok = dtest.good() && wtest.good();
    dtest.close(); wtest.close();
    if (!ok) {
        printf("错误：当前目录不是项目根目录(HezeFoodSystem/)\n");
        printf("请切换到项目根目录后重新运行。例如：\n");
        printf("  cd HezeFoodSystem\n");
        printf("  tools_cpp\\pipeline.exe json\n");
    }
    return ok;
}

// ====================================================================
// json_escape - 对JSON字符串值进行转义
// ====================================================================
// 将双引号、反斜杠、换行等特殊字符替换为JSON转义序列。
string json_escape(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }
    return result;
}

// ====================================================================
// url_encode - URL编码函数
// ====================================================================
// 将中文字符串编码为URL安全的百分号编码格式，用于拼接高德API请求URL。
// 规则：字母数字及 - _ . ~ 保留原样，其他字符转为 %XX 格式。
string url_encode(const string& s) {
    ostringstream escaped;
    escaped << hex << uppercase;
    for (auto c : s) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << setw(2) << setfill('0') << (int)(unsigned char)c;
        }
    }
    return escaped.str();
}

// ====================================================================
// http_get - HTTP GET请求
// ====================================================================
// 通过系统 curl 命令发起 HTTP GET 请求，获取API响应。
// 连接超时10秒，总超时15秒；popen 失败返回空字符串。
string http_get(const string& url) {
    string cmd = "curl -s --connect-timeout 10 --max-time 15 \"" + url + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buffer[4096];
    string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

// ====================================================================
// json_str - 从JSON字符串中提取某个key的字符串值
// ====================================================================
// 在原始JSON中查找 "key":"value" 模式，提取value（不含引号）。
// 限制：仅支持简单字符串值，不支持转义引号和Unicode转义。
string json_str(const string& json, const string& key) {
    string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == string::npos) {
        search = "\"" + key + "\": \"";
        pos = json.find(search);
    }
    if (pos == string::npos) return "";
    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == string::npos) return "";
    return json.substr(pos, end - pos);
}

// ====================================================================
// json_num - 从JSON字符串中提取某个key的数值
// ====================================================================
// 在原始JSON中查找 "key":数字 模式，提取double数值；未找到返回0。
double json_num(const string& json, const string& key) {
    string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == string::npos) return 0;
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
    string num;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) {
        num += json[pos]; pos++;
    }
    return num.empty() ? 0 : stod(num);
}

// ====================================================================
// json_str_array - 从JSON中提取字符串数组 ["a","b","c"]
// ====================================================================
// 在原始JSON中查找 "key":["v1","v2",...] 模式，提取所有字符串元素。
vector<string> json_str_array(const string& json, const string& key) {
    vector<string> result;
    string search = "\"" + key + "\":[";
    size_t pos = json.find(search);
    if (pos == string::npos) return result;
    pos += search.length();
    while (true) {
        size_t q1 = json.find("\"", pos);
        if (q1 == string::npos || q1 >= json.find("]", pos)) break;
        size_t q2 = json.find("\"", q1 + 1);
        if (q2 == string::npos) break;
        result.push_back(json.substr(q1 + 1, q2 - q1 - 1));
        pos = q2 + 1;
    }
    return result;
}

// ====================================================================
// haversine - 计算两点间的球面距离（Haversine公式）
// ====================================================================
// 根据两点的经纬度坐标，计算地球表面两点间的直线距离（单位：米）。
// 使用 WGS84 坐标系，地球平均半径 R=6371000米。
double haversine(double lng1, double lat1, double lng2, double lat2) {
    const double R = 6371000;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlng = (lng2 - lng1) * M_PI / 180.0;
    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlng/2) * sin(dlng/2);
    return R * 2 * atan2(sqrt(a), sqrt(1 - a));
}

// ====================================================================
// load_config - 加载配置文件
// ====================================================================
// 读取 config/amap_config.txt，解析 key=value 键值对。
// 忽略空行和 # 开头的注释行，自动去除首尾空白。
map<string, string> load_config() {
    map<string, string> config;
    ifstream f(CONFIG_PATH);
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq != string::npos) {
            string k = line.substr(0, eq);
            string v = line.substr(eq + 1);
            k.erase(0, k.find_first_not_of(" \t"));
            k.erase(k.find_last_not_of(" \t") + 1);
            v.erase(0, v.find_first_not_of(" \t"));
            v.erase(v.find_last_not_of(" \t") + 1);
            config[k] = v;
        }
    }
    return config;
}

// ====================================================================
// split - 字符串分割
// ====================================================================
// 按指定分隔符分割字符串，每个元素自动去除首尾空白。
vector<string> split(const string& s, char delim) {
    vector<string> result;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        result.push_back(item);
    }
    return result;
}

// ====================================================================
// load_existing_names - 从数据文件加载已有POI名称集合（用于去重）
// ====================================================================
// 读取 TXT 数据文件，提取所有POI名称到 set 中。
// 字段按 | 分隔，名称位于第2列（索引1）。
set<string> load_existing_names(const string& path) {
    set<string> names;
    ifstream f(path);
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        if (parts.size() >= 2) names.insert(parts[1]);
    }
    return names;
}

// ====================================================================
// PoiLoc - POI位置结构体
// ====================================================================
// 存储POI的名称和经纬度坐标，用于去重时的距离计算。
struct PoiLoc {
    string name;   // POI名称
    double lng;    // 经度
    double lat;    // 纬度
};

// ====================================================================
// load_food_locations - 加载美食数据中的经纬度信息
// ====================================================================
// 从 food.txt 提取所有美食的经纬度，用于去重距离比较。
// 字段位置：id|名称|经度|纬度|...(索引2=经度, 索引3=纬度)
vector<PoiLoc> load_food_locations(const string& path) {
    vector<PoiLoc> items;
    ifstream f(path);
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        if (parts.size() >= 4) {
            try {
                items.push_back({parts[1], stod(parts[2]), stod(parts[3])});
            } catch (...) {}
        }
    }
    return items;
}

// ====================================================================
// load_spot_locations - 加载景点数据中的经纬度信息
// ====================================================================
// 从 spot.txt 提取所有景点的经纬度，用于去重距离比较。
// 字段位置：id|名称|描述|地址|经度|纬度|...(索引4=经度, 索引5=纬度)
vector<PoiLoc> load_spot_locations(const string& path) {
    vector<PoiLoc> items;
    ifstream f(path);
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        if (parts.size() >= 6) {
            try {
                items.push_back({parts[1], stod(parts[4]), stod(parts[5])});
            } catch (...) {}
        }
    }
    return items;
}

// ====================================================================
// get_max_id - 获取数据文件中的最大ID值
// ====================================================================
// 读取现有数据，找到最大ID，用于为新POI分配不重复的ID。
int get_max_id(const string& path, int id_index = 0) {
    int max_id = 0;
    ifstream f(path);
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        try {
            int mid = stoi(parts[id_index]);
            if (mid > max_id) max_id = mid;
        } catch (...) {}
    }
    return max_id;
}

// ====================================================================
// name_similarity - 计算两个名称的Jaccard相似度（基于字符集合）
// ====================================================================
// 通过比较两个名称的字符集合交集与并集之比，判断名称是否相似。
double name_similarity(const string& a, const string& b) {
    set<char> sa(a.begin(), a.end());
    set<char> sb(b.begin(), b.end());
    if (sa.empty() || sb.empty()) return 0;
    int inter = 0;
    for (char c : sa) if (sb.count(c)) inter++;
    set<char> uni = sa;
    uni.insert(sb.begin(), sb.end());
    return (double)inter / uni.size();
}

// ====================================================================
// is_duplicate - 判断POI是否与已有数据重复
// ====================================================================
// 综合名称精确匹配和地理位置+名称相似度来判断POI是否已存在。
// 去重规则：1) 名称完全匹配 → 重复
//           2) 距离<200米 且 名称相似度>0.7 → 重复
bool is_duplicate(const string& name, double lng, double lat,
                  set<string>& existing_names, vector<PoiLoc>& existing_locations) {
    if (existing_names.count(name)) return true;
    for (auto& el : existing_locations) {
        double dist = haversine(lng, lat, el.lng, el.lat);
        if (dist < 200 && name_similarity(name, el.name) > 0.7) return true;
    }
    return false;
}

// ====================================================================
// safe_float_from_json - 安全地从JSON提取浮点数（带范围校验）
// ====================================================================
// 从JSON中提取数值字段，对异常值做安全处理（限制在[0, 999]范围内）。
double safe_float_from_json(const string& json, const string& key, double default_val) {
    string srch = "\"" + key + "\":";
    size_t pos = json.find(srch);
    if (pos == string::npos) return default_val;
    pos += srch.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
    string num;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) {
        num += json[pos]; pos++;
    }
    if (num.empty()) return default_val;
    try {
        double v = stod(num);
        if (v < 0 || v > 999) return default_val;
        return v;
    } catch (...) { return default_val; }
}

// ====================================================================
// safe_str - 安全字符串默认值处理
// ====================================================================
// 对空字符串、"-"、"[]"、"null"、"None"等无效值统一替换为默认值。
string safe_str(const string& s, const string& default_val = "-") {
    if (s.empty() || s == "-" || s == "[]" || s == "null" || s == "None") return default_val;
    return s;
}

// ====================================================================
// infer_food_category - 根据名称和类型码推断美食类别
// ====================================================================
// 根据美食名称中包含的关键词以及高德POI类型码，自动判断美食类别。
// 返回：汤类/面食/正餐/饮品/甜品/烧烤/凉菜/小吃
string infer_food_category(const string& name, const string& type_code) {
    if (name.find("汤") != string::npos || name.find("粥") != string::npos) return "汤类";
    vector<string> face_kw = {"面", "馍", "饼", "包", "饺", "粉"};
    for (auto& k : face_kw) if (name.find(k) != string::npos) return "面食";
    vector<string> meat_kw = {"鸡", "鸭", "鹅"};
    for (auto& k : meat_kw) if (name.find(k) != string::npos) return "正餐";
    vector<string> drink_kw = {"茶", "奶", "汁", "咖"};
    for (auto& k : drink_kw) if (name.find(k) != string::npos) return "饮品";
    vector<string> dessert_kw = {"糕", "酥", "糖", "甜"};
    for (auto& k : dessert_kw) if (name.find(k) != string::npos) return "甜品";
    vector<string> bbq_kw = {"串", "烤", "羊"};
    for (auto& k : bbq_kw) if (name.find(k) != string::npos) return "烧烤";
    vector<string> cold_kw = {"凉", "拌"};
    for (auto& k : cold_kw) if (name.find(k) != string::npos) return "凉菜";
    if (!type_code.empty() && type_code.find("050") == 0) return "正餐";
    if (name.find("火锅") != string::npos) return "正餐";
    return "小吃";
}

// ====================================================================
// extract_opentime - 从API返回的biz_ext中提取营业时间
// ====================================================================
// 从高德POI扩展信息中提取营业时间，若API未返回则根据类别给定默认值。
string extract_opentime(const string& biz_ext, const string& category) {
    string ot = json_str(biz_ext, "opentime");
    if (!ot.empty()) {
        string cleaned;
        for (char c : ot) if (c != '|') cleaned += c;
        return cleaned;
    }
    map<string, string> defaults = {
        {"汤类", "06:00-14:00,17:00-21:00"},
        {"面食", "06:00-14:00,17:00-21:00"},
        {"小吃", "08:00-21:00"},
        {"正餐", "10:00-14:00,17:00-22:00"},
        {"烧烤", "17:00-02:00"},
        {"甜品", "09:00-21:00"},
        {"饮品", "09:00-21:00"},
        {"凉菜", "10:00-21:00"}
    };
    auto it = defaults.find(category);
    return it != defaults.end() ? it->second : "10:00-14:00,17:00-22:00";
}

// ====================================================================
// generate_food_tags - 美食标签生成器（12个维度综合打标）
// ====================================================================
// 根据美食的各项属性自动生成丰富的标签集合，用于前端筛选和展示。
//
// 【12个打标维度】
//   维度1: 口味口感 (麻辣、酸辣、清淡等11种)
//   维度2: 食材主料 (羊肉、牛肉、面食等10类)
//   维度3: 烹饪方式 (烤、涮、炒、炖等9种)
//   维度4: 菜系风味 (鲁菜、川菜等，默认鲁菜)
//   维度5: 用餐时段 (早餐/午餐/晚餐/下午茶/宵夜)
//   维度6: 价格档位 (5档)
//   维度7: 推荐指数 (必吃榜/人气高/口碑店)
//   维度8: 历史/传承 (老字号/地方名吃/秘制配方/季节限定)
//   维度9: 消费场景 (一人食/快餐/堂食/外卖/聚餐/宴请)
//   维度10: 社交场景 (朋友聚会/商务宴请/亲子/独自用餐)
//   维度11: 区域特色 (区县特色、人气标签)
//   维度12: 类别默认标签 (兜底标签)
//
// 【兜底机制】口味标签<3个时，用确定性随机从池中补充。
string generate_food_tags(const string& name, const string& category,
                          const string& district, double score, double price,
                          const string& type_code, const string& opentime) {
    vector<string> tags;

    // ===== 维度1: 口味口感 =====
    map<string, vector<string>> taste_map = {
        {"麻辣", {"麻", "辣", "麻辣", "红油", "香辣", "辣味"}},
        {"酸辣", {"酸辣", "酸", "醋", "泡椒", "酸菜"}},
        {"清淡", {"清淡", "清汤", "白味", "淡雅", "清蒸", "清炖", "清炒"}},
        {"酱香", {"酱", "酱油", "红烧", "老抽", "卤", "酱香"}},
        {"蒜蓉", {"蒜", "蒜蓉", "蒜泥", "蒜香"}},
        {"鲜香", {"鲜", "鲜香", "鲜美", "浓汤", "高汤", "醇厚"}},
        {"五香", {"五香", "八角", "桂皮", "花椒", "茴香"}},
        {"甜口", {"甜", "糖", "蜜", "甜品", "糕点", "甜味"}},
        {"孜然", {"孜然", "烤串"}},
        {"酥脆", {"酥", "脆", "酥脆", "香酥", "酥皮", "脆皮"}},
        {"筋道", {"筋道", "Q弹", "弹性", "弹牙"}}
    };
    for (auto& p : taste_map) {
        for (auto& kw : p.second) {
            if (name.find(kw) != string::npos) { tags.push_back(p.first); break; }
        }
    }

    // ===== 维度2: 食材主料 =====
    map<string, vector<string>> ingredient_map = {
        {"羊肉", {"羊肉", "羊排", "羊腿", "羊蝎子", "羊汤"}},
        {"牛肉", {"牛肉", "牛排", "牛腩", "牛腱", "牛杂"}},
        {"猪肉", {"猪肉", "排骨", "蹄膀", "肘子", "五花"}},
        {"鸡肉", {"鸡", "鸡肉", "鸡腿", "鸡翅", "鸡汤", "烧鸡"}},
        {"鸭肉", {"鸭", "鸭肉", "烤鸭", "板鸭"}},
        {"鱼肉", {"鱼", "鱼肉", "鱼片", "烤鱼", "鱼汤", "酸菜鱼"}},
        {"海鲜", {"海鲜", "虾", "蟹", "贝", "鱿鱼"}},
        {"豆制品", {"豆腐", "豆皮", "豆干", "千张", "腐竹", "豆花"}},
        {"面食", {"面", "馍", "饼", "包", "饺", "粉", "馕", "拉面"}},
        {"内脏", {"肠", "肚", "肝", "腰", "百叶", "毛肚", "血"}}
    };
    for (auto& p : ingredient_map) {
        for (auto& kw : p.second) {
            if (name.find(kw) != string::npos) { tags.push_back(p.first); break; }
        }
    }

    // ===== 维度3: 烹饪方式 =====
    map<string, vector<string>> cooking_map = {
        {"烤", {"烤", "烧烤", "烤制", "炭烤", "电烤", "烤串"}},
        {"涮", {"涮", "火锅", "涮锅", "汆"}},
        {"炒", {"炒", "爆炒", "小炒", "煸", "滑炒"}},
        {"炖", {"炖", "煲", "焖", "煨", "熬", "慢炖"}},
        {"蒸", {"蒸", "清蒸", "粉蒸", "蒸制"}},
        {"炸", {"炸", "油炸", "煎", "酥炸"}},
        {"卤", {"卤", "卤制", "卤味", "酱", "酱制"}},
        {"拌", {"拌", "凉拌", "调拌", "沙拉"}},
        {"煮", {"煮", "水煮", "白煮", "煮制", "熬煮"}}
    };
    for (auto& p : cooking_map) {
        for (auto& kw : p.second) {
            if (name.find(kw) != string::npos) { tags.push_back(p.first); break; }
        }
    }

    // ===== 维度4: 菜系风味 =====
    map<string, vector<string>> cuisine_map = {
        {"鲁菜", {"鲁", "山东", "菏泽", "济宁", "济南", "胶东"}},
        {"川菜", {"川", "四川", "成都", "重庆", "麻辣"}},
        {"粤菜", {"粤", "广东", "广州", "潮汕", "烧腊"}},
        {"湘菜", {"湘", "湖南", "长沙", "剁椒"}},
        {"清真", {"清真", "回民", "清真寺"}},
        {"东北菜", {"东北", "黑龙江", "吉林", "辽宁", "哈尔滨", "铁锅"}}
    };
    bool cuisine_found = false;
    for (auto& p : cuisine_map) {
        for (auto& kw : p.second) {
            if (name.find(kw) != string::npos) { tags.push_back(p.first); cuisine_found = true; break; }
        }
        if (cuisine_found) break;
    }
    if (!cuisine_found) tags.push_back("鲁菜");

    // ===== 维度5: 用餐时段 =====
    if (category == "甜品" || category == "饮品") tags.push_back("下午茶");
    else if (name.find("火锅") != string::npos || category == "烧烤") tags.push_back("宵夜");
    else if (category == "汤类" || category == "面食") tags.push_back("早餐");
    else { tags.push_back("午餐"); tags.push_back("晚餐"); }

    // ===== 维度9: 消费场景（价格维度综合） =====
    if (price < 15) { tags.push_back("一人食"); tags.push_back("快餐"); }
    else if (price <= 80) { tags.push_back("堂食"); tags.push_back("外卖"); }
    else { tags.push_back("聚餐"); tags.push_back("宴请"); }

    // ===== 维度7: 推荐指数 =====
    if (score >= 4.7) { tags.push_back("必吃榜"); tags.push_back("口碑店"); }
    else if (score >= 4.5) { tags.push_back("人气高"); tags.push_back("口碑店"); }

    // ===== 维度8: 历史/传承 =====
    vector<string> old_kws = {"老", "传统", "正宗"};
    for (auto& k : old_kws) if (name.find(k) != string::npos) { tags.push_back("老字号"); break; }
    if (name.find(district) != string::npos) tags.push_back("地方名吃");

    // ===== 维度6: 价格档位 =====
    if (price <= 10) tags.push_back("人均10元以下");
    else if (price <= 30) tags.push_back("人均10-30元");
    else if (price <= 60) tags.push_back("人均30-60元");
    else if (price <= 100) tags.push_back("人均60-100元");
    else tags.push_back("人均100元以上");

    // ===== 24小时营业标识 =====
    if (!opentime.empty()) {
        if (opentime.find("24小时") != string::npos || opentime.find("全天") != string::npos)
            tags.push_back("24小时营业");
    }

    // ===== 维度10: 社交场景 =====
    if (name.find("火锅") != string::npos || category == "烧烤") tags.push_back("朋友聚会");
    if (price > 80) tags.push_back("商务宴请");
    if (name.find("亲子") != string::npos) tags.push_back("亲子");
    if (price <= 20) tags.push_back("独自用餐");

    // ===== 维度8扩展: 秘制配方/季节限定 =====
    if (name.find("秘制") != string::npos || name.find("秘") != string::npos) tags.push_back("秘制配方");
    vector<string> limit_kws = {"限定", "季节"};
    for (auto& k : limit_kws) if (name.find(k) != string::npos) { tags.push_back("季节限定"); break; }

    // ===== 维度11: 区域特色 =====
    if (district != "菏泽市" && !district.empty()) {
        string tag = district + "特色";
        bool has_it = false;
        for (auto& t : tags) if (t == tag) { has_it = true; break; }
        if (!has_it) tags.push_back(tag);
    }

    // ===== 维度12: 类别默认标签 =====
    map<string, vector<string>> cat_defaults = {
        {"汤类", {"鲜香", "原味", "清淡", "暖胃", "滋补"}},
        {"面食", {"筋道", "快餐", "主食"}},
        {"小吃", {"现点现做", "地方特色"}},
        {"正餐", {"菜品丰富", "环境好"}},
        {"烧烤", {"孜然", "宵夜", "朋友聚会"}},
        {"甜品", {"甜口", "下午茶"}},
        {"饮品", {"清爽", "下午茶"}},
        {"凉菜", {"开胃"}}
    };
    auto cd_it = cat_defaults.find(category);
    if (cd_it != cat_defaults.end()) for (auto& t : cd_it->second) tags.push_back(t);

    // ===== 消费场景二次补充（更细粒度的价格分层） =====
    if (price <= 10) { tags.push_back("平价实惠"); tags.push_back("快餐"); }
    else if (price <= 30) { tags.push_back("家常"); tags.push_back("堂食"); }
    else if (price <= 60) { tags.push_back("品质餐厅"); tags.push_back("朋友聚会"); }
    else if (price <= 100) { tags.push_back("环境好"); tags.push_back("商务宴请"); }
    else { tags.push_back("高端"); tags.push_back("宴请"); tags.push_back("包厢"); }

    // ===== 口味标签兜底补充 =====
    set<string> taste_set = {
        "麻辣","酸辣","清淡","酱香","蒜蓉","鲜香","五香","甜口","孜然","咖喱",
        "鲜嫩","酥脆","软糯","筋道","肥而不腻","入口即化","Q弹"
    };
    int taste_count = 0;
    for (auto& t : tags) if (taste_set.count(t)) taste_count++;
    if (taste_count < 3) {
        vector<string> pool = {"鲜香", "原味", "五香", "酱香", "清淡"};
        unsigned seed = 0;
        for (char c : name) seed += (unsigned char)c;
        mt19937 rng(seed);
        shuffle(pool.begin(), pool.end(), rng);
        int added = 0;
        for (auto& f : pool) {
            if (added >= 2 + (seed % 2)) break;
            bool has = false;
            for (auto& t : tags) if (t == f) { has = true; break; }
            if (!has) { tags.push_back(f); added++; }
        }
    }

    // ===== 维度11扩展: 区域人气标签 =====
    if (!district.empty()) {
        tags.push_back(district + "美食");
        tags.push_back(district + "人气");
    }

    // ===== 全局默认标签 =====
    if (name.find("菏泽") == string::npos) tags.push_back("菏泽");
    tags.push_back("美食推荐");
    tags.push_back("人气美食");

    // ===== 标签去重 =====
    vector<string> unique;
    set<string> seen;
    for (auto& t : tags) {
        if (!seen.count(t)) { seen.insert(t); unique.push_back(t); }
    }

    string result;
    for (size_t i = 0; i < unique.size(); i++) {
        if (i > 0) result += ",";
        result += unique[i];
    }
    return result;
}

// ====================================================================
// infer_spot_type - 根据高德类型名推断景点类别
// ====================================================================
// 将高德API返回的POI类型名映射到系统的景点分类。
// 返回：自然景观/历史文化/主题公园
string infer_spot_type(const string& type_name) {
    if (type_name.empty()) return "自然景观";
    if (type_name.find("公园") != string::npos || type_name.find("风景") != string::npos ||
        type_name.find("山") != string::npos || type_name.find("湿地") != string::npos ||
        type_name.find("河") != string::npos || type_name.find("湖") != string::npos)
        return "自然景观";
    if (type_name.find("博物") != string::npos || type_name.find("古迹") != string::npos ||
        type_name.find("纪念") != string::npos || type_name.find("历史") != string::npos ||
        type_name.find("红色") != string::npos || type_name.find("庙") != string::npos)
        return "历史文化";
    if (type_name.find("游乐") != string::npos || type_name.find("乐园") != string::npos ||
        type_name.find("主题") != string::npos)
        return "主题公园";
    return "自然景观";
}

// ====================================================================
// parse_location - 解析 "经度,纬度" 格式的坐标字符串
// ====================================================================
void parse_location(const string& loc_str, double& lng, double& lat) {
    auto parts = split(loc_str, ',');
    if (parts.size() == 2) {
        try { lng = stod(parts[0]); lat = stod(parts[1]); return; }
        catch (...) {}
    }
    lng = 0; lat = 0;
}

// ====================================================================
// amap_search_poi - 搜索高德地图POI（文本搜索接口）
// ====================================================================
// 调用高德地图 /v3/place/text 文本搜索接口，搜索指定关键词的POI。
// 逐页请求API，使用字符级状态机解析嵌套JSON对象，按id去重。
vector<string> amap_search_poi(const string& key, const string& keywords, const string& types,
                                const string& city, int page_size = 20, int max_pages = 2) {
    printf("  Searching: %s (%d pages max)...\n", keywords.c_str(), max_pages);
    vector<string> all_pois;
    set<string> seen_ids;
    for (int page = 1; page <= max_pages; page++) {
        stringstream url;
        url << "https://restapi.amap.com/v3/place/text?key=" << key
            << "&keywords=" << url_encode(keywords)
            << "&city=" << url_encode(city)
            << "&citylimit=true&types=" << types
            << "&offset=" << page_size << "&page=" << page << "&output=json";
        string resp = http_get(url.str());
        if (resp.empty()) break;

        string status = json_str(resp, "status");
        if (status != "1") break;

        string pois_str = "\"pois\":[";
        size_t pois_start = resp.find(pois_str);
        if (pois_start == string::npos) break;
        pois_start += pois_str.length();

        int depth = 0;
        bool in_str = false;
        size_t i = pois_start;
        size_t item_start = pois_start;
        while (i < resp.length()) {
            char c = resp[i];
            if (c == '\\') { i += 2; continue; }
            if (c == '"') in_str = !in_str;
            if (!in_str) {
                if (c == '{') {
                    if (depth == 0) item_start = i;
                    depth++;
                } else if (c == '}') {
                    depth--;
                    if (depth == 0) {
                        string poi_obj = resp.substr(item_start, i - item_start + 1);
                        string pid = json_str(poi_obj, "id");
                        if (pid.empty()) pid = json_str(poi_obj, "name");
                        if (!pid.empty() && !seen_ids.count(pid)) {
                            seen_ids.insert(pid);
                            all_pois.push_back(poi_obj);
                        }
                    }
                } else if (c == ']') {
                    break;
                }
            }
            i++;
        }

        if (page < max_pages) this_thread::sleep_for(chrono::milliseconds(300));
    }
    printf("  Found %zu POIs for '%s'\n", all_pois.size(), keywords.c_str());
    return all_pois;
}

// ====================================================================
// format_food_line - 格式化美食数据行为固定分隔符格式
// ====================================================================
// 格式：id|名称|经度|纬度|人均价格|评分|类别|地址|营业时间|照片|标签
string format_food_line(int id, const string& name, double lng, double lat,
                        double price, double score, const string& category,
                        const string& address, const string& opentime,
                        const string& photos, const string& tags) {
    stringstream ss;
    ss << fixed << setprecision(1);
    ss << id << "|" << name << "|" << lng << "|" << lat << "|"
       << price << "|" << score << "|" << category << "|"
       << address << "|" << opentime << "|" << photos << "|" << tags;
    return ss.str();
}

// ====================================================================
// format_spot_line - 格式化景点数据行为固定分隔符格式
// ====================================================================
// 格式：id|名称|描述|地址|经度|纬度|类型|票价|开放时间|推荐时长|最佳季节|评分|标签
string format_spot_line(int id, const string& name, const string& description,
                        const string& address, double lng, double lat,
                        const string& type, const string& ticketInfo,
                        const string& openingTime, const string& recommendDuration,
                        const string& bestSeason, double score, const string& tags) {
    stringstream ss;
    ss << fixed << setprecision(1);
    ss << id << "|" << name << "|" << description << "|" << address << "|"
       << lng << "|" << lat << "|" << type << "|" << ticketInfo << "|"
       << openingTime << "|" << recommendDuration << "|" << bestSeason << "|"
       << score << "|" << tags;
    return ss.str();
}

// ====================================================================
// reverse_geocode - 逆地理编码：根据坐标获取结构化地址
// ====================================================================
// 调用高德地图 /v3/geocode/regeo 逆地理编码接口。
string reverse_geocode(const string& key, double lng, double lat) {
    stringstream url;
    url << "https://restapi.amap.com/v3/geocode/regeo?key=" << key
        << "&location=" << fixed << setprecision(6) << lng << "," << lat
        << "&extensions=base&output=json";
    string resp = http_get(url.str());
    if (resp.empty()) return "";
    string status = json_str(resp, "status");
    if (status != "1") return "";

    size_t regeo_pos = resp.find("\"regeocode\":{");
    if (regeo_pos == string::npos) return "";
    string address = json_str(resp.substr(regeo_pos), "formatted_address");
    return address;
}

// ====================================================================
// find_amap_id - 通过名称+坐标查找POI在高德的唯一ID
// ====================================================================
// 用POI名称搜索高德API，在返回结果中找到坐标最接近的那个POI的ID。
// 返回：匹配的POI ID；未找到或距离超过5000米返回空字符串。
string find_amap_id(const string& key, const string& name, double lng, double lat) {
    stringstream url;
    url << "https://restapi.amap.com/v3/place/text?key=" << key
        << "&keywords=" << url_encode(name)
        << "&city=" << url_encode("菏泽")
        << "&output=json";
    string resp = http_get(url.str());
    if (resp.empty()) return "";
    string status = json_str(resp, "status");
    if (status != "1") return "";

    string pois_str = "\"pois\":[";
    size_t pp = resp.find(pois_str);
    if (pp == string::npos) return "";

    string best_id;
    double best_dist = 1e18;
    size_t pos = pp + pois_str.length();

    while (pos < resp.length()) {
        size_t ob = resp.find("{", pos);
        if (ob == string::npos || ob >= resp.find("]", pos)) break;
        int depth = 0;
        bool in_str = false;
        size_t i = ob;
        while (i < resp.length()) {
            char c = resp[i];
            if (c == '\\') { i += 2; continue; }
            if (c == '"') in_str = !in_str;
            if (!in_str) {
                if (c == '{') depth++;
                else if (c == '}') {
                    depth--;
                    if (depth == 0) {
                        string pobj = resp.substr(ob, i - ob + 1);
                        string pid = json_str(pobj, "id");
                        string ploc = json_str(pobj, "location");
                        auto lparts = split(ploc, ',');
                        try {
                            double plng = stod(lparts[0]), plat = stod(lparts[1]);
                            double d = haversine(lng, lat, plng, plat);
                            if (d < best_dist && d < 5000) {
                                best_dist = d;
                                best_id = pid;
                            }
                        } catch (...) {}
                        pos = i + 1;
                        break;
                    }
                }
            }
            i++;
        }
        if (depth == 0) pos = i + 1;
        else break;
    }
    return best_id;
}

// ====================================================================
// fetch_photos_by_id - 根据高德POI ID获取图片URL列表
// ====================================================================
// 调用高德 /v3/place/detail 地点详情接口，提取POI的照片URL。
// 返回：最多3张照片的URL（分号分隔）；无照片返回"-"。
string fetch_photos_by_id(const string& key, const string& amap_id) {
    if (amap_id.empty()) return "-";
    stringstream url;
    url << "https://restapi.amap.com/v3/place/detail?key=" << key
        << "&id=" << amap_id;
    string resp = http_get(url.str());
    if (resp.empty()) return "-";
    string status = json_str(resp, "status");
    if (status != "1") return "-";

    string photos_str = "\"photos\":[";
    size_t pp = resp.find(photos_str);
    if (pp == string::npos) return "-";

    vector<string> urls;
    size_t pos = pp + photos_str.length();
    while (pos < resp.length() && urls.size() < 3) {
        size_t ob = resp.find("{", pos);
        if (ob == string::npos || ob >= resp.find("]", pos)) break;
        int depth = 0;
        bool in_str = false;
        size_t i = ob;
        while (i < resp.length()) {
            char c = resp[i];
            if (c == '\\') { i += 2; continue; }
            if (c == '"') in_str = !in_str;
            if (!in_str) {
                if (c == '{') depth++;
                else if (c == '}') {
                    depth--;
                    if (depth == 0) {
                        string pobj = resp.substr(ob, i - ob + 1);
                        string purl = json_str(pobj, "url");
                        if (!purl.empty()) urls.push_back(purl);
                        pos = i + 1;
                        break;
                    }
                }
            }
            i++;
        }
        if (depth == 0) pos = i + 1;
        else break;
    }

    if (urls.empty()) return "-";
    string result;
    for (size_t i = 0; i < urls.size(); i++) {
        if (i > 0) result += ";";
        result += urls[i];
    }
    return result;
}

// ====================================================================
// load_nodes_to_graph — 从数据文件加载节点到邻接表图
// ====================================================================
// 读取 food.txt 或 spot.txt，将每个POI添加为图的顶点。
void load_nodes_to_graph(const string& path, AdjacencyGraph& graph,
                          const char* type, int lngIdx, int latIdx) {
    ifstream f(path);
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        if ((int)parts.size() <= max(lngIdx, latIdx)) continue;
        try {
            int id = stoi(parts[0]);
            string name = parts[1];
            double lng = stod(parts[lngIdx]);
            double lat = stod(parts[latIdx]);
            graph.addVertex(id, name.c_str(), lng, lat, type);
        } catch (...) {}
    }
}

// ====================================================================
// time_from_distance - 根据距离估算通行时间
// ====================================================================
// 将距离（米）按给定速度换算为时间（分钟），最少1分钟。
int time_from_distance(double dist_m, double speed_kmh = 40) {
    return max(1, (int)round(dist_m / (speed_kmh * 1000 / 60)));
}

// ====================================================================
// amap_driving - 调用高德驾车路径规划API获取真实距离和时间
// ====================================================================
// 获取两点之间的实际驾车距离和耗时。
// 返回：pair<距离(米), 时间(分钟)>；API失败返回{0,0}。
pair<int,int> amap_driving(const string& key, double olng, double olat, double dlng, double dlat) {
    if (key.empty()) return {0, 0};
    stringstream url;
    url << "https://restapi.amap.com/v3/direction/driving?"
        << "origin=" << fixed << setprecision(6) << olng << "," << olat
        << "&destination=" << dlng << "," << dlat
        << "&key=" << key << "&strategy=0";
    string resp = http_get(url.str());
    if (resp.empty()) return {0, 0};
    string status = json_str(resp, "status");
    if (status != "1") return {0, 0};

    size_t rp = resp.find("\"route\":{");
    if (rp == string::npos) return {0, 0};
    string route_seg = resp.substr(rp);

    size_t pp = route_seg.find("\"paths\":[");
    if (pp == string::npos) return {0, 0};
    size_t ob = route_seg.find("{", pp);
    if (ob == string::npos) return {0, 0};

    int depth = 0;
    bool in_str = false;
    size_t i = ob;
    while (i < route_seg.length()) {
        char c = route_seg[i];
        if (c == '\\') { i += 2; continue; }
        if (c == '"') in_str = !in_str;
        if (!in_str) {
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) {
                    string path_obj = route_seg.substr(ob, i - ob + 1);
                    double dist = json_num(path_obj, "distance");
                    double dur = json_num(path_obj, "duration");
                    return {(int)dist, (int)(dur / 60)};
                }
            }
        }
        i++;
    }
    return {0, 0};
}
