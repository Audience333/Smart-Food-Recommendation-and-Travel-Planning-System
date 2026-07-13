#ifndef ROUTESERVICE_H
#define ROUTESERVICE_H

#include <string>
#include <iostream>
#include <iomanip>

#include "../model/Food.h"
#include "../model/Spot.h"
#include "../structure/Graph.h"
#include "../structure/SeqList.h"
#include "../structure/HashTable.h"
#include "../algorithm/BFS.h"
#include "../algorithm/Dijkstra.h"
#include "../utils/FileManager.h"

/**
 * 地图节点数据结构
 */
struct MapNode {
    int         id;             // 原始 ID（美食/景点）
    std::string name;           // 名称
    int         type;           // 0=美食, 1=景点
    double      longitude;      // 经度
    double      latitude;       // 纬度

    MapNode() : id(0), type(0), longitude(0), latitude(0) {}
    MapNode(int i, const std::string& n, int t, double lng, double lat)
        : id(i), name(n), type(t), longitude(lng), latitude(lat) {}
};

/**
 * 城市漫游路线规划服务
 *
 * 功能：
 *   1. 加载美食和景点数据，构建城市图
 *   2. 加载道路连接数据
 *   3. 最短路径查询
 *   4. 附近美食搜索
 *   5. 美食漫游路线生成
 *
 * 数据结构使用：
 *   - Graph<MapNode>: 邻接表图，存储城市地图
 *   - HashTable: ID → 图索引映射
 *   - BFS: 附近搜索
 *   - Dijkstra: 最短路径
 *   - Heap: 优先队列
 *   - Queue: BFS 队列
 */
class RouteService {
private:
    Graph<MapNode>          cityGraph_;         // 城市图
    HashTable<int, int>     idToIndex_;         // 原始 ID → 图索引
    HashTable<int, int>     indexToType_;       // 图索引 → 类型（0=美食,1=景点）
    bool                    loaded_;

    /**
     * 从 road.txt 加载道路连接
     */
    bool loadRoads(const std::string& roadFile) {
        std::ifstream file(roadFile);
        if (!file.is_open()) {
            std::cerr << "[RouteService] 错误: 无法打开 " << roadFile << std::endl;
            return false;
        }

        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            // 解析: fromId|toId|distance|time
            SeqList<std::string> fields = FileManager::split(line, '|');
            if (fields.size() < 4) continue;

            int fromId = std::stoi(FileManager::trim(fields[0]));
            int toId = std::stoi(FileManager::trim(fields[1]));
            double distance = std::stod(FileManager::trim(fields[2]));
            double time = std::stod(FileManager::trim(fields[3]));

            // 查找图索引
            if (idToIndex_.contains(fromId) && idToIndex_.contains(toId)) {
                int fromIdx = idToIndex_.get(fromId);
                int toIdx = idToIndex_.get(toId);
                cityGraph_.addEdge(fromIdx, toIdx, distance, time);
                count++;
            }
        }
        file.close();

        std::cout << "[RouteService] 加载道路连接: " << count << " 条" << std::endl;
        return true;
    }

public:
    RouteService() : loaded_(false) {}

    /**
     * 加载数据并构建城市图
     */
    bool load(const std::string& foodFile = "data/food.txt",
              const std::string& spotFile = "data/spot.txt",
              const std::string& roadFile = "data/road.txt") {

        std::cout << "[RouteService] 加载城市数据..." << std::endl;

        // 加载美食数据
        SeqList<Food> foods = FileManager::loadFoodData(foodFile);
        for (int i = 0; i < foods.size(); i++) {
            MapNode node(foods[i].id, foods[i].name, 0,
                        foods[i].longitude, foods[i].latitude);
            int idx = cityGraph_.addVertex(node);
            idToIndex_.put(foods[i].id, idx);
            indexToType_.put(idx, 0);
        }

        // 加载景点数据
        SeqList<Spot> spots = FileManager::loadSpotData(spotFile);
        for (int i = 0; i < spots.size(); i++) {
            MapNode node(spots[i].id, spots[i].name, 1,
                        spots[i].longitude, spots[i].latitude);
            int idx = cityGraph_.addVertex(node);
            idToIndex_.put(spots[i].id, idx);
            indexToType_.put(idx, 1);
        }

        // 加载道路连接
        if (!loadRoads(roadFile)) return false;

        loaded_ = true;
        std::cout << "[RouteService] 城市图构建完成: "
                  << cityGraph_.vertexCount() << " 个节点, "
                  << cityGraph_.edgeCount() << " 条边" << std::endl;
        return true;
    }

    /**
     * 显示城市地图
     */
    void displayMap() const {
        cityGraph_.displayGraph([](const MapNode& node) -> std::string {
            return node.name;
        });
    }

    /**
     * 查找最短路径
     *
     * @param fromId 起点 ID
     * @param toId 终点 ID
     * @return 路径结果
     *
     * 时间复杂度: O((V + E) log V)
     */
    Dijkstra::PathResult findShortestPath(int fromId, int toId) {
        if (!loaded_ || !idToIndex_.contains(fromId) || !idToIndex_.contains(toId)) {
            return Dijkstra::PathResult();
        }
        int fromIdx = idToIndex_.get(fromId);
        int toIdx = idToIndex_.get(toId);
        return Dijkstra::findShortestPath(cityGraph_, fromIdx, toIdx);
    }

    /**
     * 查找最快路径
     */
    Dijkstra::PathResult findFastestPath(int fromId, int toId) {
        if (!loaded_ || !idToIndex_.contains(fromId) || !idToIndex_.contains(toId)) {
            return Dijkstra::PathResult();
        }
        int fromIdx = idToIndex_.get(fromId);
        int toIdx = idToIndex_.get(toId);
        return Dijkstra::findFastestPath(cityGraph_, fromIdx, toIdx);
    }

    /**
     * 搜索附近美食
     *
     * @param locationId 当前位置 ID
     * @param maxLayer 搜索层数
     * @return 附近的美食列表
     *
     * 时间复杂度: O(V + E)
     */
    SeqList<BFS::SearchResult> findNearbyFood(int locationId, int maxLayer = 3) {
        SeqList<BFS::SearchResult> results;
        if (!loaded_ || !idToIndex_.contains(locationId)) return results;

        int startIdx = idToIndex_.get(locationId);
        SeqList<BFS::SearchResult> all = BFS::searchLayers(cityGraph_, startIdx, maxLayer);

        // 只保留美食节点
        for (int i = 0; i < all.size(); i++) {
            int idx = all[i].vertexIndex;
            if (indexToType_.contains(idx) && indexToType_.get(idx) == 0) {
                results.push_back(all[i]);
            }
        }
        return results;
    }

    /**
     * 搜索附近景点
     */
    SeqList<BFS::SearchResult> findNearbySpots(int locationId, int maxLayer = 3) {
        SeqList<BFS::SearchResult> results;
        if (!loaded_ || !idToIndex_.contains(locationId)) return results;

        int startIdx = idToIndex_.get(locationId);
        SeqList<BFS::SearchResult> all = BFS::searchLayers(cityGraph_, startIdx, maxLayer);

        for (int i = 0; i < all.size(); i++) {
            int idx = all[i].vertexIndex;
            if (indexToType_.contains(idx) && indexToType_.get(idx) == 1) {
                results.push_back(all[i]);
            }
        }
        return results;
    }

    /**
     * 获取节点名称
     */
    std::string getNodeName(int id) const {
        if (!idToIndex_.contains(id)) return "未知";
        int idx = idToIndex_.get(id);
        return cityGraph_.getVertex(idx).name;
    }

    /**
     * 获取所有美食 ID
     */
    SeqList<int> getAllFoodIds() const {
        SeqList<int> ids;
        for (int i = 0; i < cityGraph_.vertexCount(); i++) {
            if (indexToType_.contains(i) && indexToType_.get(i) == 0) {
                ids.push_back(cityGraph_.getVertex(i).id);
            }
        }
        return ids;
    }

    /**
     * 获取所有景点 ID
     */
    SeqList<int> getAllSpotIds() const {
        SeqList<int> ids;
        for (int i = 0; i < cityGraph_.vertexCount(); i++) {
            if (indexToType_.contains(i) && indexToType_.get(i) == 1) {
                ids.push_back(cityGraph_.getVertex(i).id);
            }
        }
        return ids;
    }

    // ==================== 输出 ====================

    /**
     * 打印路径结果
     */
    void printPathResult(const Dijkstra::PathResult& result,
                          const std::string& title = "最短路径") const {
        std::cout << std::endl << "=== " << title << " ===" << std::endl;
        if (!result.found) {
            std::cout << "  未找到可达路径" << std::endl;
            return;
        }

        std::cout << "  路线: ";
        for (int i = 0; i < result.path.size(); i++) {
            int idx = result.path[i];
            const MapNode& node = cityGraph_.getVertex(idx);
            std::string typeStr = (node.type == 0) ? "[美食]" : "[景点]";
            std::cout << typeStr << node.name;
            if (i < result.path.size() - 1) std::cout << " → ";
        }
        std::cout << std::endl;

        std::cout << "  总距离: " << std::fixed << std::setprecision(0)
                  << result.totalDistance << " 米";
        if (result.totalDistance >= 1000) {
            std::cout << " (" << std::fixed << std::setprecision(1)
                      << result.totalDistance / 1000 << " 公里)";
        }
        std::cout << std::endl;

        std::cout << "  预计时间: " << std::fixed << std::setprecision(1)
                  << result.totalTime << " 分钟";
        if (result.totalTime >= 60) {
            std::cout << " (" << std::fixed << std::setprecision(1)
                      << result.totalTime / 60 << " 小时)";
        }
        std::cout << std::endl;
    }

    /**
     * 打印附近搜索结果
     */
    void printNearbyResults(const SeqList<BFS::SearchResult>& results,
                             const std::string& title = "附近搜索") const {
        std::cout << std::endl << "=== " << title << " ===" << std::endl;
        if (results.empty()) {
            std::cout << "  未找到附近地点" << std::endl;
            return;
        }

        std::cout << "  找到 " << results.size() << " 个地点:" << std::endl;
        std::cout << "  " << std::string(60, '-') << std::endl;

        for (int i = 0; i < results.size(); i++) {
            int idx = results[i].vertexIndex;
            const MapNode& node = cityGraph_.getVertex(idx);
            std::string typeStr = (node.type == 0) ? "[美食]" : "[景点]";
            std::cout << "  " << std::left << std::setw(4) << (i + 1)
                      << typeStr << std::setw(16) << node.name
                      << "距离: " << std::setw(8) << std::fixed << std::setprecision(0)
                      << results[i].totalDistance << "m"
                      << "时间: " << std::fixed << std::setprecision(1)
                      << results[i].totalTime << "min"
                      << " (" << results[i].layer << "跳)" << std::endl;
        }
        std::cout << std::endl;
    }
};

#endif // ROUTESERVICE_H
