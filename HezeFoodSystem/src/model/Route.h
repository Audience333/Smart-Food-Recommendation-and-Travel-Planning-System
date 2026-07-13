#ifndef ROUTE_H
#define ROUTE_H

#include <string>
#include <sstream>
#include <iomanip>

/**
 * 路线结果结构体
 *
 * 业务用途：
 *   - 存储路线规划的结果
 *   - 包含途经点序列、总距离、预计时间
 */
struct RouteResult {
    // 途经点（存储地点 ID，正数=景点，负数=美食）
    SeqList<int>    waypoints;
    double          totalDistance;   // 总距离（公里）
    double          totalTime;      // 预计时间（小时）
    std::string     description;    // 路线描述

    RouteResult() : totalDistance(0), totalTime(0) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "=== 路线规划结果 ===" << std::endl;
        oss << "途经点数: " << waypoints.size() << std::endl;
        oss << "总距离: " << std::fixed << std::setprecision(1) << totalDistance << " km" << std::endl;
        oss << "预计时间: " << std::fixed << std::setprecision(1) << totalTime << " 小时" << std::endl;
        if (!description.empty()) {
            oss << "说明: " << description << std::endl;
        }
        return oss.str();
    }
};

/**
 * 地理坐标点
 * 用于 KD-Tree 和距离计算
 */
struct GeoPoint {
    int     id;         // 关联的美食/景点 ID
    int     type;       // 0=美食, 1=景点
    double  longitude;  // 经度
    double  latitude;   // 纬度
    std::string name;   // 名称

    GeoPoint() : id(0), type(0), longitude(0), latitude(0) {}
    GeoPoint(int i, int t, double lng, double lat, const std::string& n)
        : id(i), type(t), longitude(lng), latitude(lat), name(n) {}

    bool operator==(const GeoPoint& other) const { return id == other.id && type == other.type; }
    bool operator!=(const GeoPoint& other) const { return !(*this == other); }
};

/**
 * 推荐结果结构体
 */
struct RecommendResult {
    int     foodId;     // 美食 ID
    std::string name;   // 美食名称
    double  similarity; // 相似度 (0-1)
    double  score;      // 评分

    RecommendResult() : foodId(0), similarity(0), score(0) {}

    // 用于堆排序（按相似度比较）
    bool operator<(const RecommendResult& other) const {
        return similarity < other.similarity;
    }
    bool operator>(const RecommendResult& other) const {
        return similarity > other.similarity;
    }
};

#endif // ROUTE_H
