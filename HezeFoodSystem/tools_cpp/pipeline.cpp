/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 - C++ 数据处理管道 (Pipeline)
 * ============================================================================
 *
 * 【整体目的】
 *   本文件是一个单文件C++命令行工具，整合了原Python工具链中6个脚本的全部功能：
 *   1. expand   - 通过高德地图API搜索并扩充POI数据（替代 expand_data.py）
 *   2. fill     - 通过逆地理编码填充缺失地址（替代 fill_addresses.py 和 fetch_amap_data.py）
 *   3. photos   - 获取POI图片URL（替代 fetch_photos.py）
 *   4. roads    - 重新计算道路连接距离与时间（替代 recalc_roads.py）
 *   5. json     - 从TXT生成前端所需的Web JSON文件（替代 gen_web_json.py）
 *   6. all      - 按顺序执行以上全部步骤
 *
 * 【编译方法】
 *   g++ -std=c++17 -O2 pipeline.cpp -o pipeline.exe
 *   依赖：curl（命令行HTTP工具，需在系统PATH中可用）
 *
 * 【使用方法】
 *   pipeline expand    - 搜索并扩充POI数据
 *   pipeline fill      - 填充缺失的地址信息
 *   pipeline photos    - 获取POI照片链接
 *   pipeline roads     - 重新计算道路连接
 *   pipeline json      - 生成前端JSON文件
 *   pipeline all       - 按顺序执行全部步骤
 *
 * 【API依赖】
 *   所有网络请求通过命令行curl调用高德地图Web API：
 *   - 地点搜索:   /v3/place/text
 *   - 地点详情:   /v3/place/detail
 *   - 逆地理编码: /v3/geocode/regeo
 *   - 驾车路径:   /v3/direction/driving
 *   API Key配置在 config/amap_config.txt 中
 *
 * 【数据文件格式】
 *   data/food.txt: id|名称|经度|纬度|人均价格|评分|类别|地址|营业时间|照片|标签
 *   data/spot.txt: id|名称|描述|地址|经度|纬度|类型|票价|开放时间|推荐时长|最佳季节|评分|标签
 *   data/road.txt: 起点ID|终点ID|距离(米)|预计时间(分钟)
 *
 * 【HTTP/JSON处理策略】
 *   本工具不依赖任何第三方JSON库，而是通过简单的字符串搜索(如 "\"key\":\"")
 *   和字符级状态机解析来提取JSON字段。这样做的好处是零外部依赖、编译简单、
 *   部署方便。代价是对复杂嵌套JSON的支持有限，但足够处理高德API的标准响应。
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

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 对JSON字符串值进行转义：将双引号和反斜杠替换为转义序列
std::string json_escape(const std::string& s) {
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

using namespace std;

// ===================== 全局路径常量 =====================
const string DATA_DIR = "data/";                    // 数据文件目录
const string CONFIG_PATH = "config/amap_config.txt"; // 高德API配置文件
const string FOOD_TXT = "data/food.txt";             // 美食数据文件
const string SPOT_TXT = "data/spot.txt";             // 景点数据文件
const string ROAD_TXT = "data/road.txt";             // 道路连接数据文件
const string WEB_DATA_DIR = "web/data/";             // Web前端数据输出目录

// ====================================================================
// url_encode - URL编码函数
// ====================================================================
// 目的：将中文字符串编码为URL安全的百分号编码格式，用于拼接高德API请求URL。
// 参数：s - 待编码的原始字符串
// 返回：URL编码后的字符串
// 规则：字母数字及 - _ . ~ 保留原样，其他字符（包括中文）转为 %XX 格式。
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
// 目的：通过系统curl命令发起HTTP GET请求，获取API响应。
// 参数：url - 完整的请求URL（应已包含所有参数和编码）
// 返回：HTTP响应体的字符串；失败或超时返回空字符串。
// 错误处理：连接超时10秒，总超时15秒；popen失败返回空。
// 注意：此函数通过 popen 调用外部 curl 命令，需要 curl 在系统PATH中。
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
// 目的：在原始JSON字符串中查找 "key":"value" 或 "key": "value" 模式，提取value。
// 参数：json - 原始JSON字符串；key - 要查找的字段名
// 返回：找到的字符串值（不含引号）；未找到返回空字符串。
// 限制：仅支持简单字符串值，不支持转义引号和Unicode转义，但足以处理高德API响应。
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
// 目的：在原始JSON中查找 "key":数字 或 "key":"数字" 模式，提取数值。
// 参数：json - 原始JSON字符串；key - 要查找的字段名
// 返回：提取到的double数值；未找到返回0。
// 处理：跳过冒号后的空格和引号，连续读取数字/小数点/负号字符。
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
// 目的：在原始JSON中查找 "key":["v1","v2",...] 模式，提取所有字符串元素。
// 参数：json - 原始JSON字符串；key - 数组字段名
// 返回：字符串向量，每个元素是数组中的一个字符串值。
// 限制：假设数组内所有元素都是字符串类型，不支持嵌套对象。
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
// 目的：根据两点的经纬度坐标，计算地球表面两点间的直线距离。
// 参数：lng1, lat1 - 起点的经度、纬度（度数）
//       lng2, lat2 - 终点的经度、纬度（度数）
// 返回：两点间距离，单位：米
// 公式说明：
//   a = sin²(Δlat/2) + cos(lat1)*cos(lat2)*sin²(Δlng/2)
//   c = 2*atan2(√a, √(1-a))
//   d = R * c
//   其中 R=6371000米（地球平均半径），所有角度先转换为弧度。
// 坐标系：使用WGS84坐标系（GPS坐标），与高德地图GCJ-02坐标可能有偏移。
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
// 目的：读取 config/amap_config.txt，解析键值对。
// 返回：map<string,string>，包含所有配置项（如 AMAP_KEY 或 API_KEY）。
// 格式：每行 key=value，忽略空行和 # 开头的注释行，自动去除首尾空白。
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
// 目的：按指定分隔符分割字符串。
// 参数：s - 待分割字符串；delim - 分隔字符
// 返回：分割后的字符串向量，每个元素自动去除首尾空白。
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
// 目的：读取TXT数据文件，提取所有POI名称到set中，用于去重判断。
// 参数：path - 数据文件路径（food.txt 或 spot.txt）
// 返回：已有名称的set集合
// 注意：字段按 | 分隔，名称位于第2列（索引1）。
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
// 用途：存储POI的名称和经纬度坐标，用于去重时的距离计算。
struct PoiLoc {
    string name;   // POI名称
    double lng;    // 经度
    double lat;    // 纬度
};

// ====================================================================
// load_food_locations - 加载美食数据中的经纬度信息
// ====================================================================
// 目的：从 food.txt 中提取所有美食的经纬度，用于去重距离比较。
// 参数：path - food.txt 路径
// 返回：PoiLoc向量，包含名称、经度、纬度
// 字段位置：id|名称|经度|纬度|...(即第3列为经度，第4列为纬度) 索引2,3
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
// 目的：从 spot.txt 中提取所有景点的经纬度，用于去重距离比较。
// 参数：path - spot.txt 路径
// 返回：PoiLoc向量
// 字段位置：id|名称|描述|地址|经度|纬度|...(索引4为经度，索引5为纬度)
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
// 目的：读取现有数据，找到最大ID，用于为新POI分配不重复的ID。
// 参数：path - 数据文件路径；id_index - ID字段的列索引（默认第0列）
// 返回：最大ID整数值；若文件为空或无有效ID则返回0。
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
// 目的：通过比较两个名称的字符集合交集与并集之比，判断名称是否相似。
// 参数：a, b - 两个待比较的名称字符串
// 返回：0~1之间的相似度，Jaccard系数 = |A∩B| / |A∪B|
// 用途：去重时辅助判断——距离近且名称相似度高的两个POI视为重复。
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
// 目的：综合名称精确匹配和地理位置+名称相似度来判断一个POI是否已存在。
// 参数：name - POI名称；lng/lat - 经纬度
//       existing_names - 已有名称集合（用于精确匹配）
//       existing_locations - 已有位置信息（用于距离+名称相似度匹配）
// 返回：true=重复（应跳过），false=不重复（可以添加）
// 去重规则：
//   1) 名称完全匹配 → 重复
//   2) 距离<200米 且 名称相似度>0.7 → 重复
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
// 目的：从JSON中提取数值字段，并对异常值做安全处理。
// 参数：json - JSON字符串；key - 字段名；default_val - 提取失败时的默认值
// 返回：提取到的double值（限制在[0, 999]范围内，超出返回default_val）
// 用途：主要用于提取评分(rating)和价格(cost)，防止异常数据污染。
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
// 目的：对空字符串、"-"、"[]"、"null"、"None"等无效值统一替换为默认值。
// 参数：s - 原始字符串；default_val - 默认值（默认"-"）
// 返回：有效字符串原样返回；无效字符串返回默认值。
string safe_str(const string& s, const string& default_val = "-") {
    if (s.empty() || s == "-" || s == "[]" || s == "null" || s == "None") return default_val;
    return s;
}

// ====================================================================
// infer_food_category - 根据名称和类型码推断美食类别
// ====================================================================
// 目的：根据美食名称中包含的关键词以及高德POI类型码，自动判断美食类别。
// 参数：name - 美食名称；type_code - 高德POI类型码（如 "050100"）
// 返回：类别名称（汤类/面食/正餐/饮品/甜品/烧烤/凉菜/小吃）
// 判断规则：
//   含"汤""粥" → 汤类      含"面""馍""饼""包""饺""粉" → 面食
//   含"鸡""鸭""鹅" → 正餐   含"茶""奶""汁""咖" → 饮品
//   含"糕""酥""糖""甜" → 甜品   含"串""烤""羊" → 烧烤
//   含"凉""拌" → 凉菜      含"火锅" → 正餐
//   类型码以"050"开头 → 正餐
//   以上都不匹配 → 默认为"小吃"
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
// 目的：从高德POI扩展信息中提取营业时间，若API未返回则根据类别给定默认值。
// 参数：biz_ext - API返回的biz_ext JSON字符串；category - 美食类别
// 返回：营业时间字符串（格式如 "06:00-14:00,17:00-21:00"）
// 默认营业时间按类别设定：
//   汤类/面食: 06:00-14:00,17:00-21:00（早餐+午餐+晚餐）
//   小吃: 08:00-21:00          烧烤: 17:00-02:00（傍晚到凌晨）
//   正餐: 10:00-14:00,17:00-22:00   甜品/饮品: 09:00-21:00
//   凉菜: 10:00-21:00
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
// 目的：根据美食的各项属性自动生成丰富的标签集合，用于前端筛选和展示。
// 参数：
//   name     - 美食名称       category - 美食类别
//   district - 所在区/县      score    - 评分（1~5）
//   price    - 人均价格（元） type_code - 高德POI类型码
//   opentime - 营业时间字符串
// 返回：逗号分隔的标签字符串
//
// 【12个打标维度说明】
//
// 维度1: 口味口感
//   通过名称中的关键词匹配12种口味：麻辣、酸辣、清淡、酱香、蒜蓉、鲜香、
//   五香、甜口、孜然、酥脆、筋道。例如含有"麻""辣"→"麻辣"，含"清蒸"→"清淡"。
//
// 维度2: 食材主料
//   通过名称关键词识别10类主要食材：羊肉、牛肉、猪肉、鸡肉、鸭肉、鱼肉、
//   海鲜、豆制品、面食、内脏。例如含"豆腐"→"豆制品"，含"虾"→"海鲜"。
//
// 维度3: 烹饪方式
//   识别9种烹饪方式：烤、涮、炒、炖、蒸、炸、卤、拌、煮。
//   例如含"烤"→"烤"，含"火锅"→"涮"，含"凉拌"→"拌"。
//
// 维度4: 菜系风味
//   识别菜系归属：鲁菜（默认，菏泽属山东）、川菜、粤菜、湘菜、清真、东北菜。
//   匹配逻辑：名称含"山东""菏泽"等→"鲁菜"；含"川""麻辣"→"川菜"等。
//   未匹配到任何菜系时默认标注"鲁菜"。
//
// 维度5: 用餐时段
//   根据类别和名称判断适合的用餐场景：
//   甜品/饮品 → "下午茶"；火锅/烧烤 → 含"宵夜"
//   汤类/面食 → "早餐"（另加"午餐""晚餐"）；其他 → "午餐""晚餐"
//
// 维度6: 价格档位
//   人均10元以下 → "人均10元以下"；10~30元 → "人均10-30元"
//   30~60元 → "人均30-60元"；60~100元 → "人均60-100元"
//   100元以上 → "人均100元以上"
//
// 维度7: 推荐指数
//   评分≥4.7 → "必吃榜""口碑店"；评分≥4.5 → "人气高""口碑店"
//
// 维度8: 历史/传承
//   名称含"老""传统""正宗"→"老字号"
//   名称含区名 → "地方名吃"
//   含"秘制""秘"→"秘制配方"；含"限定""季节"→"季节限定"
//
// 维度9: 消费场景
//   价格<15 → "一人食""快餐"；15~80 → "堂食""外卖"
//   80+ → "聚餐""宴请"
//
// 维度10: 社交场景
//   火锅/烧烤 → "朋友聚会"；价格>80 → "商务宴请"
//   含"亲子"→"亲子"；价格≤20 → "独自用餐"
//
// 维度11: 区域特色
//   附加区县名的特色标签，如"牡丹区特色""牡丹区美食""牡丹区人气"
//   名称不含"菏泽"时自动添加"菏泽"标签
//
// 维度12: 类别默认标签
//   根据类别附加兜底标签：
//   汤类→"鲜香""原味""清淡""暖胃""滋补"
//   面食→"筋道""快餐""主食"
//   小吃→"现点现做""地方特色"
//   正餐→"菜品丰富""环境好" 等
//
// 【兜底机制】
//   如果口味标签少于3个，随机从{鲜香,原味,五香,酱香,清淡}中补充，
//   使用名称的字符和作为随机种子，确保同一名称每次生成一致。
//
// 【去重】
//   所有标签最终通过set去重，保证每个标签只出现一次。
string generate_food_tags(const string& name, const string& category,
                          const string& district, double score, double price,
                          const string& type_code, const string& opentime) {
    vector<string> tags;

    // ===== 维度1: 口味口感 =====
    // 通过11种口味关键词匹配，如含"麻"→"麻辣"，含"清蒸"→"清淡"
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
    // 通过10类食材关键词匹配，如含"豆腐"→"豆制品"
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
    // 识别9种烹饪方式：烤、涮、炒、炖、蒸、炸、卤、拌、煮
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
    // 通过名称关键词匹配菜系，未匹配时默认归为鲁菜（菏泽所在省份）
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
    // 甜品/饮品→下午茶; 火锅/烧烤→宵夜; 汤类/面食→早餐
    if (category == "甜品" || category == "饮品") tags.push_back("下午茶");
    else if (name.find("火锅") != string::npos || category == "烧烤") tags.push_back("宵夜");
    else if (category == "汤类" || category == "面食") tags.push_back("早餐");
    else { tags.push_back("午餐"); tags.push_back("晚餐"); }

    // ===== 维度9: 消费场景（价格维度综合） =====
    // 价格<15→一人食/快餐; 15~80→堂食/外卖; 80+→聚餐/宴请
    if (price < 15) { tags.push_back("一人食"); tags.push_back("快餐"); }
    else if (price <= 80) { tags.push_back("堂食"); tags.push_back("外卖"); }
    else { tags.push_back("聚餐"); tags.push_back("宴请"); }

    // ===== 维度7: 推荐指数 =====
    // 4.7+→必吃榜; 4.5+→人气高; 均加口碑店
    if (score >= 4.7) { tags.push_back("必吃榜"); tags.push_back("口碑店"); }
    else if (score >= 4.5) { tags.push_back("人气高"); tags.push_back("口碑店"); }

    // ===== 维度8: 历史/传承 =====
    // 含"老""传统""正宗"→老字号; 含区名→地方名吃
    vector<string> old_kws = {"老", "传统", "正宗"};
    for (auto& k : old_kws) if (name.find(k) != string::npos) { tags.push_back("老字号"); break; }
    if (name.find(district) != string::npos) tags.push_back("地方名吃");

    // ===== 维度6: 价格档位 =====
    // 按人均价格分5档标签订阅
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
    // 附加区县标签，如"牡丹区特色"（非"菏泽市"时）
    if (district != "菏泽市" && !district.empty()) {
        string tag = district + "特色";
        bool has_it = false;
        for (auto& t : tags) if (t == tag) { has_it = true; break; }
        if (!has_it) tags.push_back(tag);
    }

    // ===== 维度12: 类别默认标签 =====
    // 根据类别给每个美食补充兜底标签
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
    // 确保每个美食至少有2~3个口味标签
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

    // 用逗号连接所有标签
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
// 目的：将高德API返回的POI类型名映射到系统的景点分类。
// 参数：type_name - 高德POI类型名（如"公园"、"博物馆"）
// 返回：分类名称 自然景观/历史文化/主题公园
// 规则：
//   含"公园""风景""山""湿地""河""湖" → 自然景观
//   含"博物""古迹""纪念""历史""红色""庙" → 历史文化
//   含"游乐""乐园""主题" → 主题公园
//   默认 → 自然景观
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
// 目的：将高德API返回的 "116.397,39.908" 格式坐标解析为两个double。
// 参数：loc_str - 坐标字符串；lng - 输出经度；lat - 输出纬度
// 失败时 lng=0, lat=0
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
// 目的：调用高德地图 /v3/place/text 文本搜索接口，搜索指定关键词的POI。
// 参数：
//   key       - 高德API Key
//   keywords  - 搜索关键词（如"火锅"、"公园"）
//   types     - POI类型码（如"050000"=餐饮，"110000"=旅游景点）
//   city      - 限定搜索城市（如"牡丹区"、"菏泽"）
//   page_size - 每页结果数（默认20）
//   max_pages - 最大翻页数（默认2页）
// 返回：所有POI的原始JSON对象字符串向量。
//
// 处理流程：
//   1) 逐页请求API，每次间隔300ms（遵守高德API限流要求）
//   2) 解析每页返回的 "pois":[...] JSON数组
//   3) 使用字符级状态机正确解析嵌套JSON对象
//   4) 根据POI的id字段去重（同一POI只保留一次）
//   5) 达到最大页数或API返回空结果时停止
//
// 速率限制：每页之间延迟300ms，调用方可能在关键词之间另有延迟。
vector<string> amap_search_poi(const string& key, const string& keywords, const string& types,
                                const string& city, int page_size = 20, int max_pages = 2) {
    printf("  Searching: %s (%d pages max)...\n", keywords.c_str(), max_pages);
    vector<string> all_pois;
    set<string> seen_ids;
    for (int page = 1; page <= max_pages; page++) {
        // 拼接请求URL：关键词、城市、类型码、分页参数
        stringstream url;
        url << "https://restapi.amap.com/v3/place/text?key=" << key
            << "&keywords=" << url_encode(keywords)
            << "&city=" << url_encode(city)
            << "&citylimit=true&types=" << types
            << "&offset=" << page_size << "&page=" << page << "&output=json";
        string resp = http_get(url.str());
        if (resp.empty()) break;

        // 检查API返回状态码
        string status = json_str(resp, "status");
        if (status != "1") break;

        // 定位到 "pois":[{ 数组起始位置
        string pois_str = "\"pois\":[";
        size_t pois_start = resp.find(pois_str);
        if (pois_start == string::npos) break;
        pois_start += pois_str.length();

        // 使用字符级状态机解析JSON对象数组
        // 通过跟踪大括号嵌套深度和字符串边界，正确提取每个POI对象
        int depth = 0;
        bool in_str = false;
        size_t i = pois_start;
        size_t item_start = pois_start;
        while (i < resp.length()) {
            char c = resp[i];
            if (c == '\\') { i += 2; continue; }  // 跳过转义字符
            if (c == '"') in_str = !in_str;         // 跟踪字符串边界
            if (!in_str) {
                if (c == '{') {
                    if (depth == 0) item_start = i;  // 记录对象起始位置
                    depth++;
                } else if (c == '}') {
                    depth--;
                    if (depth == 0) {
                        // 提取完整POI对象，按id去重
                        string poi_obj = resp.substr(item_start, i - item_start + 1);
                        string pid = json_str(poi_obj, "id");
                        if (pid.empty()) pid = json_str(poi_obj, "name");
                        if (!pid.empty() && !seen_ids.count(pid)) {
                            seen_ids.insert(pid);
                            all_pois.push_back(poi_obj);
                        }
                    }
                } else if (c == ']') {
                    break;  // 数组结束
                }
            }
            i++;
        }

        // 页面间延迟（遵守API限流）
        if (page < max_pages) this_thread::sleep_for(chrono::milliseconds(300));
    }
    printf("  Found %zu POIs for '%s'\n", all_pois.size(), keywords.c_str());
    return all_pois;
}

// ====================================================================
// format_food_line - 格式化美食数据行为固定分隔符格式
// ====================================================================
// 目的：按照 food.txt 的标准格式拼接美食数据行。
// 格式：id|名称|经度|纬度|人均价格|评分|类别|地址|营业时间|照片|标签
// 坐标保留1位小数，价格和评分为浮点数。
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
// 目的：按照 spot.txt 的标准格式拼接景点数据行。
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
// cmd_expand - 扩充POI数据命令（替代 expand_data.py）
// ====================================================================
// 目的：通过高德地图API文本搜索接口，按区县和关键词搜索美食和景点POI，
//       去重后追加到 data/food.txt 和 data/spot.txt。
//
// 输入文件：
//   data/food.txt     - 已有美食数据（用于去重和ID分配）
//   data/spot.txt     - 已有景点数据（用于去重和ID分配）
//   config/amap_config.txt - 高德API Key
//
// 输出文件：
//   data/food.txt     - 追加新发现的美食POI
//   data/spot.txt     - 追加新发现的景点POI
//
// API调用：
//   使用高德地图 /v3/place/text 文本搜索接口
//   美食类型码: 050000|051000|052000 (餐饮大类)
//   景点类型码: 110000|110100|110200 (旅游景点)
//
// 算法流程：
//   1) 加载已有数据（名称集合、位置信息、最大ID）
//   2) 遍历菏泽市9个区县（牡丹区、单县、曹县、郓城、巨野、东明、定陶、成武、鄄城）
//   3) 对每个区县，使用16个美食关键词和7个景点关键词分别搜索
//   4) 去重检查：名称精确匹配 OR (距离<200米 AND 名称相似度>0.7) → 视为重复
//   5) 自动推断类别、营业时间、评分、价格、标签等属性
//   6) 美食按评分排序取前25个，景点取前10个
//   7) 追加写入数据文件（带日期标记的分隔注释）
//
// 速率限制：每个关键词搜索后延迟300ms，每个区县处理有额外延迟。
int cmd_expand() {
    printf("============================================================\n");
    printf("  POI Data Expansion (Amap API)\n");
    printf("============================================================\n");

    // 加载API Key配置
    auto config = load_config();
    string key = config.count("AMAP_KEY") ? config["AMAP_KEY"] : config["API_KEY"];
    if (key.empty()) {
        printf("ERROR: API Key not found in config/amap_config.txt\n");
        return 1;
    }
    printf("API Key: %.8s...\n", key.c_str());

    // 加载已有数据用于去重
    auto existing_food_names = load_existing_names(FOOD_TXT);
    auto existing_spot_names = load_existing_names(SPOT_TXT);
    auto existing_food_locs = load_food_locations(FOOD_TXT);
    auto existing_spot_locs = load_spot_locations(SPOT_TXT);
    int max_food_id = get_max_id(FOOD_TXT, 0);
    int max_spot_id = get_max_id(SPOT_TXT, 0);

    printf("Existing foods: %zu (max ID: %d)\n", existing_food_names.size(), max_food_id);
    printf("Existing spots: %zu (max ID: %d)\n", existing_spot_names.size(), max_spot_id);
    printf("\n");

    // 菏泽市9个区县
    vector<string> districts = {"牡丹区","单县","曹县","郓城","巨野","东明","定陶","成武","鄄城"};
    // 美食搜索关键词（覆盖各类菜系和餐饮类型）
    vector<string> food_keywords = {"火锅","烧烤","面馆","川菜","鲁菜","小吃","快餐","甜品",
                                     "饮品","海鲜","自助餐","农家菜","特色菜","汤","糕点","羊肉汤"};
    // 景点搜索关键词
    vector<string> spot_keywords = {"公园","景区","博物馆","文化","寺庙","展览","纪念馆"};

    vector<string> all_new_food_lines;
    vector<string> all_new_spot_lines;
    int food_id_start = max_food_id + 1;
    int spot_id_start = max_spot_id + 1;

    // 逐区县处理
    for (auto& district : districts) {
        printf("[District: %s]\n", district.c_str());

        // ---- 搜索美食POI ----
        printf("  Searching foods (%zu keywords)...\n", food_keywords.size());
        vector<string> all_food_pois;
        set<string> fseen;
        for (auto& kw : food_keywords) {
            auto pois = amap_search_poi(key, kw, "050000|051000|052000", district, 20, 2);
            for (auto& p : pois) {
                string pid = json_str(p, "id");
                if (pid.empty()) pid = json_str(p, "name");
                if (pid.empty()) continue;
                if (!fseen.count(pid)) { fseen.insert(pid); all_food_pois.push_back(p); }
            }
            if (pois.size() > 0) this_thread::sleep_for(chrono::milliseconds(300));
        }
        printf("  Total unique food POIs: %zu\n", all_food_pois.size());

        // 候选美食结构体
        struct FoodCandidate {
            string name, lng_str, lat_str, category, address, opentime, tags, type_code;
            double lng, lat, score, price;
        };
        vector<FoodCandidate> food_candidates;

        // 解析每个POI的详细信息
        for (auto& p : all_food_pois) {
            string loc_str = json_str(p, "location");
            double lng = 0, lat = 0;
            parse_location(loc_str, lng, lat);
            if (lng == 0 && lat == 0) continue;  // 无效坐标跳过

            string name = json_str(p, "name");
            if (is_duplicate(name, lng, lat, existing_food_names, existing_food_locs)) continue;

            string type_code = json_str(p, "typecode");
            string category = infer_food_category(name, type_code);  // 自动推断类别

            // 从扩展信息中提取评分和价格
            string biz_ext_str = json_str(p, "biz_ext");
            if (biz_ext_str.empty()) biz_ext_str = "{}";
            double score = safe_float_from_json(biz_ext_str, "rating", 4.0);
            if (score < 1.0 || score > 5.0) score = 4.0;
            double cost = safe_float_from_json(biz_ext_str, "cost", 30.0);

            string address = json_str(p, "address");
            if (address.empty()) address = "-";
            string addr_clean;
            for (char c : address) if (c != '|') addr_clean += c;  // 去除管道符

            string opentime = extract_opentime(biz_ext_str, category);
            string tags = generate_food_tags(name, category, district, score, cost, type_code, opentime);

            food_candidates.push_back({name, to_string(lng), to_string(lat), category, addr_clean,
                                        opentime, tags, type_code, lng, lat, score, cost});
        }

        // 按评分降序排序，取前25个高分POI
        sort(food_candidates.begin(), food_candidates.end(),
             [](const FoodCandidate& a, const FoodCandidate& b) { return a.score > b.score; });

        int max_food = 25;  // 每区最多25个新美食
        int taken = 0;
        for (auto& fc : food_candidates) {
            if (taken >= max_food) break;
            int new_id = food_id_start++;
            string line = format_food_line(new_id, fc.name, fc.lng, fc.lat,
                                           fc.price, fc.score, fc.category,
                                           fc.address, fc.opentime, "-", fc.tags);
            all_new_food_lines.push_back(line);
            existing_food_names.insert(fc.name);
            existing_food_locs.push_back({fc.name, fc.lng, fc.lat});
            taken++;
        }
        printf("  New foods added: %d\n", taken);

        // ---- 搜索景点POI ----
        printf("  Searching spots (%zu keywords)...\n", spot_keywords.size());
        vector<string> all_spot_pois;
        set<string> sseen;
        for (auto& kw : spot_keywords) {
            auto pois = amap_search_poi(key, kw, "110000|110100|110200", district, 20, 2);
            for (auto& p : pois) {
                string pid = json_str(p, "id");
                if (pid.empty()) pid = json_str(p, "name");
                if (pid.empty()) continue;
                if (!sseen.count(pid)) { sseen.insert(pid); all_spot_pois.push_back(p); }
            }
            if (pois.size() > 0) this_thread::sleep_for(chrono::milliseconds(300));
        }

        // 候选景点结构体
        struct SpotCandidate {
            string name, description, address, type, lng_str, lat_str;
            double lng, lat, score;
        };
        vector<SpotCandidate> spot_candidates;

        for (auto& p : all_spot_pois) {
            string loc_str = json_str(p, "location");
            double lng = 0, lat = 0;
            parse_location(loc_str, lng, lat);
            if (lng == 0 && lat == 0) continue;

            string name = json_str(p, "name");
            if (is_duplicate(name, lng, lat, existing_spot_names, existing_spot_locs)) continue;

            string type_name = json_str(p, "type");
            string biz_ext_str = json_str(p, "biz_ext");
            if (biz_ext_str.empty()) biz_ext_str = "{}";
            double score = safe_float_from_json(biz_ext_str, "rating", 4.0);
            if (score < 1.0 || score > 5.0) score = 4.0;

            string address = json_str(p, "address");
            if (address.empty()) address = "-";
            string addr_clean;
            for (char c : address) if (c != '|') addr_clean += c;

            spot_candidates.push_back({name, name, addr_clean, infer_spot_type(type_name),
                                        to_string(lng), to_string(lat), lng, lat, score});
        }

        // 按评分排序，取前10个
        sort(spot_candidates.begin(), spot_candidates.end(),
             [](const SpotCandidate& a, const SpotCandidate& b) { return a.score > b.score; });

        int max_spot = 10;  // 每区最多10个新景点
        int staken = 0;
        for (auto& sc : spot_candidates) {
            if (staken >= max_spot) break;
            int new_id = spot_id_start++;
            string line = format_spot_line(new_id, sc.name, sc.description, sc.address,
                                           sc.lng, sc.lat, sc.type, "免费", "08:00-18:00",
                                           "1-2小时", "全年", sc.score, "新增");
            all_new_spot_lines.push_back(line);
            existing_spot_names.insert(sc.name);
            existing_spot_locs.push_back({sc.name, sc.lng, sc.lat});
            staken++;
        }
        printf("  New spots added: %d\n", staken);
        printf("\n");
    }

    // ---- 汇总输出 ----
    printf("============================================================\n");
    printf("Total new foods: %zu\n", all_new_food_lines.size());
    printf("Total new spots: %zu\n", all_new_spot_lines.size());

    // 追加到 food.txt（带日期标记）
    if (!all_new_food_lines.empty()) {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&now));
        ofstream f(FOOD_TXT, ios::app);
        f << "\n# ========== C++ Pipeline Extension (" << buf << ") ==========\n";
        for (auto& line : all_new_food_lines) f << line << "\n";
        printf("Appended to %s\n", FOOD_TXT);
    }

    // 追加到 spot.txt（带日期标记）
    if (!all_new_spot_lines.empty()) {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&now));
        ofstream f(SPOT_TXT, ios::app);
        f << "\n# ========== C++ Pipeline Extension (" << buf << ") ==========\n";
        for (auto& line : all_new_spot_lines) f << line << "\n";
        printf("Appended to %s\n", SPOT_TXT);
    }

    printf("\nDone! Next: pipeline roads && pipeline json\n");
    return 0;
}

// ====================================================================
// reverse_geocode - 逆地理编码：根据坐标获取结构化地址
// ====================================================================
// 目的：调用高德地图 /v3/geocode/regeo 逆地理编码接口。
// 参数：key - API Key；lng/lat - 经纬度坐标
// 返回：格式化地址字符串（如"山东省菏泽市牡丹区中华路XX号"）；失败返回空字符串。
// 请求参数：extensions=base 返回基本地址信息（非全量POI信息）。
string reverse_geocode(const string& key, double lng, double lat) {
    stringstream url;
    url << "https://restapi.amap.com/v3/geocode/regeo?key=" << key
        << "&location=" << fixed << setprecision(6) << lng << "," << lat
        << "&extensions=base&output=json";
    string resp = http_get(url.str());
    if (resp.empty()) return "";
    string status = json_str(resp, "status");
    if (status != "1") return "";

    // 从 regeocode.formatted_address 提取格式化地址
    size_t regeo_pos = resp.find("\"regeocode\":{");
    if (regeo_pos == string::npos) return "";
    string address = json_str(resp.substr(regeo_pos), "formatted_address");
    return address;
}

// ====================================================================
// cmd_fill - 填充缺失地址命令（替代 fill_addresses.py 和 fetch_amap_data.py）
// ====================================================================
// 目的：读取 food.txt 和 spot.txt，对地址为空或为"-"/"无法获取"的条目，
//       通过高德逆地理编码接口查询并填充实际地址。
//
// 输入文件：
//   data/food.txt    - 美食数据（含经纬度）
//   data/spot.txt    - 景点数据（含经纬度）
//   config/amap_config.txt - API Key
//
// 输出文件：
//   data/food.txt    - 覆盖更新（地址字段被填充）
//   data/spot.txt    - 覆盖更新（地址字段被填充）
//
// 算法流程：
//   1) 读取全部数据行（保留注释和空行）
//   2) 对每条数据检查地址字段是否有有效值
//   3) 若地址缺失，调用逆地理编码获取地址
//   4) 使用坐标缓存（5位小数精度）避免重复查询同一坐标
//   5) 每次API调用后延迟100ms（遵守限流）
//   6) 全部更新后覆盖写回原文件
//
// 速率限制：每次逆地理编码调用后延迟100ms
int cmd_fill() {
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

    // 通用文件处理Lambda：处理美食或景点数据文件
    // lng_idx/lat_idx/addr_idx: 经纬度和地址字段的列索引
    auto process_file = [&](const string& path, int lng_idx, int lat_idx, int addr_idx, const string& label) {
        printf("\n[%s] Processing %s...\n", label.c_str(), path.c_str());
        vector<string> lines;
        ifstream fin(path);
        string line;
        while (getline(fin, line)) lines.push_back(line);
        fin.close();

        map<string, string> addr_cache;  // 坐标缓存：坐标串→地址
        int updated = 0;
        vector<string> new_lines;

        for (auto& orig_line : lines) {
            string stripped = orig_line;
            stripped.erase(0, stripped.find_first_not_of(" \t\r\n"));
            stripped.erase(stripped.find_last_not_of(" \t\r\n") + 1);

            // 保留注释和空行
            if (stripped.empty() || stripped[0] == '#') {
                new_lines.push_back(orig_line);
                continue;
            }

            auto parts = split(stripped, '|');
            if ((int)parts.size() <= max({lng_idx, lat_idx, addr_idx})) {
                new_lines.push_back(orig_line);
                continue;
            }

            // 已有有效地址 → 跳过
            string cur_addr = parts[addr_idx];
            if (cur_addr != "-" && cur_addr != "" && cur_addr != "无法获取") {
                new_lines.push_back(orig_line);
                continue;
            }

            try {
                double lng = stod(parts[lng_idx]);
                double lat = stod(parts[lat_idx]);

                // 以5位小数精度生成缓存key
                char cache_key[64];
                snprintf(cache_key, sizeof(cache_key), "%.5f,%.5f", lng, lat);
                string ck(cache_key);

                string addr;
                if (addr_cache.count(ck)) {
                    addr = addr_cache[ck];  // 命中缓存
                } else {
                    printf("  Geocoding %s (id=%s)...\n", parts[1].c_str(), parts[0].c_str());
                    addr = reverse_geocode(key, lng, lat);
                    if (addr.empty()) addr = "无法获取";
                    addr_cache[ck] = addr;
                    this_thread::sleep_for(chrono::milliseconds(100));  // API限流
                }

                // 更新地址字段并重建行
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

        // 覆盖写回文件
        if (updated > 0) {
            ofstream fout(path);
            for (auto& l : new_lines) fout << l << "\n";
            printf("  Done: updated %d entries\n", updated);
        }
        return updated;
    };

    // 美食数据：经度索引2, 纬度索引3, 地址索引7
    int food_updates = process_file(FOOD_TXT, 2, 3, 7, "foods");
    // 景点数据：经度索引4, 纬度索引5, 地址索引3
    int spot_updates = process_file(SPOT_TXT, 4, 5, 3, "spots");

    printf("\nTotal updated: foods=%d, spots=%d\n", food_updates, spot_updates);
    printf("Done! Next: pipeline json\n");
    return 0;
}

// ====================================================================
// find_amap_id - 通过名称+坐标查找POI在高德的唯一ID
// ====================================================================
// 目的：用POI名称搜索高德API，在返回结果中找到坐标最接近的那个POI的ID。
// 参数：key - API Key；name - POI名称；lng/lat - 已知坐标
// 返回：匹配的POI ID；未找到或距离超过5000米返回空字符串。
//
// 算法：
//   1) 用名称搜索高德文本搜索接口（限定菏泽市）
//   2) 遍历返回的所有POI，对每个计算与已知坐标的距离
//   3) 选择距离最近且小于5000米的POI
//   4) 返回该POI的id（用于后续详情查询）
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

    // 定位pois数组
    string pois_str = "\"pois\":[";
    size_t pp = resp.find(pois_str);
    if (pp == string::npos) return "";

    string best_id;
    double best_dist = 1e18;
    size_t pos = pp + pois_str.length();

    // 遍历每个POI对象，找距离最近的
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
                            if (d < best_dist && d < 5000) {  // 5000米半径内
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
// 目的：调用高德 /v3/place/detail 地点详情接口，提取POI的照片URL。
// 参数：key - API Key；amap_id - 高德POI唯一ID
// 返回：最多3张照片的URL（分号分隔）；无照片返回"-"。
//
// 处理流程：
//   1) 调用详情接口，解析返回的 "photos":[{...}] 数组
//   2) 从每个photo对象中提取url字段
//   3) 最多取前3张（减少数据传输量）
//   4) 用分号连接所有URL
string fetch_photos_by_id(const string& key, const string& amap_id) {
    if (amap_id.empty()) return "-";
    stringstream url;
    url << "https://restapi.amap.com/v3/place/detail?key=" << key
        << "&id=" << amap_id;
    string resp = http_get(url.str());
    if (resp.empty()) return "-";
    string status = json_str(resp, "status");
    if (status != "1") return "-";

    // 定位photos数组
    string photos_str = "\"photos\":[";
    size_t pp = resp.find(photos_str);
    if (pp == string::npos) return "-";

    vector<string> urls;
    size_t pos = pp + photos_str.length();
    while (pos < resp.length() && urls.size() < 3) {  // 最多3张
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
    // 分号连接多张图片URL
    string result;
    for (size_t i = 0; i < urls.size(); i++) {
        if (i > 0) result += ";";
        result += urls[i];
    }
    return result;
}

// ====================================================================
// cmd_photos - 获取POI照片命令（替代 fetch_photos.py）
// ====================================================================
// 目的：为 food.txt 和 spot.txt 中的每个POI查询高德地图的照片URL。
//
// 输入文件：
//   data/food.txt, data/spot.txt - 含POI名称和坐标的数据文件
//   config/amap_config.txt       - 高德API Key
//
// 输出文件：
//   data/food.txt, data/spot.txt - 覆盖更新（在标签字段前插入照片URL字段）
//
// API调用：
//   第一步：高德 /v3/place/text 文本搜索 → 找到匹配POI的ID
//   第二步：高德 /v3/place/detail 详情查询 → 获取照片URL
//
// 算法流程：
//   1) 逐行读取数据文件
//   2) 对每条数据调用 find_amap_id() 获取POI ID
//   3) 用ID调用 fetch_photos_by_id() 获取照片URL
//   4) 将照片URL插入到倒数第二个字段位置
//   5) 逐条处理后覆盖写回
//
// 速率限制：每条POI处理后有150ms延迟（含两次API调用）
int cmd_photos() {
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

    // 通用文件处理Lambda：处理美食或景点数据
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

                // 先搜索POI ID，再用ID获取照片
                string amap_id = find_amap_id(key, name, lng, lat);
                string photos = fetch_photos_by_id(key, amap_id);
                if (photos != "-") success++;

                // 将照片URL插入到倒数第二个字段位置
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

    // 处理美食（经度索引2, 纬度索引3）和景点（经度索引4, 纬度索引5）
    process_file(FOOD_TXT, 2, 3, "foods", key);
    process_file(SPOT_TXT, 4, 5, "spots", key);

    printf("\nDone!\n");
    return 0;
}

// ====================================================================
// Node - 道路图中的节点结构体
// ====================================================================
// 用途：表示路网图中的一个节点（景点或美食POI），包含ID、名称和坐标。
struct Node {
    int id;
    string name;
    double lng;
    double lat;
};

// ====================================================================
// parse_nodes - 从数据文件中解析节点信息
// ====================================================================
// 目的：从 food.txt 或 spot.txt 中提取所有POI的ID、名称和坐标。
// 参数：path - 数据文件路径
// 返回：map<id, Node>，键为POI ID
//
// 算法：逐行读取，自动识别数值字段中的经纬度对（两个连续的小数），
//       不需要预先知道经纬度字段的确切列索引。
map<int, Node> parse_nodes(const string& path) {
    map<int, Node> data;
    ifstream f(path);
    string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto parts = split(line, '|');
        if (parts.size() < 2) continue;
        try {
            int oid = stoi(parts[0]);
            string name = parts[1];
            double lng = 0, lat = 0;
            bool found = false;

            // 自动检测经纬度字段：找两个连续的数值字段
            for (size_t i = 2; i < parts.size(); i++) {
                if (!found) {
                    // 检查是否为合法数字（允许小数点和前导负号）
                    bool is_num = true;
                    int dot_count = 0;
                    string& p = parts[i];
                    for (size_t j = 0; j < p.size(); j++) {
                        if (p[j] == '.') dot_count++;
                        else if (p[j] == '-' && j == 0) continue;
                        else if (!isdigit(p[j])) { is_num = false; break; }
                    }
                    if (is_num && dot_count <= 1 && !p.empty() && p != "-") {
                        lng = stod(p);
                        // 检查下一个字段是否也是合法数字（纬度）
                        if (i + 1 < parts.size()) {
                            string& p2 = parts[i + 1];
                            bool is_num2 = true;
                            int dot2 = 0;
                            for (size_t j = 0; j < p2.size(); j++) {
                                if (p2[j] == '.') dot2++;
                                else if (p2[j] == '-' && j == 0) continue;
                                else if (!isdigit(p2[j])) { is_num2 = false; break; }
                            }
                            if (is_num2 && dot2 <= 1 && !p2.empty() && p2 != "-") {
                                lat = stod(p2);
                                found = true;
                            }
                        }
                        break;
                    }
                }
            }
            if (found) data[oid] = {oid, name, lng, lat};
        } catch (...) {}
    }
    return data;
}

// ====================================================================
// time_from_distance - 根据距离估算通行时间
// ====================================================================
// 目的：将距离（米）按给定速度换算为时间（分钟）。
// 参数：dist_m - 距离（米）；speed_kmh - 平均速度（km/h，默认40）
// 返回：时间（分钟），最少为1分钟。
// 公式：time = round(dist_m / (speed_kmh * 1000 / 60))
int time_from_distance(double dist_m, double speed_kmh = 40) {
    return max(1, (int)round(dist_m / (speed_kmh * 1000 / 60)));
}

// ====================================================================
// amap_driving - 调用高德驾车路径规划API获取真实距离和时间
// ====================================================================
// 目的：获取两点之间的实际驾车距离和耗时。
// 参数：key - API Key；olng/olat - 起点坐标；dlng/dlat - 终点坐标
// 返回：pair<距离(米), 时间(分钟)>；API失败返回{0,0}。
//
// API：/v3/direction/driving，strategy=0（速度优先）
// 从响应的 route.paths[0] 中提取 distance 和 duration 字段。
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

    // 定位 route 对象
    size_t rp = resp.find("\"route\":{");
    if (rp == string::npos) return {0, 0};
    string route_seg = resp.substr(rp);

    // 定位 paths[0]（第一条路径方案）
    size_t pp = route_seg.find("\"paths\":[");
    if (pp == string::npos) return {0, 0};
    size_t ob = route_seg.find("{", pp);
    if (ob == string::npos) return {0, 0};

    // 解析路径对象，提取distance(米)和duration(秒)
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
                    return {(int)dist, (int)(dur / 60)};  // duration秒→分钟
                }
            }
        }
        i++;
    }
    return {0, 0};
}

// ====================================================================
// cmd_roads - 重新计算道路连接命令（替代 recalc_roads.py）
// ====================================================================
// 目的：基于POI坐标计算所有节点间的道路连接（距离和时间），生成路网数据。
//
// 输入文件：
//   data/food.txt    - 美食数据（含坐标）
//   data/spot.txt    - 景点数据（含坐标）
//   config/amap_config.txt - API Key（可选，用于驾车路径API）
//
// 输出文件：
//   data/road.txt    - 覆盖生成的路网连接数据
//
// 算法流程（分层构建道路网络）：
//
// 第一层：预设的热门景点间连接
//   使用预定义的景点对（spot_connections列表，共17对）建立连接。
//   优先使用高德驾车API获取真实距离/时间；API不可用时使用Haversine直线距离。
//
// 第二层：各区县美食聚集点与枢纽的连接
//   county_hub_ranges定义了8个县的枢纽景点ID与美食ID范围的映射。
//   对每个县，计算枢纽点与所有美食的Haversine距离，取最近的3个建立连接。
//
// 第三层：牡丹区景点与美食的邻近连接
//   牡丹区景点(13个)与牡丹区美食范围建立连接。
//   搜索半径5000米内，取最近的5个建立连接。
//
// 第四层：牡丹区美食集群内部连接
//   将牡丹区美食分为4个集群，集群内距离<3000米的美食互相连接。
//   另有ID 164-188范围内的美食，距离<5000米的互相连接。
//
// 第五层：跨区枢纽连接
//   9个区县枢纽节点（1,121,129,136,142,148,153,157,161）之间全连接。
//   距离使用Haversine×1.3系数（模拟非直线道路）。
//
// 第六层：各景点与总枢纽(1)的连接
//   所有景点(ID 21-110)与枢纽1之间建立连接（Haversine×1.3系数）。
//
// 第七层：各区县内部节点连接
//   8个县的景点之间和美食之间，距离阈值内互相连接。
//
// 去重与排序：
//   对所有边按(min(a,b), max(a,b))去重，保留最短距离。
//   输出分5个section：牡丹区景点间、景点↔美食、牡丹区美食间、跨区、各县内部。
//
// 距离计算策略：
//   - 有高德API Key时，支持点对点驾车距离（仅预设连接使用，避免大量API调用）
//   - 无API Key或API调用失败时，使用Haversine直线距离
//   - 跨区道路乘以1.3系数模拟实际道路的绕行
//   - 时间由距离÷40km/h估算
int cmd_roads() {
    printf("============================================================\n");
    printf("  Recalculate Road Connections\n");
    printf("============================================================\n");

    auto config = load_config();
    string amap_key = config.count("AMAP_KEY") ? config["AMAP_KEY"] : "";
    if (!amap_key.empty()) printf("Using Amap driving API\n");
    else printf("Using Haversine distance only\n");

    // 解析所有节点（景点和美食）
    auto spots = parse_nodes(SPOT_TXT);
    auto foods = parse_nodes(FOOD_TXT);

    printf("Loaded spots: %zu\n", spots.size());
    printf("Loaded foods: %zu\n", foods.size());

    // 合并所有节点
    map<int, Node> all_nodes = spots;
    for (auto& p : foods) all_nodes[p.first] = p.second;

    // 边的向量：(起点ID, 终点ID, 距离米, 时间分钟)
    vector<tuple<int,int,int,int>> edges;

    // ===== 第一层：牡丹区热门景点间的预设连接 =====
    vector<int> mudan_spots = {1,2,8,9,10,11,12,13,14,15,16,20,18};
    vector<pair<int,int>> spot_connections = {
        {1,2},{1,13},{1,14},{2,13},{8,9},{8,12},{8,16},{9,10},{9,11},{9,16},
        {10,15},{11,20},{11,12},{12,16},{19,15},{3,17},{5,19}
    };

    for (auto& sc : spot_connections) {
        int a = sc.first, b = sc.second;
        if (!all_nodes.count(a) || !all_nodes.count(b)) continue;
        auto& na = all_nodes[a], &nb = all_nodes[b];
        // 优先使用高德驾车API获取实际距离
        if (!amap_key.empty()) {
            auto [dist, dur] = amap_driving(amap_key, na.lng, na.lat, nb.lng, nb.lat);
            if (dist > 0) {
                edges.push_back({a, b, dist, dur});
                edges.push_back({b, a, dist, dur});
                this_thread::sleep_for(chrono::milliseconds(500));
                continue;
            }
        }
        // 降级到Haversine直线距离
        double dist = haversine(na.lng, na.lat, nb.lng, nb.lat);
        int t = time_from_distance(dist);
        edges.push_back({a, b, (int)dist, t});
        edges.push_back({b, a, (int)dist, t});
    }

    // ===== 第二层：各区县美食与枢纽连接 =====
    // 每个县有一个枢纽景点(key)和一组美食ID范围(value)
    map<int, vector<int>> county_hub_ranges = {
        {121, {122,123,124,125,126,127,128,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213}},
        {129, {130,131,132,133,134,135,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238}},
        {136, {137,138,139,140,141,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263}},
        {142, {143,144,145,146,147,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,313}},
        {148, {149,150,151,152,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288}},
        {153, {154,155,156,314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336,337,338}},
        {157, {158,159,160,339,340,341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363}},
        {161, {162,163,364,365,366,367,368,369,370,371,372,373,374,375,376,377,378,379,380,381,382,383,384,385,386,387,388}},
    };

    for (auto& ch : county_hub_ranges) {
        int hub_id = ch.first;
        if (!all_nodes.count(hub_id)) continue;
        double hlng = all_nodes[hub_id].lng, hlat = all_nodes[hub_id].lat;

        // 计算枢纽到所有美食的距离，取最近的3个
        vector<pair<int,double>> candidates;
        for (int fid : ch.second) {
            if (!all_nodes.count(fid)) continue;
            double dist = haversine(hlng, hlat, all_nodes[fid].lng, all_nodes[fid].lat);
            candidates.push_back({fid, dist});
        }
        sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) { return a.second < b.second; });
        int taken = 0;
        for (auto& c : candidates) {
            if (taken >= 3) break;
            double dist = c.second;
            int t = time_from_distance(dist);
            edges.push_back({hub_id, c.first, (int)dist, t});
            edges.push_back({c.first, hub_id, (int)dist, t});
            taken++;
        }
    }

    // ===== 第三层：牡丹区景点与美食的邻近连接 =====
    // 牡丹区美食ID范围：101-120 和 164-188
    vector<int> mudan_food_ids;
    for (int i = 101; i <= 120; i++) mudan_food_ids.push_back(i);
    for (int i = 164; i <= 188; i++) mudan_food_ids.push_back(i);

    for (int sid : mudan_spots) {
        if (!all_nodes.count(sid)) continue;
        double slng = all_nodes[sid].lng, slat = all_nodes[sid].lat;
        vector<pair<int,double>> nearby;
        for (int fid : mudan_food_ids) {
            if (!all_nodes.count(fid)) continue;
            double dist = haversine(slng, slat, all_nodes[fid].lng, all_nodes[fid].lat);
            if (dist < 5000) nearby.push_back({fid, dist});  // 5000米半径
        }
        sort(nearby.begin(), nearby.end(), [](auto& a, auto& b) { return a.second < b.second; });
        int taken = 0;
        for (auto& n : nearby) {
            if (taken >= 5) break;  // 每个景点最多连接5个最近美食
            int t = time_from_distance(n.second);
            edges.push_back({sid, n.first, (int)n.second, t});
            edges.push_back({n.first, sid, (int)n.second, t});
            taken++;
        }
    }

    // ===== 第四层：牡丹区美食集群内部连接 =====
    // 将牡丹区美食分为4个集群，集群内距离<3000米的互相连接
    vector<vector<int>> food_clusters = {
        {101,102,108,111,112,120},
        {103,105,106,109,110,116,117,118},
        {104,107,113,114,119},
        {115}
    };
    for (auto& cluster : food_clusters) {
        for (size_t i = 0; i < cluster.size(); i++) {
            for (size_t j = i+1; j < cluster.size(); j++) {
                int a = cluster[i], b = cluster[j];
                if (all_nodes.count(a) && all_nodes.count(b)) {
                    double dist = haversine(all_nodes[a].lng, all_nodes[a].lat,
                                           all_nodes[b].lng, all_nodes[b].lat);
                    if (dist < 3000) {
                        int t = time_from_distance(dist);
                        edges.push_back({a, b, (int)dist, t});
                        edges.push_back({b, a, (int)dist, t});
                    }
                }
            }
        }
    }

    // 扩展美食范围 164-188 之间的连接（距离<5000米）
    for (int a = 164; a <= 188; a++) {
        for (int b = a + 1; b <= 188; b++) {
            if (all_nodes.count(a) && all_nodes.count(b)) {
                double dist = haversine(all_nodes[a].lng, all_nodes[a].lat,
                                       all_nodes[b].lng, all_nodes[b].lat);
                if (dist < 5000) {
                    int t = time_from_distance(dist);
                    edges.push_back({a, b, (int)dist, t});
                    edges.push_back({b, a, (int)dist, t});
                }
            }
        }
    }

    // ===== 第五层：跨区枢纽全连接 =====
    // 9个区县枢纽节点之间全连接（Haversine×1.3系数）
    vector<int> hub_ids = {1,121,129,136,142,148,153,157,161};
    for (size_t i = 0; i < hub_ids.size(); i++) {
        for (size_t j = i+1; j < hub_ids.size(); j++) {
            int a = hub_ids[i], b = hub_ids[j];
            if (all_nodes.count(a) && all_nodes.count(b)) {
                double dist = haversine(all_nodes[a].lng, all_nodes[a].lat,
                                       all_nodes[b].lng, all_nodes[b].lat);
                int road_dist = (int)(dist * 1.3);  // 跨区道路系数1.3
                int t = max(1, (int)round(road_dist / (60.0 * 1000 / 60)));
                edges.push_back({a, b, road_dist, t});
                edges.push_back({b, a, road_dist, t});
            }
        }
    }

    // ===== 第六层：各景点与总枢纽(1)连接 =====
    // 所有景点(ID 21-110)到总枢纽1的星形连接
    for (int sid = 21; sid <= 110; sid++) {
        if (all_nodes.count(sid) && all_nodes.count(1) && sid != 1) {
            double dist = haversine(all_nodes[1].lng, all_nodes[1].lat,
                                   all_nodes[sid].lng, all_nodes[sid].lat);
            int road_dist = (int)(dist * 1.3);  // 跨区系数
            int t = max(1, (int)round(road_dist / (60.0 * 1000 / 60)));
            edges.push_back({1, sid, road_dist, t});
            edges.push_back({sid, 1, road_dist, t});
        }
    }

    // ===== 第七层：各区县内部节点间连接 =====
    // 8个县的景点范围（距离<5000米内互相连接）
    vector<pair<int,int>> county_ranges = {
        {121,128},{129,135},{136,141},{142,147},{148,152},{153,156},{157,160},{161,163}
    };
    for (auto& cr : county_ranges) {
        for (int a = cr.first; a <= cr.second; a++) {
            for (int b = a + 1; b <= cr.second; b++) {
                if (all_nodes.count(a) && all_nodes.count(b)) {
                    double dist = haversine(all_nodes[a].lng, all_nodes[a].lat,
                                           all_nodes[b].lng, all_nodes[b].lat);
                    if (dist < 5000) {
                        int t = time_from_distance(dist);
                        edges.push_back({a, b, (int)dist, t});
                        edges.push_back({b, a, (int)dist, t});
                    }
                }
            }
        }
    }

    // 8个县的新增美食范围（距离<10000米内互相连接，更大的阈值）
    vector<pair<int,int>> new_county_ranges = {
        {189,213},{214,238},{239,263},{264,288},{289,313},{314,338},{339,363},{364,388}
    };
    for (auto& cr : new_county_ranges) {
        for (int a = cr.first; a <= cr.second; a++) {
            for (int b = a + 1; b <= cr.second; b++) {
                if (all_nodes.count(a) && all_nodes.count(b)) {
                    double dist = haversine(all_nodes[a].lng, all_nodes[a].lat,
                                           all_nodes[b].lng, all_nodes[b].lat);
                    if (dist < 10000) {
                        int t = time_from_distance(dist);
                        edges.push_back({a, b, (int)dist, t});
                        edges.push_back({b, a, (int)dist, t});
                    }
                }
            }
        }
    }

    // ===== 去重：对相同节点对的边保留最短距离 =====
    map<pair<int,int>,pair<int,int>> edge_map;
    for (auto& e : edges) {
        int a = get<0>(e), b = get<1>(e), dist = get<2>(e), t = get<3>(e);
        auto key = make_pair(min(a,b), max(a,b));
        if (!edge_map.count(key) || dist < edge_map[key].first) {
            edge_map[key] = {dist, t};
        }
    }

    // ===== 写入 road.txt =====
    ofstream f(ROAD_TXT);
    f << "# 菏泽城市道路连接数据（基于高德地图真实坐标计算）\n";
    f << "# 格式: 起点ID|终点ID|距离(米)|预计时间(分钟)\n";
    f << "# 节点类型: 1-110=景点, 101-388=美食\n";
    f << "# 距离使用Haversine公式计算，跨区道路×1.3系数\n";
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&now));
    f << "# 更新日期: " << buf << "\n";
    f << "#\n";

    // 分组过滤器：按连接类型分5个section导出
    // group 0: 景点间   1: 景点↔美食   2: 牡丹区美食间   3: 跨区   4: 各县内部
    auto filter_fn = [](int a, int b, int d, int t, int group) {
        switch (group) {
            case 0: return a <= 110 && b <= 110;
            case 1: return (a <= 110 && b >= 101) || (b <= 110 && a >= 101);
            case 2: return a >= 101 && a <= 188 && b >= 101 && b <= 188;
            case 3: return (a <= 110 && b >= 189) || (b <= 110 && a >= 189);
            case 4: return a >= 189 && b >= 189;
        }
        return false;
    };

    string section_names[] = {
        "牡丹区景点间连接", "景点↔美食连接", "牡丹区美食间连接",
        "跨区连接", "各县内部连接"
    };

    for (int g = 0; g < 5; g++) {
        vector<tuple<int,int,int,int>> section;
        for (auto& ekv : edge_map) {
            int a = ekv.first.first, b = ekv.first.second;
            int dist = ekv.second.first, t = ekv.second.second;
            if (filter_fn(a, b, dist, t, g)) section.push_back({a, b, dist, t});
        }
        if (!section.empty()) {
            sort(section.begin(), section.end());
            f << "\n# ========== " << section_names[g] << " ==========\n";
            for (auto& s : section) {
                f << get<0>(s) << "|" << get<1>(s) << "|" << get<2>(s) << "|" << get<3>(s) << "\n";
            }
        }
    }

    printf("\nGenerated %zu road connections\n", edge_map.size());
    printf("Written to %s\n", ROAD_TXT);
    return 0;
}

// ====================================================================
// cmd_json - 生成Web前端JSON文件命令（替代 gen_web_json.py）
// ====================================================================
// 目的：将 food.txt 和 spot.txt 中的管道分隔数据转换为前端可用的JSON格式。
//
// 输入文件：
//   data/food.txt    - 美食数据（管道符分隔）
//   data/spot.txt    - 景点数据（管道符分隔）
//
// 输出文件：
//   web/data/food.json   - 美食JSON数组
//   web/data/spot.json   - 景点JSON数组
//   web/data/route.json  - 推荐路线JSON
//
// 处理流程：
//   1) 读取 food.txt，按 | 分隔各字段
//   2) 智能识别地址、营业时间、照片、标签等字段位置
//      由于历史数据格式可能存在差异，通过关键词启发式判断字段含义
//   3) 组装为JSON对象并写入JSON数组
//   4) 同样处理 spot.txt
//   5) 生成一条预设的"菏泽美食文化一日游"路线JSON
//
// 字段识别策略（美食）：
//   利用启发式规则判断地址（含"路""街""区""市""县"等）、
//   营业时间（含":"和"-"或"全天"）、照片（含"http"或";"）、
//   标签（最后一个字段）。标签字段进一步按逗号拆分为数组。
//
// 字段识别策略（景点）：
//   从第12个字段开始，含"http"或";"的为照片字段，其余为标签字段。
int cmd_json() {
    printf("============================================================\n");
    printf("  Generate Web JSON from TXT\n");
    printf("============================================================\n");

    // 确保 web/data/ 目录存在
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

            // 启发式识别各字段：地址、营业时间、照片、标签
            string address = "-";
            string opentime = "-";
            string tags_part = "";

            int part8_idx = 7;
            if ((int)parts.size() > part8_idx) {
                string p8 = parts[part8_idx];
                // 判断第8个字段是否为地址（含地理特征词）
                bool is_addr = (p8.find("区") != string::npos || p8.find("路") != string::npos ||
                               p8.find("街") != string::npos || p8.find("市") != string::npos ||
                               p8.find("县") != string::npos || p8 == "-" || p8 == "无法获取");
                if (is_addr) {
                    address = p8;
                    if ((int)parts.size() > part8_idx + 1) {
                        string p9 = parts[part8_idx + 1];
                        // 判断第9个字段是否为营业时间（含":"和"-"）
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

            // 标签始终在最后一个字段
            tags_part = parts.back();

            // 照片在标签前一个字段（倒数第二个）
            string photos_str = parts.size() >= 2 ? parts[parts.size() - 2] : "-";
            bool photos_is_tag = true;
            if (!photos_str.empty() && (photos_str.find("http") != string::npos || photos_str.find(";") != string::npos || photos_str == "-"))
                photos_is_tag = false;
            if (photos_is_tag) photos_str = "-";

            // 解析照片URL（分号分隔多个URL）
            vector<string> photos;
            if (photos_str != "-") {
                auto purls = split(photos_str, ';');
                for (auto& pu : purls) if (!pu.empty() && pu != "-") photos.push_back(pu);
            }
            if (photos.empty()) photos.push_back("-");

            // 解析标签（逗号分隔）
            vector<string> tags;
            auto tlist = split(tags_part, ',');
            for (auto& t : tlist) if (!t.empty()) tags.push_back(t);

            // 组装JSON对象
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

    // 输出为JSON数组
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

            // 从第12个字段开始解析照片和标签
            string photos_str = "-";
            vector<string> tag_fields;
            for (size_t i = 12; i < parts.size(); i++) {
                if (photos_str == "-" && (parts[i].find(";") != string::npos || parts[i].find("http") != string::npos))
                    photos_str = parts[i];  // 含";"或"http"的为照片字段
                else
                    tag_fields.push_back(parts[i]);
            }

            // 解析照片URL
            vector<string> photos;
            if (photos_str != "-") {
                auto purls = split(photos_str, ';');
                for (auto& pu : purls) if (!pu.empty() && pu != "-") photos.push_back(pu);
            }

            // 组装JSON对象
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
    // 预设路线：曹州牡丹园 → 菏泽烧牛肉 → 胡辣汤 → 羊肉汤 → 天香公园
    string route_json = "{\n";
    route_json += "  \"name\":\" 菏泽美食文化一日游路线\",\n";
    route_json += "  \"found\":true,\n";
    route_json += "  \"path\":[[115.497379,35.279915],[115.500907,35.241715],[115.499334,35.246949],[115.437718,35.251241],[115.476401,35.235636]],\n";
    route_json += "  \"waypoints\":[\n";
    route_json += "    {\"type\":\"spot\",\"lng\":115.497379,\"lat\":35.279915,\"name\":\" 曹州牡丹园\"},\n";
    route_json += "    {\"type\":\"food\",\"lng\":115.500907,\"lat\":35.241715,\"name\":\" 菏泽烧牛肉\"},\n";
    route_json += "    {\"type\":\"food\",\"lng\":115.499334,\"lat\":35.246949,\"name\":\" 胡辣汤\"},\n";
    route_json += "    {\"type\":\"food\",\"lng\":115.437718,\"lat\":35.251241,\"name\":\" 羊肉汤\"},\n";
    route_json += "    {\"type\":\"spot\",\"lng\":115.476401,\"lat\":35.235636,\"name\":\" 天香公园\"}\n";
    route_json += "  ],\n";

    // 计算路线总距离（Haversine累加）
    double total_dist = 0;
    total_dist += haversine(115.497379,35.279915,115.500907,35.241715);
    total_dist += haversine(115.500907,35.241715,115.499334,35.246949);
    total_dist += haversine(115.499334,35.246949,115.437718,35.251241);
    total_dist += haversine(115.437718,35.251241,115.476401,35.235636);

    stringstream rt;
    rt << ",\"totalDistance\":" << fixed << setprecision(1) << total_dist;
    rt << ",\"totalTime\":" << fixed << setprecision(1) << (total_dist / (40.0 * 1000 / 60));  // 40km/h估算时间
    route_json += rt.str();
    route_json += "\n}\n";
    write_json_file(WEB_DATA_DIR + "route.json", route_json);
    printf("Generated route.json\n");
    printf("Route distance: %.1f km\n", total_dist / 1000);

    printf("\nDone!\n");
    return 0;
}

// ====================================================================
// main - 命令行入口，负责子命令分发
// ====================================================================
// 用法：pipeline <command>
// 支持的命令：
//   expand  - 通过高德API扩充POI数据
//   fill    - 逆地理编码填充缺失地址
//   photos  - 获取POI照片URL
//   roads   - 重新计算道路连接
//   json    - 生成Web前端JSON文件
//   all     - 按顺序执行以上全部命令（一整条流水线）
//
// 返回值：0=成功，1=失败（参数错误或某个步骤失败）
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
        // 全流水线：按依赖顺序执行所有步骤
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
