/*
 * ============================================================================
 * 菏泽美食旅游推荐系统 - C++ 数据处理管道 (cmd_roads)
 * ============================================================================
 *
 * 【命令功能】
 *   pipeline roads — 重新计算道路连接命令（替代 recalc_roads.py）
 *
 * 【v2.0 更新】
 *   集成手写数据结构：AdjacencyGraph、Dijkstra、BFS。
 *
 * 【输入文件】
 *   data/food.txt    - 美食数据（含坐标）
 *   data/spot.txt    - 景点数据（含坐标）
 *   config/amap_config.txt - API Key（可选，用于驾车路径API）
 *
 * 【输出文件】
 *   data/road.txt    - 覆盖生成的路网连接数据
 *
 * 【算法流程（分层构建道路网络 + 数据结构集成）】
 *   第1步：将所有POI加载到 AdjacencyGraph
 *   第2层：各区县美食与枢纽连接
 *   第3层：牡丹区景点与美食的邻近连接
 *   第4层：牡丹区美食集群内部连接
 *   第5层：跨区枢纽全连接
 *   第6层：各景点与总枢纽(1)连接
 *   第7层：各区县内部节点间连接
 *   第1层（后执行）：使用 Dijkstra 查找景点间最短路径
 *   BFS额外功能：查找各主要景点周边的美食集群
 *
 * 【距离计算策略】
 *   - 有高德API Key时，支持点对点驾车距离
 *   - 无API Key或API调用失败时，使用 Dijkstra 在图中的路径距离
 *   - Dijkstra 不可用时，降级使用 Haversine 直线距离
 *   - 跨区道路乘以1.3系数模拟实际道路的绕行
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
#include <tuple>
#include <fstream>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

#include "../include/adjacency_graph.h"
#include "../include/dijkstra.h"
#include "../include/bfs.h"

using namespace std;

// ---- 外部工具函数声明 (定义在 pipeline_util.cpp) ----
extern bool validate_workdir();
extern map<string, string> load_config();
extern void load_nodes_to_graph(const string& path, AdjacencyGraph& graph,
                                 const char* type, int lngIdx, int latIdx);
extern double haversine(double lng1, double lat1, double lng2, double lat2);
extern int time_from_distance(double dist_m, double speed_kmh = 40);
extern pair<int,int> amap_driving(const string& key, double olng, double olat,
                                   double dlng, double dlat);

// ---- 全局常量（定义在 pipeline_util.cpp）----
extern const string FOOD_TXT;
extern const string SPOT_TXT;
extern const string ROAD_TXT;

// ====================================================================
// cmd_roads - 重新计算道路连接命令
// ====================================================================
int cmd_roads() {
    if (!validate_workdir()) return 1;
    printf("============================================================\n");
    printf("  Recalculate Road Connections (DS-Integrated v2.0)\n");
    printf("============================================================\n");

    auto config = load_config();
    string amap_key = config.count("AMAP_KEY") ? config["AMAP_KEY"] : "";
    if (!amap_key.empty()) printf("Using Amap driving API\n");
    else printf("Using Haversine/Dijkstra distance\n");

    // 第1步：创建邻接表图，加载所有POI节点
    AdjacencyGraph graph;
    load_nodes_to_graph(FOOD_TXT, graph, "food", 2, 3);
    load_nodes_to_graph(SPOT_TXT, graph, "spot", 4, 5);
    printf("Graph vertices: %d (spots+foods)\n", graph.vertexCount_());

    vector<tuple<int,int,int,int>> edges;

    auto hasNode = [&](int id) { return graph.hasVertex(id); };

    auto getNodeCoord = [&](int id, double& lng, double& lat) -> bool {
        const Vertex* v = graph.getVertexById(id);
        if (v == nullptr) return false;
        lng = v->lng; lat = v->lat;
        return true;
    };

    // 第2~7层：构建基础路网
    vector<int> mudan_spots = {1,2,8,9,10,11,12,13,14,15,16,20,18};
    vector<pair<int,int>> spot_connections = {
        {1,2},{1,13},{1,14},{2,13},{8,9},{8,12},{8,16},{9,10},{9,11},{9,16},
        {10,15},{11,20},{11,12},{12,16},{19,15},{3,17},{5,19}
    };

    // ===== 第二层：各区县美食与枢纽连接 =====
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
        if (!hasNode(hub_id)) continue;
        double hlng, hlat;
        if (!getNodeCoord(hub_id, hlng, hlat)) continue;

        vector<pair<int,double>> candidates;
        for (int fid : ch.second) {
            if (!hasNode(fid)) continue;
            double flng, flat;
            if (!getNodeCoord(fid, flng, flat)) continue;
            double dist = haversine(hlng, hlat, flng, flat);
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
            graph.addEdge(hub_id, c.first, dist, (double)t);
            taken++;
        }
    }

    // ===== 第三层：牡丹区景点与美食的邻近连接 =====
    vector<int> mudan_food_ids;
    for (int i = 101; i <= 120; i++) mudan_food_ids.push_back(i);
    for (int i = 164; i <= 188; i++) mudan_food_ids.push_back(i);

    for (int sid : mudan_spots) {
        if (!hasNode(sid)) continue;
        double slng, slat;
        if (!getNodeCoord(sid, slng, slat)) continue;
        vector<pair<int,double>> nearby;
        for (int fid : mudan_food_ids) {
            if (!hasNode(fid)) continue;
            double flng, flat;
            if (!getNodeCoord(fid, flng, flat)) continue;
            double dist = haversine(slng, slat, flng, flat);
            if (dist < 5000) nearby.push_back({fid, dist});
        }
        sort(nearby.begin(), nearby.end(), [](auto& a, auto& b) { return a.second < b.second; });
        int taken = 0;
        for (auto& n : nearby) {
            if (taken >= 5) break;
            int t = time_from_distance(n.second);
            edges.push_back({sid, n.first, (int)n.second, t});
            edges.push_back({n.first, sid, (int)n.second, t});
            graph.addEdge(sid, n.first, n.second, (double)t);
            taken++;
        }
    }

    // ===== 第四层：牡丹区美食集群内部连接 =====
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
                if (hasNode(a) && hasNode(b)) {
                    double alng, alat, blng, blat;
                    if (getNodeCoord(a, alng, alat) && getNodeCoord(b, blng, blat)) {
                        double dist = haversine(alng, alat, blng, blat);
                        if (dist < 3000) {
                            int t = time_from_distance(dist);
                            edges.push_back({a, b, (int)dist, t});
                            edges.push_back({b, a, (int)dist, t});
                            graph.addEdge(a, b, dist, (double)t);
                        }
                    }
                }
            }
        }
    }

    for (int a = 164; a <= 188; a++) {
        for (int b = a + 1; b <= 188; b++) {
            if (hasNode(a) && hasNode(b)) {
                double alng, alat, blng, blat;
                if (getNodeCoord(a, alng, alat) && getNodeCoord(b, blng, blat)) {
                    double dist = haversine(alng, alat, blng, blat);
                    if (dist < 5000) {
                        int t = time_from_distance(dist);
                        edges.push_back({a, b, (int)dist, t});
                        edges.push_back({b, a, (int)dist, t});
                        graph.addEdge(a, b, dist, (double)t);
                    }
                }
            }
        }
    }

    // ===== 第五层：跨区枢纽全连接 =====
    vector<int> hub_ids = {1,121,129,136,142,148,153,157,161};
    for (size_t i = 0; i < hub_ids.size(); i++) {
        for (size_t j = i+1; j < hub_ids.size(); j++) {
            int a = hub_ids[i], b = hub_ids[j];
            if (hasNode(a) && hasNode(b)) {
                double alng, alat, blng, blat;
                if (getNodeCoord(a, alng, alat) && getNodeCoord(b, blng, blat)) {
                    double dist = haversine(alng, alat, blng, blat);
                    int road_dist = (int)(dist * 1.3);
                    int t = max(1, (int)round(road_dist / (60.0 * 1000 / 60)));
                    edges.push_back({a, b, road_dist, t});
                    edges.push_back({b, a, road_dist, t});
                    graph.addEdge(a, b, (double)road_dist, (double)t);
                }
            }
        }
    }

    // ===== 第六层：各景点与总枢纽(1)连接 =====
    for (int sid = 21; sid <= 110; sid++) {
        if (hasNode(sid) && hasNode(1) && sid != 1) {
            double slng, slat, l1ng, l1at;
            if (getNodeCoord(1, l1ng, l1at) && getNodeCoord(sid, slng, slat)) {
                double dist = haversine(l1ng, l1at, slng, slat);
                int road_dist = (int)(dist * 1.3);
                int t = max(1, (int)round(road_dist / (60.0 * 1000 / 60)));
                edges.push_back({1, sid, road_dist, t});
                edges.push_back({sid, 1, road_dist, t});
                graph.addEdge(1, sid, (double)road_dist, (double)t);
            }
        }
    }

    // ===== 第七层：各区县内部节点间连接 =====
    vector<pair<int,int>> county_ranges = {
        {121,128},{129,135},{136,141},{142,147},{148,152},{153,156},{157,160},{161,163}
    };
    for (auto& cr : county_ranges) {
        for (int a = cr.first; a <= cr.second; a++) {
            for (int b = a + 1; b <= cr.second; b++) {
                if (hasNode(a) && hasNode(b)) {
                    double alng, alat, blng, blat;
                    if (getNodeCoord(a, alng, alat) && getNodeCoord(b, blng, blat)) {
                        double dist = haversine(alng, alat, blng, blat);
                        if (dist < 5000) {
                            int t = time_from_distance(dist);
                            edges.push_back({a, b, (int)dist, t});
                            edges.push_back({b, a, (int)dist, t});
                            graph.addEdge(a, b, dist, (double)t);
                        }
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
                if (hasNode(a) && hasNode(b)) {
                    double alng, alat, blng, blat;
                    if (getNodeCoord(a, alng, alat) && getNodeCoord(b, blng, blat)) {
                        double dist = haversine(alng, alat, blng, blat);
                        if (dist < 10000) {
                            int t = time_from_distance(dist);
                            edges.push_back({a, b, (int)dist, t});
                            edges.push_back({b, a, (int)dist, t});
                            graph.addEdge(a, b, dist, (double)t);
                        }
                    }
                }
            }
        }
    }

    // ====================================================================
    // 第一层：使用 Dijkstra 查找景点间最短路径
    // ====================================================================
    printf("\n=== Dijkstra: Spot-to-Spot Shortest Paths ===\n");
    Dijkstra dijkstra(graph);

    for (auto& sc : spot_connections) {
        int a = sc.first, b = sc.second;
        if (!hasNode(a) || !hasNode(b)) continue;

        bool api_used = false;
        if (!amap_key.empty()) {
            double alng, alat, blng, blat;
            if (getNodeCoord(a, alng, alat) && getNodeCoord(b, blng, blat)) {
                auto [dist, dur] = amap_driving(amap_key, alng, alat, blng, blat);
                if (dist > 0) {
                    edges.push_back({a, b, dist, dur});
                    edges.push_back({b, a, dist, dur});
                    graph.addEdge(a, b, (double)dist, (double)dur);
                    api_used = true;
                    this_thread::sleep_for(chrono::milliseconds(500));
                }
            }
        }

        if (!api_used) {
            auto result = dijkstra.shortestPath(a, b, "distance");

            if (result.distance < 1e15 && !result.path.empty() && result.path.size() >= 2) {
                int t = time_from_distance(result.distance);
                edges.push_back({a, b, (int)result.distance, t});
                edges.push_back({b, a, (int)result.distance, t});
                graph.addEdge(a, b, result.distance, (double)t);

                printf("  Dijkstra %d->%d: %.0fm via %zu nodes",
                       a, b, result.distance, result.path.size());
                if (result.path.size() <= 6) {
                    printf(" [");
                    for (size_t pi = 0; pi < result.path.size(); pi++) {
                        if (pi > 0) printf("→");
                        printf("%d", result.path[pi]);
                    }
                    printf("]");
                }
                printf("\n");
            } else {
                double alng, alat, blng, blat;
                if (getNodeCoord(a, alng, alat) && getNodeCoord(b, blng, blat)) {
                    double dist = haversine(alng, alat, blng, blat);
                    int t = time_from_distance(dist);
                    edges.push_back({a, b, (int)dist, t});
                    edges.push_back({b, a, (int)dist, t});
                    graph.addEdge(a, b, dist, (double)t);
                    printf("  Haversine %d->%d: %.0fm (no graph path)\n", a, b, dist);
                }
            }
        }
    }

    // ====================================================================
    // BFS 额外功能：查找各主要景点周边的美食集群
    // ====================================================================
    printf("\n=== BFS: Nearby Food Clusters (5km radius) ===\n");
    BFS bfs(graph);
    vector<int> keySpots = {1, 2, 8, 9, 10, 12, 14, 16, 20};
    for (int spotId : keySpots) {
        if (!hasNode(spotId)) continue;
        auto nearby = bfs.nearbyFood(spotId, 5000.0);
        const Vertex* sv = graph.getVertexById(spotId);
        if (sv == nullptr) continue;
        printf("  %s (ID=%d): %zu foods within 5km\n",
               sv->name, spotId, nearby.size());
        int showCount = 0;
        for (auto& item : nearby) {
            if (showCount >= 5) break;
            printf("    - %s (%.0fm)\n", item.name.c_str(), item.distance);
            showCount++;
        }
    }

    // ====================================================================
    // 去重：对相同节点对的边保留最短距离
    // ====================================================================
    map<pair<int,int>,pair<int,int>> edge_map;
    for (auto& e : edges) {
        int a = get<0>(e), b = get<1>(e), dist = get<2>(e), t = get<3>(e);
        auto key = make_pair(min(a,b), max(a,b));
        if (!edge_map.count(key) || dist < edge_map[key].first) {
            edge_map[key] = {dist, t};
        }
    }

    // ====================================================================
    // 写入 road.txt
    // ====================================================================
    ofstream f(ROAD_TXT);
    f << "# 菏泽城市道路连接数据（基于高德地图真实坐标计算）\n";
    f << "# 格式: 起点ID|终点ID|距离(米)|预计时间(分钟)\n";
    f << "# 节点类型: 1-110=景点, 101-388=美食\n";
    f << "# 距离计算：Dijkstra最短路径 | Haversine公式 | 跨区道路×1.3系数\n";
    f << "# 数据结构：AdjacencyGraph + Dijkstra + BFS (手写C++实现)\n";
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
    printf("Graph: %d vertices, %d edges\n", graph.vertexCount_(), graph.edgeCount());
    printf("Written to %s\n", ROAD_TXT);
    return 0;
}
