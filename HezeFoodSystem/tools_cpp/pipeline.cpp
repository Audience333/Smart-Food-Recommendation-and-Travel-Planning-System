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

using namespace std;

const string DATA_DIR = "data/";
const string CONFIG_PATH = "config/amap_config.txt";
const string FOOD_TXT = "data/food.txt";
const string SPOT_TXT = "data/spot.txt";
const string ROAD_TXT = "data/road.txt";
const string WEB_DATA_DIR = "web/data/";

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

double haversine(double lng1, double lat1, double lng2, double lat2) {
    const double R = 6371000;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlng = (lng2 - lng1) * M_PI / 180.0;
    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlng/2) * sin(dlng/2);
    return R * 2 * atan2(sqrt(a), sqrt(1 - a));
}

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

struct PoiLoc {
    string name;
    double lng;
    double lat;
};

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

bool is_duplicate(const string& name, double lng, double lat,
                  set<string>& existing_names, vector<PoiLoc>& existing_locations) {
    if (existing_names.count(name)) return true;
    for (auto& el : existing_locations) {
        double dist = haversine(lng, lat, el.lng, el.lat);
        if (dist < 200 && name_similarity(name, el.name) > 0.7) return true;
    }
    return false;
}

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

string safe_str(const string& s, const string& default_val = "-") {
    if (s.empty() || s == "-" || s == "[]" || s == "null" || s == "None") return default_val;
    return s;
}

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

string generate_food_tags(const string& name, const string& category,
                          const string& district, double score, double price,
                          const string& type_code, const string& opentime) {
    vector<string> tags;

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

    if (category == "甜品" || category == "饮品") tags.push_back("下午茶");
    else if (name.find("火锅") != string::npos || category == "烧烤") tags.push_back("宵夜");
    else if (category == "汤类" || category == "面食") tags.push_back("早餐");
    else { tags.push_back("午餐"); tags.push_back("晚餐"); }

    if (price < 15) { tags.push_back("一人食"); tags.push_back("快餐"); }
    else if (price <= 80) { tags.push_back("堂食"); tags.push_back("外卖"); }
    else { tags.push_back("聚餐"); tags.push_back("宴请"); }

    if (score >= 4.7) { tags.push_back("必吃榜"); tags.push_back("口碑店"); }
    else if (score >= 4.5) { tags.push_back("人气高"); tags.push_back("口碑店"); }

    vector<string> old_kws = {"老", "传统", "正宗"};
    for (auto& k : old_kws) if (name.find(k) != string::npos) { tags.push_back("老字号"); break; }
    if (name.find(district) != string::npos) tags.push_back("地方名吃");

    if (price <= 10) tags.push_back("人均10元以下");
    else if (price <= 30) tags.push_back("人均10-30元");
    else if (price <= 60) tags.push_back("人均30-60元");
    else if (price <= 100) tags.push_back("人均60-100元");
    else tags.push_back("人均100元以上");

    if (!opentime.empty()) {
        if (opentime.find("24小时") != string::npos || opentime.find("全天") != string::npos)
            tags.push_back("24小时营业");
    }

    if (name.find("火锅") != string::npos || category == "烧烤") tags.push_back("朋友聚会");
    if (price > 80) tags.push_back("商务宴请");
    if (name.find("亲子") != string::npos) tags.push_back("亲子");
    if (price <= 20) tags.push_back("独自用餐");

    if (name.find("秘制") != string::npos || name.find("秘") != string::npos) tags.push_back("秘制配方");
    vector<string> limit_kws = {"限定", "季节"};
    for (auto& k : limit_kws) if (name.find(k) != string::npos) { tags.push_back("季节限定"); break; }

    if (district != "菏泽市" && !district.empty()) {
        string tag = district + "特色";
        bool has_it = false;
        for (auto& t : tags) if (t == tag) { has_it = true; break; }
        if (!has_it) tags.push_back(tag);
    }

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

    if (price <= 10) { tags.push_back("平价实惠"); tags.push_back("快餐"); }
    else if (price <= 30) { tags.push_back("家常"); tags.push_back("堂食"); }
    else if (price <= 60) { tags.push_back("品质餐厅"); tags.push_back("朋友聚会"); }
    else if (price <= 100) { tags.push_back("环境好"); tags.push_back("商务宴请"); }
    else { tags.push_back("高端"); tags.push_back("宴请"); tags.push_back("包厢"); }

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

    if (!district.empty()) {
        tags.push_back(district + "美食");
        tags.push_back(district + "人气");
    }

    if (name.find("菏泽") == string::npos) tags.push_back("菏泽");
    tags.push_back("美食推荐");
    tags.push_back("人气美食");

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

void parse_location(const string& loc_str, double& lng, double& lat) {
    auto parts = split(loc_str, ',');
    if (parts.size() == 2) {
        try { lng = stod(parts[0]); lat = stod(parts[1]); return; }
        catch (...) {}
    }
    lng = 0; lat = 0;
}

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

int cmd_expand() {
    printf("============================================================\n");
    printf("  POI Data Expansion (Amap API)\n");
    printf("============================================================\n");

    auto config = load_config();
    string key = config.count("AMAP_KEY") ? config["AMAP_KEY"] : config["API_KEY"];
    if (key.empty()) {
        printf("ERROR: API Key not found in config/amap_config.txt\n");
        return 1;
    }
    printf("API Key: %.8s...\n", key.c_str());

    auto existing_food_names = load_existing_names(FOOD_TXT);
    auto existing_spot_names = load_existing_names(SPOT_TXT);
    auto existing_food_locs = load_food_locations(FOOD_TXT);
    auto existing_spot_locs = load_spot_locations(SPOT_TXT);
    int max_food_id = get_max_id(FOOD_TXT, 0);
    int max_spot_id = get_max_id(SPOT_TXT, 0);

    printf("Existing foods: %zu (max ID: %d)\n", existing_food_names.size(), max_food_id);
    printf("Existing spots: %zu (max ID: %d)\n", existing_spot_names.size(), max_spot_id);
    printf("\n");

    vector<string> districts = {"牡丹区","单县","曹县","郓城","巨野","东明","定陶","成武","鄄城"};
    vector<string> food_keywords = {"火锅","烧烤","面馆","川菜","鲁菜","小吃","快餐","甜品",
                                     "饮品","海鲜","自助餐","农家菜","特色菜","汤","糕点","羊肉汤"};
    vector<string> spot_keywords = {"公园","景区","博物馆","文化","寺庙","展览","纪念馆"};

    vector<string> all_new_food_lines;
    vector<string> all_new_spot_lines;
    int food_id_start = max_food_id + 1;
    int spot_id_start = max_spot_id + 1;

    for (auto& district : districts) {
        printf("[District: %s]\n", district.c_str());

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

        struct FoodCandidate {
            string name, lng_str, lat_str, category, address, opentime, tags, type_code;
            double lng, lat, score, price;
        };
        vector<FoodCandidate> food_candidates;

        for (auto& p : all_food_pois) {
            string loc_str = json_str(p, "location");
            double lng = 0, lat = 0;
            parse_location(loc_str, lng, lat);
            if (lng == 0 && lat == 0) continue;

            string name = json_str(p, "name");
            if (is_duplicate(name, lng, lat, existing_food_names, existing_food_locs)) continue;

            string type_code = json_str(p, "typecode");
            string category = infer_food_category(name, type_code);

            string biz_ext_str = json_str(p, "biz_ext");
            if (biz_ext_str.empty()) biz_ext_str = "{}";
            double score = safe_float_from_json(biz_ext_str, "rating", 4.0);
            if (score < 1.0 || score > 5.0) score = 4.0;
            double cost = safe_float_from_json(biz_ext_str, "cost", 30.0);

            string address = json_str(p, "address");
            if (address.empty()) address = "-";
            string addr_clean;
            for (char c : address) if (c != '|') addr_clean += c;

            string opentime = extract_opentime(biz_ext_str, category);
            string tags = generate_food_tags(name, category, district, score, cost, type_code, opentime);

            food_candidates.push_back({name, to_string(lng), to_string(lat), category, addr_clean,
                                       opentime, tags, type_code, lng, lat, score, cost});
        }

        sort(food_candidates.begin(), food_candidates.end(),
             [](const FoodCandidate& a, const FoodCandidate& b) { return a.score > b.score; });

        int max_food = 25;
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

        sort(spot_candidates.begin(), spot_candidates.end(),
             [](const SpotCandidate& a, const SpotCandidate& b) { return a.score > b.score; });

        int max_spot = 10;
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

    printf("============================================================\n");
    printf("Total new foods: %zu\n", all_new_food_lines.size());
    printf("Total new spots: %zu\n", all_new_spot_lines.size());

    if (!all_new_food_lines.empty()) {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&now));
        ofstream f(FOOD_TXT, ios::app);
        f << "\n# ========== C++ Pipeline Extension (" << buf << ") ==========\n";
        for (auto& line : all_new_food_lines) f << line << "\n";
        printf("Appended to %s\n", FOOD_TXT);
    }

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

struct Node {
    int id;
    string name;
    double lng;
    double lat;
};

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
            for (size_t i = 2; i < parts.size(); i++) {
                if (!found) {
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

int time_from_distance(double dist_m, double speed_kmh = 40) {
    return max(1, (int)round(dist_m / (speed_kmh * 1000 / 60)));
}

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

int cmd_roads() {
    printf("============================================================\n");
    printf("  Recalculate Road Connections\n");
    printf("============================================================\n");

    auto config = load_config();
    string amap_key = config.count("AMAP_KEY") ? config["AMAP_KEY"] : "";
    if (!amap_key.empty()) printf("Using Amap driving API\n");
    else printf("Using Haversine distance only\n");

    auto spots = parse_nodes(SPOT_TXT);
    auto foods = parse_nodes(FOOD_TXT);

    printf("Loaded spots: %zu\n", spots.size());
    printf("Loaded foods: %zu\n", foods.size());

    map<int, Node> all_nodes = spots;
    for (auto& p : foods) all_nodes[p.first] = p.second;

    vector<tuple<int,int,int,int>> edges;

    vector<int> mudan_spots = {1,2,8,9,10,11,12,13,14,15,16,20,18};
    vector<pair<int,int>> spot_connections = {
        {1,2},{1,13},{1,14},{2,13},{8,9},{8,12},{8,16},{9,10},{9,11},{9,16},
        {10,15},{11,20},{11,12},{12,16},{19,15},{3,17},{5,19}
    };

    for (auto& sc : spot_connections) {
        int a = sc.first, b = sc.second;
        if (!all_nodes.count(a) || !all_nodes.count(b)) continue;
        auto& na = all_nodes[a], &nb = all_nodes[b];
        if (!amap_key.empty()) {
            auto [dist, dur] = amap_driving(amap_key, na.lng, na.lat, nb.lng, nb.lat);
            if (dist > 0) {
                edges.push_back({a, b, dist, dur});
                edges.push_back({b, a, dist, dur});
                this_thread::sleep_for(chrono::milliseconds(500));
                continue;
            }
        }
        double dist = haversine(na.lng, na.lat, nb.lng, nb.lat);
        int t = time_from_distance(dist);
        edges.push_back({a, b, (int)dist, t});
        edges.push_back({b, a, (int)dist, t});
    }

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
            if (dist < 5000) nearby.push_back({fid, dist});
        }
        sort(nearby.begin(), nearby.end(), [](auto& a, auto& b) { return a.second < b.second; });
        int taken = 0;
        for (auto& n : nearby) {
            if (taken >= 5) break;
            int t = time_from_distance(n.second);
            edges.push_back({sid, n.first, (int)n.second, t});
            edges.push_back({n.first, sid, (int)n.second, t});
            taken++;
        }
    }

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

    vector<int> hub_ids = {1,121,129,136,142,148,153,157,161};
    for (size_t i = 0; i < hub_ids.size(); i++) {
        for (size_t j = i+1; j < hub_ids.size(); j++) {
            int a = hub_ids[i], b = hub_ids[j];
            if (all_nodes.count(a) && all_nodes.count(b)) {
                double dist = haversine(all_nodes[a].lng, all_nodes[a].lat,
                                       all_nodes[b].lng, all_nodes[b].lat);
                int road_dist = (int)(dist * 1.3);
                int t = max(1, (int)round(road_dist / (60.0 * 1000 / 60)));
                edges.push_back({a, b, road_dist, t});
                edges.push_back({b, a, road_dist, t});
            }
        }
    }

    for (int sid = 21; sid <= 110; sid++) {
        if (all_nodes.count(sid) && all_nodes.count(1) && sid != 1) {
            double dist = haversine(all_nodes[1].lng, all_nodes[1].lat,
                                   all_nodes[sid].lng, all_nodes[sid].lat);
            int road_dist = (int)(dist * 1.3);
            int t = max(1, (int)round(road_dist / (60.0 * 1000 / 60)));
            edges.push_back({1, sid, road_dist, t});
            edges.push_back({sid, 1, road_dist, t});
        }
    }

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

    map<pair<int,int>,pair<int,int>> edge_map;
    for (auto& e : edges) {
        int a = get<0>(e), b = get<1>(e), dist = get<2>(e), t = get<3>(e);
        auto key = make_pair(min(a,b), max(a,b));
        if (!edge_map.count(key) || dist < edge_map[key].first) {
            edge_map[key] = {dist, t};
        }
    }

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

int cmd_json() {
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
            ss << ",\"name\":\"" << name << "\"";
            ss << ",\"lng\":" << fixed << setprecision(6) << lng;
            ss << ",\"lat\":" << fixed << setprecision(6) << lat;
            ss << ",\"price\":" << fixed << setprecision(0) << price;
            ss << ",\"score\":" << fixed << setprecision(1) << score;
            ss << ",\"category\":\"" << category << "\"";
            ss << ",\"address\":\"" << address << "\"";
            ss << ",\"opentime\":\"" << opentime << "\"";
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
            ss << ",\"name\":\"" << name << "\"";
            ss << ",\"description\":\"" << description << "\"";
            ss << ",\"address\":\"" << address << "\"";
            ss << ",\"lng\":" << fixed << setprecision(6) << lng;
            ss << ",\"lat\":" << fixed << setprecision(6) << lat;
            ss << ",\"type\":\"" << type << "\"";
            ss << ",\"ticketInfo\":\"" << ticketInfo << "\"";
            ss << ",\"openingTime\":\"" << openingTime << "\"";
            ss << ",\"recommendDuration\":\"" << recommendDuration << "\"";
            ss << ",\"bestSeason\":\"" << bestSeason << "\"";
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
