#ifndef FOOD_H
#define FOOD_H

#include <string>
#include <sstream>
#include <iomanip>

/**
 * 美食数据结构体
 *
 * 对应数据文件: data/food.txt
 * 文件格式: id|name|longitude|latitude|price|score|category|tags
 * 示例: 1|单县羊肉汤|115.94|35.06|35|4.8|汤类|早餐,地方特色
 *
 * 存储方式: SeqList<Food>
 * 索引方式: HashTable<int, int>  (id → 下标)
 */
struct Food {
    int         id;             // 唯一标识
    std::string name;           // 美食名称
    double      longitude;      // 经度
    double      latitude;       // 纬度
    double      price;          // 参考价格（元）
    double      score;          // 评分 (1.0 - 5.0)
    std::string category;       // 分类（汤类/小吃/面食/正餐/甜品/饮品/凉菜/烧烤）
    std::string tags;           // 标签，逗号分隔（如: 早餐,地方特色,传统）

    // 默认构造
    Food() : id(0), longitude(0), latitude(0), price(0), score(0) {}

    // 带参构造
    Food(int id, const std::string& name, double lng, double lat,
         double price, double score, const std::string& cat, const std::string& tags)
        : id(id), name(name), longitude(lng), latitude(lat),
          price(price), score(score), category(cat), tags(tags) {}

    // 获取价格等级 (1-5)
    int priceLevel() const {
        if (price <= 10)  return 1;
        if (price <= 25)  return 2;
        if (price <= 50)  return 3;
        if (price <= 100) return 4;
        return 5;
    }

    // 格式化显示
    std::string toString() const {
        std::ostringstream oss;
        oss << "[" << id << "] " << name
            << " | " << category
            << " | ¥" << std::fixed << std::setprecision(0) << price
            << " | ★" << std::fixed << std::setprecision(1) << score
            << " | (" << std::fixed << std::setprecision(4) << longitude
            << ", " << latitude << ")"
            << " | 标签: " << tags;
        return oss.str();
    }

    // 判断相等（按 ID）
    bool operator==(const Food& other) const { return id == other.id; }
    bool operator!=(const Food& other) const { return id != other.id; }
};

#endif // FOOD_H
