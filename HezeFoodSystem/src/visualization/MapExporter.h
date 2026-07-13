#ifndef MAPEXPORTER_H
#define MAPEXPORTER_H

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "../model/Food.h"
#include "../model/Spot.h"
#include "../model/Route.h"
#include "../structure/SeqList.h"
#include "../structure/Graph.h"
#include "../algorithm/Dijkstra.h"
#include "../services/RouteService.h"

/**
 * 地图数据导出器
 *
 * 职责：
 *   - 将 C++ 数据结构导出为 JSON 格式
 *   - 生成高德地图 JavaScript API 所需的数据文件
 *   - 支持美食、景点、路线三种数据类型
 *
 * 输出文件：
 *   - web/data/food.json    美食数据
 *   - web/data/spot.json    景点数据
 *   - web/data/route.json   路线数据
 *
 * JSON 格式说明：
 *   - 使用标准 JSON 格式
 *   - 坐标使用 GCJ-02 坐标系（高德坐标系）
 *   - 数值保留适当精度
 */
class MapExporter {
public:
    /**
     * 导出美食数据为 JSON
     *
     * @param foods 美食列表
     * @param outputPath 输出文件路径
     * @return 是否成功
     *
     * 输出格式:
     * [
     *   {
     *     "id": 1,
     *     "name": "单县羊肉汤",
     *     "lng": 115.9400,
     *     "lat": 34.7900,
     *     "price": 35,
     *     "score": 4.8,
     *     "category": "汤类",
     *     "tags": ["早餐", "地方特色"]
     *   },
     *   ...
     * ]
     */
    static bool exportFoodJson(const SeqList<Food>& foods,
                                const std::string& outputPath = "web/data/food.json") {
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "[MapExporter] 错误: 无法写入 " << outputPath << std::endl;
            return false;
        }

        file << "[" << std::endl;

        for (int i = 0; i < foods.size(); i++) {
            const Food& f = foods[i];
            file << "  {" << std::endl;
            file << "    \"id\": " << f.id << "," << std::endl;
            file << "    \"name\": \"" << escapeJson(f.name) << "\"," << std::endl;
            file << "    \"lng\": " << std::fixed << std::setprecision(6) << f.longitude << "," << std::endl;
            file << "    \"lat\": " << std::fixed << std::setprecision(6) << f.latitude << "," << std::endl;
            file << "    \"price\": " << std::fixed << std::setprecision(0) << f.price << "," << std::endl;
            file << "    \"score\": " << std::fixed << std::setprecision(1) << f.score << "," << std::endl;
            file << "    \"category\": \"" << escapeJson(f.category) << "\"," << std::endl;
            file << "    \"tags\": " << stringToJsonArray(f.tags) << std::endl;
            file << "  }";
            if (i < foods.size() - 1) file << ",";
            file << std::endl;
        }

        file << "]" << std::endl;
        file.close();

        std::cout << "[MapExporter] 美食数据已导出: " << outputPath
                  << " (" << foods.size() << " 条)" << std::endl;
        return true;
    }

    /**
     * 导出景点数据为 JSON
     *
     * @param spots 景点列表
     * @param outputPath 输出文件路径
     * @return 是否成功
     *
     * 输出格式:
     * [
     *   {
     *     "id": 1,
     *     "name": "曹州牡丹园",
     *     "lng": 115.4806,
     *     "lat": 35.2337,
     *     "type": "自然景观",
     *     "score": 4.8,
     *     "address": "菏泽市牡丹区牡丹南路",
     *     "ticketInfo": "免费",
     *     "openingTime": "08:00-18:00",
     *     "description": "世界上面积最大的牡丹园"
     *   },
     *   ...
     * ]
     */
    static bool exportSpotJson(const SeqList<Spot>& spots,
                                const std::string& outputPath = "web/data/spot.json") {
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "[MapExporter] 错误: 无法写入 " << outputPath << std::endl;
            return false;
        }

        file << "[" << std::endl;

        for (int i = 0; i < spots.size(); i++) {
            const Spot& s = spots[i];
            file << "  {" << std::endl;
            file << "    \"id\": " << s.id << "," << std::endl;
            file << "    \"name\": \"" << escapeJson(s.name) << "\"," << std::endl;
            file << "    \"lng\": " << std::fixed << std::setprecision(6) << s.longitude << "," << std::endl;
            file << "    \"lat\": " << std::fixed << std::setprecision(6) << s.latitude << "," << std::endl;
            file << "    \"type\": \"" << escapeJson(s.type) << "\"," << std::endl;
            file << "    \"score\": " << std::fixed << std::setprecision(1) << s.score << "," << std::endl;
            file << "    \"address\": \"" << escapeJson(s.address) << "\"," << std::endl;
            file << "    \"ticketInfo\": \"" << escapeJson(s.ticketInfo) << "\"," << std::endl;
            file << "    \"openingTime\": \"" << escapeJson(s.openingTime) << "\"," << std::endl;
            file << "    \"description\": \"" << escapeJson(s.description) << "\"" << std::endl;
            file << "  }";
            if (i < spots.size() - 1) file << ",";
            file << std::endl;
        }

        file << "]" << std::endl;
        file.close();

        std::cout << "[MapExporter] 景点数据已导出: " << outputPath
                  << " (" << spots.size() << " 条)" << std::endl;
        return true;
    }

    /**
     * 导出路线数据为 JSON
     *
     * @param pathResult Dijkstra 路径结果
     * @param graph 城市图
     * @param routeName 路线名称
     * @param outputPath 输出文件路径
     * @return 是否成功
     *
     * 输出格式:
     * {
     *   "name": "推荐路线",
     *   "totalDistance": 1234,
     *   "totalTime": 15.5,
     *   "waypoints": [
     *     {"id": 1, "name": "曹州牡丹园", "lng": 115.48, "lat": 35.23, "type": "spot"},
     *     {"id": 101, "name": "菏泽烧牛肉", "lng": 115.48, "lat": 35.23, "type": "food"}
     *   ],
     *   "path": [
     *     [115.4806, 35.2337],
     *     [115.4810, 35.2340]
     *   ]
     * }
     */
    template<typename T>
    static bool exportRouteJson(const Dijkstra::PathResult& pathResult,
                                 const Graph<T>& graph,
                                 const std::string& routeName = "推荐路线",
                                 const std::string& outputPath = "web/data/route.json") {
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "[MapExporter] 错误: 无法写入 " << outputPath << std::endl;
            return false;
        }

        file << "{" << std::endl;
        file << "  \"name\": \"" << escapeJson(routeName) << "\"," << std::endl;
        file << "  \"found\": " << (pathResult.found ? "true" : "false") << "," << std::endl;
        file << "  \"totalDistance\": " << std::fixed << std::setprecision(0) << pathResult.totalDistance << "," << std::endl;
        file << "  \"totalTime\": " << std::fixed << std::setprecision(1) << pathResult.totalTime << "," << std::endl;

        // 导出途经点
        file << "  \"waypoints\": [" << std::endl;
        for (int i = 0; i < pathResult.path.size(); i++) {
            int idx = pathResult.path[i];
            const auto& node = graph.getVertex(idx);
            file << "    {" << std::endl;
            file << "      \"name\": \"" << escapeJson(getNodeName(node)) << "\"," << std::endl;
            file << "      \"lng\": " << std::fixed << std::setprecision(6) << getNodeLng(node) << "," << std::endl;
            file << "      \"lat\": " << std::fixed << std::setprecision(6) << getNodeLat(node) << "," << std::endl;
            file << "      \"type\": \"" << (getNodeType(node) == 0 ? "food" : "spot") << "\"" << std::endl;
            file << "    }";
            if (i < pathResult.path.size() - 1) file << ",";
            file << std::endl;
        }
        file << "  ]," << std::endl;

        // 导出路径坐标（用于 Polyline）
        file << "  \"path\": [" << std::endl;
        for (int i = 0; i < pathResult.path.size(); i++) {
            int idx = pathResult.path[i];
            const auto& node = graph.getVertex(idx);
            file << "    [" << std::fixed << std::setprecision(6)
                 << getNodeLng(node) << ", " << getNodeLat(node) << "]";
            if (i < pathResult.path.size() - 1) file << ",";
            file << std::endl;
        }
        file << "  ]" << std::endl;

        file << "}" << std::endl;
        file.close();

        std::cout << "[MapExporter] 路线数据已导出: " << outputPath << std::endl;
        return true;
    }

    /**
     * 导出多条路线数据为 JSON
     *
     * @param routes 路线结果列表
     * @param graph 城市图
     * @param outputPath 输出文件路径
     * @return 是否成功
     */
    template<typename T>
    static bool exportMultiRouteJson(const SeqList<Dijkstra::PathResult>& routes,
                                      const SeqList<std::string>& routeNames,
                                      const Graph<T>& graph,
                                      const std::string& outputPath = "web/data/routes.json") {
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cerr << "[MapExporter] 错误: 无法写入 " << outputPath << std::endl;
            return false;
        }

        file << "[" << std::endl;

        for (int r = 0; r < routes.size(); r++) {
            const Dijkstra::PathResult& pathResult = routes[r];
            std::string routeName = (r < routeNames.size()) ? routeNames[r] : "路线" + std::to_string(r + 1);

            file << "  {" << std::endl;
            file << "    \"name\": \"" << escapeJson(routeName) << "\"," << std::endl;
            file << "    \"found\": " << (pathResult.found ? "true" : "false") << "," << std::endl;
            file << "    \"totalDistance\": " << std::fixed << std::setprecision(0) << pathResult.totalDistance << "," << std::endl;
            file << "    \"totalTime\": " << std::fixed << std::setprecision(1) << pathResult.totalTime << "," << std::endl;

            // 途经点
            file << "    \"waypoints\": [" << std::endl;
            for (int i = 0; i < pathResult.path.size(); i++) {
                int idx = pathResult.path[i];
                const auto& node = graph.getVertex(idx);
                file << "      {\"name\": \"" << escapeJson(getNodeName(node))
                     << "\", \"lng\": " << std::fixed << std::setprecision(6) << getNodeLng(node)
                     << ", \"lat\": " << std::fixed << std::setprecision(6) << getNodeLat(node)
                     << ", \"type\": \"" << (getNodeType(node) == 0 ? "food" : "spot") << "\"}";
                if (i < pathResult.path.size() - 1) file << ",";
                file << std::endl;
            }
            file << "    ]," << std::endl;

            // 路径坐标
            file << "    \"path\": [" << std::endl;
            for (int i = 0; i < pathResult.path.size(); i++) {
                int idx = pathResult.path[i];
                const auto& node = graph.getVertex(idx);
                file << "      [" << std::fixed << std::setprecision(6)
                     << getNodeLng(node) << ", " << getNodeLat(node) << "]";
                if (i < pathResult.path.size() - 1) file << ",";
                file << std::endl;
            }
            file << "    ]" << std::endl;

            file << "  }";
            if (r < routes.size() - 1) file << ",";
            file << std::endl;
        }

        file << "]" << std::endl;
        file.close();

        std::cout << "[MapExporter] 多条路线数据已导出: " << outputPath
                  << " (" << routes.size() << " 条)" << std::endl;
        return true;
    }

    /**
     * 导出所有数据（美食、景点、路线）
     *
     * @param foods 美食列表
     * @param spots 景点列表
     * @param pathResult 路线结果
     * @param graph 城市图
     * @param outputDir 输出目录
     * @return 是否成功
     */
    template<typename T>
    static bool exportAll(const SeqList<Food>& foods,
                           const SeqList<Spot>& spots,
                           const Dijkstra::PathResult& pathResult,
                           const Graph<T>& graph,
                           const std::string& outputDir = "web/data") {
        // 创建输出目录
        std::string mkdirCmd = "mkdir -p " + outputDir;
        std::system(mkdirCmd.c_str());

        bool success = true;
        success &= exportFoodJson(foods, outputDir + "/food.json");
        success &= exportSpotJson(spots, outputDir + "/spot.json");
        success &= exportRouteJson(pathResult, graph, "推荐路线", outputDir + "/route.json");

        return success;
    }

private:
    /**
     * 转义 JSON 字符串中的特殊字符
     */
    static std::string escapeJson(const std::string& str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c;
            }
        }
        return result;
    }

    /**
     * 将逗号分隔的字符串转换为 JSON 数组
     * 例如: "早餐,地方特色" → ["早餐", "地方特色"]
     */
    static std::string stringToJsonArray(const std::string& str) {
        std::string result = "[";
        std::string token;
        std::istringstream iss(str);
        bool first = true;

        while (std::getline(iss, token, ',')) {
            // 去除首尾空格
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start != std::string::npos) {
                token = token.substr(start, end - start + 1);
            }

            if (!first) result += ", ";
            result += "\"" + escapeJson(token) + "\"";
            first = false;
        }

        result += "]";
        return result;
    }

    // ==================== MapNode 访问器 ====================
    // 适配 RouteService.h 中的 MapNode 结构

    static std::string getNodeName(const MapNode& node) {
        return node.name;
    }

    static double getNodeLng(const MapNode& node) {
        return node.longitude;
    }

    static double getNodeLat(const MapNode& node) {
        return node.latitude;
    }

    static int getNodeType(const MapNode& node) {
        return node.type;
    }
};

#endif // MAPEXPORTER_H
