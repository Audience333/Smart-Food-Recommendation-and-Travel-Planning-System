#ifndef GEOUTIL_H
#define GEOUTIL_H

#include <cmath>
#include <string>

/**
 * 地理计算工具类
 *
 * 职责：
 *   - 使用 Haversine 公式计算两点间的球面距离
 *   - 坐标格式转换
 *   - 距离格式化
 *
 * 应用场景：
 *   1. 路线规划中的距离计算
 *   2. 附近美食/景点搜索
 *   3. 地图节点间边的生成
 *
 * 数学原理：
 *   Haversine 公式基于球面三角学，计算地球表面两点间的大圆距离。
 *   地球半径取平均值 6371000 米。
 *
 *   公式: a = sin²(Δlat/2) + cos(lat1) * cos(lat2) * sin²(Δlng/2)
 *         c = 2 * atan2(√a, √(1-a))
 *         d = R * c
 *
 * 时间复杂度: O(1)
 * 空间复杂度: O(1)
 */
class GeoUtil {
public:
    // 地球平均半径（米）
    static constexpr double EARTH_RADIUS = 6371000.0;

    // π 常量
    static constexpr double PI = 3.14159265358979323846;

    /**
     * 角度转弧度
     * @param degrees 角度值
     * @return 弧度值
     */
    static double toRadians(double degrees) {
        return degrees * PI / 180.0;
    }

    /**
     * 弧度转角度
     * @param radians 弧度值
     * @return 角度值
     */
    static double toDegrees(double radians) {
        return radians * 180.0 / PI;
    }

    /**
     * Haversine 公式计算两点间距离
     *
     * @param lng1 第一个点的经度
     * @param lat1 第一个点的纬度
     * @param lng2 第二个点的经度
     * @param lat2 第二个点的纬度
     * @return 距离（米）
     *
     * 示例:
     *   double dist = GeoUtil::haversine(115.4806, 35.2337, 115.9400, 34.7900);
     *   // 计算菏泽市区到单县的距离
     *
     * 精度: 约 ±0.3%（因地球椭球体形状导致的误差）
     */
    static double haversine(double lng1, double lat1, double lng2, double lat2) {
        // 转换为弧度
        double radLat1 = toRadians(lat1);
        double radLat2 = toRadians(lat2);
        double dLat = toRadians(lat2 - lat1);
        double dLng = toRadians(lng2 - lng1);

        // Haversine 公式
        double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                   std::cos(radLat1) * std::cos(radLat2) *
                   std::sin(dLng / 2) * std::sin(dLng / 2);
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

        return EARTH_RADIUS * c;
    }

    /**
     * 计算距离并返回格式化字符串
     *
     * @param lng1 经度1
     * @param lat1 纬度1
     * @param lng2 经度2
     * @param lat2 纬度2
     * @return 格式化距离字符串（如 "3.5 km" 或 "800 m"）
     */
    static std::string distanceStr(double lng1, double lat1, double lng2, double lat2) {
        double dist = haversine(lng1, lat1, lng2, lat2);
        if (dist >= 1000) {
            return std::to_string((int)(dist / 1000)) + "." +
                   std::to_string(((int)dist % 1000) / 100) + " km";
        }
        return std::to_string((int)dist) + " m";
    }

    /**
     * 根据距离估算步行时间
     * @param distanceMeters 距离（米）
     * @return 步行时间（分钟），平均速度 5 km/h
     */
    static double estimateWalkTime(double distanceMeters) {
        // 步行速度: 5 km/h = 83.33 m/min
        return distanceMeters / 83.33;
    }

    /**
     * 根据距离估算驾车时间
     * @param distanceMeters 距离（米）
     * @return 驾车时间（分钟），城市平均速度 30 km/h
     */
    static double estimateDriveTime(double distanceMeters) {
        // 城市驾车速度: 30 km/h = 500 m/min
        return distanceMeters / 500.0;
    }

    /**
     * 判断两点是否在指定范围内
     *
     * @param lng1 经度1
     * @param lat1 纬度1
     * @param lng2 经度2
     * @param lat2 纬度2
     * @param radiusMeters 范围半径（米）
     * @return 是否在范围内
     */
    static bool isWithinRange(double lng1, double lat1,
                               double lng2, double lat2,
                               double radiusMeters) {
        return haversine(lng1, lat1, lng2, lat2) <= radiusMeters;
    }

    /**
     * 解析高德坐标字符串 "lng,lat"
     *
     * @param location 坐标字符串（格式: "115.4806,35.2337"）
     * @param lng 输出经度
     * @param lat 输出纬度
     * @return 是否解析成功
     */
    static bool parseLocation(const std::string& location, double& lng, double& lat) {
        size_t commaPos = location.find(',');
        if (commaPos == std::string::npos) return false;

        try {
            lng = std::stod(location.substr(0, commaPos));
            lat = std::stod(location.substr(commaPos + 1));
            return true;
        } catch (...) {
            return false;
        }
    }

    /**
     * 格式化坐标为字符串
     */
    static std::string formatLocation(double lng, double lat) {
        return std::to_string(lng) + "," + std::to_string(lat);
    }
};

#endif // GEOUTIL_H
