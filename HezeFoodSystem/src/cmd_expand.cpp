/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 - C++ 数据处理管道 (cmd_expand)
 * ============================================================================
 *
 * 【命令功能】
 *   pipeline expand — 扩充POI数据命令（替代 expand_data.py）
 *
 * 【输入文件】
 *   data/food.txt     - 已有美食数据（用于去重和ID分配）
 *   data/spot.txt     - 已有景点数据（用于去重和ID分配）
 *   config/amap_config.txt - 高德API Key
 *
 * 【输出文件】
 *   data/food.txt     - 追加新发现的美食POI
 *   data/spot.txt     - 追加新发现的景点POI
 *
 * 【算法流程】
 *   1) 加载已有数据（名称集合、位置信息、最大ID）
 *   2) 遍历菏泽市9个区县（牡丹区、单县、曹县、郓城、巨野、东明、定陶、成武、鄄城）
 *   3) 对每个区县，使用16个美食关键词和7个景点关键词分别搜索
 *   4) 去重检查：名称精确匹配 OR (距离<200米 AND 名称相似度>0.7)
 *   5) 自动推断类别、营业时间、评分、价格、标签等属性
 *   6) 美食按评分排序取前25个，景点取前10个
 *   7) 追加写入数据文件（带日期标记的分隔注释）
 *
 * 【API调用】
 *   使用高德地图 /v3/place/text 文本搜索接口
 *   美食类型码: 050000|051000|052000 (餐饮大类)
 *   景点类型码: 110000|110100|110200 (旅游景点)
 *
 * 【本文件依赖 pipeline_util.cpp】
 *   所有工具函数均在其中定义，本文件仅通过 extern 声明引用。
 * ============================================================================
 */

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <thread>

#include "../include/decision_tree.h"
#include "../include/seq_stack.h"
#include "../include/doubly_linked_list.h"

using namespace std;

// ---- 外部工具函数声明 (定义在 pipeline_util.cpp) ----
extern bool validate_workdir();
extern map<string, string> load_config();
extern set<string> load_existing_names(const string& path);
struct PoiLoc { string name; double lng; double lat; };
extern vector<PoiLoc> load_food_locations(const string& path);
extern vector<PoiLoc> load_spot_locations(const string& path);
extern int get_max_id(const string& path, int id_index = 0);
extern vector<string> amap_search_poi(const string& key, const string& keywords,
                                       const string& types, const string& city,
                                       int page_size = 20, int max_pages = 2);
extern void parse_location(const string& loc_str, double& lng, double& lat);
extern bool is_duplicate(const string& name, double lng, double lat,
                         set<string>& existing_names, vector<PoiLoc>& existing_locations);
extern string infer_food_category(const string& name, const string& type_code);
extern double safe_float_from_json(const string& json, const string& key, double default_val);
extern string extract_opentime(const string& biz_ext, const string& category);
extern string generate_food_tags(const string& name, const string& category,
                                 const string& district, double score, double price,
                                 const string& type_code, const string& opentime);
extern string format_food_line(int id, const string& name, double lng, double lat,
                               double price, double score, const string& category,
                               const string& address, const string& opentime,
                               const string& photos, const string& tags);
extern string format_spot_line(int id, const string& name, const string& description,
                               const string& address, double lng, double lat,
                               const string& type, const string& ticketInfo,
                               const string& openingTime, const string& recommendDuration,
                               const string& bestSeason, double score, const string& tags);
extern string infer_spot_type(const string& type_name);
extern string json_str(const string& json, const string& key);

// ---- 全局常量（定义在 pipeline_util.cpp）----
extern const string FOOD_TXT;
extern const string SPOT_TXT;

// ====================================================================
// cmd_expand - 扩充POI数据命令
// ====================================================================
int cmd_expand() {
    if (!validate_workdir()) return 1;
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

    DoublyLinkedList<string> new_food_lines;
    DoublyLinkedList<string> new_spot_lines;
    int food_id_start = max_food_id + 1;
    int spot_id_start = max_spot_id + 1;

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

        struct FoodCandidate {
            string name, lng_str, lat_str, category, address, opentime, tags, type_code;
            double lng, lat, score, price;
        };
        vector<FoodCandidate> food_candidates;

        SeqStack<string> poiNameStack(200);

        for (auto& p : all_food_pois) {
            string loc_str = json_str(p, "location");
            double lng = 0, lat = 0;
            parse_location(loc_str, lng, lat);
            if (lng == 0 && lat == 0) continue;

            string name = json_str(p, "name");
            poiNameStack.push(name);
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

        printf("  SeqStack: Processed %d POI names\n", poiNameStack.size());

        sort(food_candidates.begin(), food_candidates.end(),
             [](const FoodCandidate& a, const FoodCandidate& b) { return a.score > b.score; });

        printf("  Applying DecisionTree filter to %zu candidates...\n", food_candidates.size());
        {
            DecisionTree<FoodCandidate> tree;
            vector<bool(*)(const FoodCandidate&)> conditions;
            conditions.push_back([](const FoodCandidate& f) { return f.score >= 3.5; });
            conditions.push_back([](const FoodCandidate& f) { return f.name.length() >= 2; });
            tree.build(conditions);
            auto filtered = tree.filter(food_candidates);
            printf("  DecisionTree: %zu -> %zu candidates after filter\n", food_candidates.size(), filtered.size());

            int max_food = 25;
            int taken = 0;
            for (auto& fc : filtered) {
                if (taken >= max_food) break;
                int new_id = food_id_start++;
                string line = format_food_line(new_id, fc.name, fc.lng, fc.lat,
                                               fc.price, fc.score, fc.category,
                                               fc.address, fc.opentime, "-", fc.tags);
                new_food_lines.pushBack(line);
                existing_food_names.insert(fc.name);
                existing_food_locs.push_back({fc.name, fc.lng, fc.lat});
                taken++;
            }
            printf("  New foods added: %d\n", taken);
        }

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
            new_spot_lines.pushBack(line);
            existing_spot_names.insert(sc.name);
            existing_spot_locs.push_back({sc.name, sc.lng, sc.lat});
            staken++;
        }
        printf("  New spots added: %d\n", staken);
        printf("\n");
    }

    // ---- 汇总输出 ----
    printf("============================================================\n");
    printf("Total new foods: %d\n", new_food_lines.size());
    printf("Total new spots: %d\n", new_spot_lines.size());

    if (!new_food_lines.empty()) {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&now));
        ofstream f(FOOD_TXT, ios::app);
        f << "\n# ========== C++ Pipeline Extension (" << buf << ") ==========\n";
        auto food_vec = new_food_lines.toVector();
        for (auto& line : food_vec) f << line << "\n";
        printf("Appended to %s\n", FOOD_TXT);
    }

    if (!new_spot_lines.empty()) {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&now));
        ofstream f(SPOT_TXT, ios::app);
        f << "\n# ========== C++ Pipeline Extension (" << buf << ") ==========\n";
        auto spot_vec = new_spot_lines.toVector();
        for (auto& line : spot_vec) f << line << "\n";
        printf("Appended to %s\n", SPOT_TXT);
    }

    printf("\nDone! Next: pipeline roads && pipeline json\n");
    return 0;
}
