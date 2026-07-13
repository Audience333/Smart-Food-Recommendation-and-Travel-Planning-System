#ifndef USER_H
#define USER_H

#include <string>
#include <sstream>
#include "../structure/SeqList.h"

/**
 * 用户偏好向量（7 维）
 *
 * 维度设计（归一化到 0-1）：
 *   [0] pricePreference    价格偏好（0=低价，1=高价）
 *   [1] spicyPreference    辣度偏好（0=不辣，1=重辣）
 *   [2] sweetPreference    甜度偏好（0=不甜，1=重甜）
 *   [3] localPreference    地方特色偏好（0=普通，1=强烈偏好地方特色）
 *   [4] morningPreference  早餐偏好（0=不在意，1=强烈偏好早餐类）
 *   [5] soupPreference     汤类偏好（0=不在意，1=强烈偏好汤类）
 *   [6] scorePreference    评分偏好（0=不在意评分，1=只要高分）
 */
struct PreferenceVector {
    static const int DIM = 7;  // 向量维度

    double data[DIM];

    PreferenceVector() {
        for (int i = 0; i < DIM; i++) data[i] = 0.5;  // 默认中等偏好
    }

    // 按下标访问
    double& operator[](int idx) { return data[idx]; }
    double  operator[](int idx) const { return data[idx]; }

    // 归一化到 [0, 1]
    void normalize() {
        for (int i = 0; i < DIM; i++) {
            if (data[i] < 0) data[i] = 0;
            if (data[i] > 1) data[i] = 1;
        }
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "[";
        for (int i = 0; i < DIM; i++) {
            if (i > 0) oss << ", ";
            oss << data[i];
        }
        oss << "]";
        return oss.str();
    }
};

/**
 * 用户数据结构体
 *
 * 包含：
 *   - 基本信息（id, username）
 *   - 偏好向量（用于推荐算法）
 *   - 收藏列表（用于偏好更新）
 *   - 浏览历史（用于偏好更新）
 */
struct User {
    int                 id;                 // 用户 ID
    std::string         username;           // 用户名
    PreferenceVector    preference;         // 偏好向量（7维）
    SeqList<int>        favoriteFoodIds;    // 收藏的美食 ID 列表
    SeqList<int>        historyFoodIds;     // 浏览过的美食 ID 列表
    SeqList<int>        ratedFoodIds;       // 已评分的美食 ID 列表
    SeqList<double>     ratings;            // 对应的评分

    User() : id(0) {}

    /**
     * 添加收藏
     * @return 是否成功添加（避免重复）
     */
    bool addFavorite(int foodId) {
        if (favoriteFoodIds.find(foodId) != -1) return false;
        favoriteFoodIds.push_back(foodId);
        return true;
    }

    /**
     * 移除收藏
     * @return 是否成功移除
     */
    bool removeFavorite(int foodId) {
        return favoriteFoodIds.removeIf(
            [foodId](const int& id) { return id == foodId; }
        );
    }

    /**
     * 是否已收藏
     */
    bool isFavorite(int foodId) const {
        return favoriteFoodIds.find(foodId) != -1;
    }

    /**
     * 添加浏览记录
     */
    void addHistory(int foodId) {
        // 避免连续重复（但允许重复浏览）
        historyFoodIds.push_back(foodId);
    }

    /**
     * 添加评分
     */
    void addRating(int foodId, double rating) {
        // 如果已评分，更新评分
        for (int i = 0; i < ratedFoodIds.size(); i++) {
            if (ratedFoodIds[i] == foodId) {
                ratings[i] = rating;
                return;
            }
        }
        ratedFoodIds.push_back(foodId);
        ratings.push_back(rating);
    }

    /**
     * 获取对某美食的评分
     * @return 评分，未评分返回 -1
     */
    double getRating(int foodId) const {
        for (int i = 0; i < ratedFoodIds.size(); i++) {
            if (ratedFoodIds[i] == foodId) return ratings[i];
        }
        return -1;
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "用户[" << id << "] " << username
            << " | 收藏: " << favoriteFoodIds.size()
            << " | 浏览: " << historyFoodIds.size()
            << " | 偏好: " << preference.toString();
        return oss.str();
    }

    bool operator==(const User& other) const { return id == other.id; }
    bool operator!=(const User& other) const { return id != other.id; }
};

#endif // USER_H
