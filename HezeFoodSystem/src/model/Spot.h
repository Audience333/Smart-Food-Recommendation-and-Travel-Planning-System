#ifndef SPOT_H
#define SPOT_H

#include <string>
#include <sstream>
#include <iomanip>

/**
 * 景点数据结构体
 *
 * 对应数据文件: data/spot.txt
 * 存储方式: SeqList<Spot>
 * 索引方式: HashTable<int, int>     (id → 下标)
 *           HashTable<string, SeqList<int>> (type → 下标列表)
 */
struct Spot {
    int         id;             // 唯一标识
    std::string name;           // 景点名称
    std::string description;    // 描述
    std::string address;        // 地址
    double      longitude;      // 经度
    double      latitude;       // 纬度
    std::string type;           // 景点类型（自然景观/历史文化/主题公园/宗教场所）
    std::string ticketInfo;     // 门票信息
    std::string openingTime;    // 开放时间
    std::string recommendDuration; // 建议游玩时长
    std::string bestSeason;     // 最佳季节
    double      score;          // 评分

    // 标签（用 '|' 分隔）
    std::string tagsStr;

    Spot() : id(0), longitude(0), latitude(0), score(0) {}

    std::string toString() const {
        std::ostringstream oss;
        oss << "[" << id << "] " << name
            << " | " << type
            << " | " << ticketInfo
            << " | " << openingTime
            << " | ★" << std::fixed << std::setprecision(1) << score
            << " | " << address;
        return oss.str();
    }

    bool operator==(const Spot& other) const { return id == other.id; }
    bool operator!=(const Spot& other) const { return id != other.id; }
};

#endif // SPOT_H
