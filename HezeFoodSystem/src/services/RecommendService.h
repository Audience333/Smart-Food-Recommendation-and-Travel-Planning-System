#ifndef RECOMMENDSERVICE_H
#define RECOMMENDSERVICE_H

#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>

#include "../model/Food.h"
#include "../model/User.h"
#include "../structure/SeqList.h"
#include "../structure/Heap.h"
#include "../structure/HashTable.h"
#include "../algorithm/FoodFeature.h"
#include "../algorithm/Similarity.h"

/**
 * 推荐结果结构体
 */
struct RecommendItem {
    int         foodId;         // 美食 ID
    std::string foodName;       // 美食名称
    std::string category;       // 分类
    double      price;          // 价格
    double      score;          // 评分
    double      similarity;     // 相似度分数

    RecommendItem()
        : foodId(0), price(0), score(0), similarity(0) {}

    // 用于堆排序（按相似度比较，最大堆）
    bool operator<(const RecommendItem& other) const {
        return similarity < other.similarity;
    }
    bool operator>(const RecommendItem& other) const {
        return similarity > other.similarity;
    }
};

/**
 * 个性化美食推荐服务
 *
 * 推荐流程：
 *   1. 提取用户偏好向量
 *   2. 遍历所有美食，提取特征向量
 *   3. 计算用户向量与每个美食向量的余弦相似度
 *   4. 使用最小堆维护 Top-K 结果
 *   5. 输出推荐列表
 *
 * 偏好更新机制：
 *   - 收藏美食：增加对应特征的权重
 *   - 浏览历史：轻微增加对应特征的权重
 *   - 用户评分：按评分强度调整偏好
 *
 * 数据结构使用：
 *   - PreferenceVector: 7 维向量（数组）
 *   - Heap<RecommendItem>: 最大堆，维护 Top-K
 *   - HashTable: 美食 ID 索引
 *   - SeqList: 存储推荐结果
 */
class RecommendService {
private:
    const SeqList<Food>*   foods_;          // 美食数据（不拥有）
    HashTable<int, int>    idIndex_;         // food ID → 下标
    SeqList<PreferenceVector> foodVectors_;  // 美食特征向量缓存

    /**
     * 获取美食对象
     */
    const Food* getFoodById(int id) const {
        if (idIndex_.contains(id)) {
            return &((*foods_)[idIndex_.get(id)]);
        }
        return nullptr;
    }

public:
    RecommendService() : foods_(nullptr) {}

    /**
     * 初始化推荐服务
     * 预计算所有美食的特征向量
     *
     * 时间复杂度: O(n)，n 为美食数量
     * 空间复杂度: O(n)
     */
    void init(const SeqList<Food>& foods) {
        foods_ = &foods;
        idIndex_.clear();
        foodVectors_.clear();

        for (int i = 0; i < foods.size(); i++) {
            idIndex_.put(foods[i].id, i);
            foodVectors_.push_back(FoodFeature::extract(foods[i]));
        }

        std::cout << "[RecommendService] 初始化完成，缓存 "
                  << foodVectors_.size() << " 个特征向量" << std::endl;
    }

    // ==================== 偏好更新 ====================

    /**
     * 根据收藏更新用户偏好
     *
     * 策略：
     *   - 收藏的美食特征向量 * 0.3 加到用户偏好上
     *   - 然后归一化
     *
     * 时间复杂度: O(d)，d 为向量维度
     */
    void updatePreferenceByFavorite(User& user, int foodId) {
        if (!idIndex_.contains(foodId)) return;
        int idx = idIndex_.get(foodId);
        const PreferenceVector& foodVec = foodVectors_[idx];

        // 收藏权重
        const double weight = 0.3;

        for (int i = 0; i < PreferenceVector::DIM; i++) {
            user.preference[i] += weight * foodVec[i];
        }
        user.preference.normalize();
    }

    /**
     * 根据浏览历史更新偏好（权重较轻）
     *
     * 时间复杂度: O(d)
     */
    void updatePreferenceByHistory(User& user, int foodId) {
        if (!idIndex_.contains(foodId)) return;
        int idx = idIndex_.get(foodId);
        const PreferenceVector& foodVec = foodVectors_[idx];

        // 浏览权重（比收藏轻）
        const double weight = 0.1;

        for (int i = 0; i < PreferenceVector::DIM; i++) {
            user.preference[i] += weight * foodVec[i];
        }
        user.preference.normalize();
    }

    /**
     * 根据评分更新偏好
     *
     * 策略：
     *   - 高评分（>=4）正向调整
     *   - 低评分（<=2）负向调整
     *
     * 时间复杂度: O(d)
     */
    void updatePreferenceByRating(User& user, int foodId, double rating) {
        if (!idIndex_.contains(foodId)) return;
        int idx = idIndex_.get(foodId);
        const PreferenceVector& foodVec = foodVectors_[idx];

        double weight = 0;
        if (rating >= 4.0) weight = 0.2;
        else if (rating >= 3.0) weight = 0.05;
        else if (rating <= 2.0) weight = -0.1;

        for (int i = 0; i < PreferenceVector::DIM; i++) {
            user.preference[i] += weight * foodVec[i];
        }
        user.preference.normalize();
    }

    /**
     * 从用户历史行为批量更新偏好
     *
     * 时间复杂度: O(k * d)，k 为历史记录数
     */
    void rebuildPreference(User& user) {
        // 重置偏好为默认值
        user.preference = PreferenceVector();

        // 从收藏学习
        for (int i = 0; i < user.favoriteFoodIds.size(); i++) {
            updatePreferenceByFavorite(user, user.favoriteFoodIds[i]);
        }

        // 从浏览历史学习（最近的权重更高，但这里简化处理）
        for (int i = 0; i < user.historyFoodIds.size(); i++) {
            updatePreferenceByHistory(user, user.historyFoodIds[i]);
        }

        // 从评分学习
        for (int i = 0; i < user.ratedFoodIds.size(); i++) {
            updatePreferenceByRating(user, user.ratedFoodIds[i], user.ratings[i]);
        }
    }

    // ==================== 推荐功能 ====================

    /**
     * 基于用户偏好推荐 Top-K 美食
     *
     * 算法流程：
     *   1. 获取用户偏好向量
     *   2. 遍历所有美食，计算相似度
     *   3. 使用最大堆维护 Top-K
     *   4. 从堆中提取结果（降序）
     *
     * 时间复杂度: O(n * d + n * log k)
     *   n = 美食数量，d = 向量维度，k = 推荐数量
     * 空间复杂度: O(k)
     */
    SeqList<RecommendItem> recommendByPreference(const PreferenceVector& pref,
                                                  int k = 10) const {
        if (!foods_ || foods_->empty()) return SeqList<RecommendItem>();

        // 使用最大堆维护 Top-K
        // 由于我们要保留最大的 k 个，使用最小堆更高效
        // 当堆大小 < k 时直接插入
        // 当新元素 > 堆顶时，替换堆顶
        Heap<RecommendItem> minHeap(true);  // 最小堆

        for (int i = 0; i < foods_->size(); i++) {
            double sim = Similarity::cosineSimilarity(pref, foodVectors_[i]);

            RecommendItem item;
            item.foodId = (*foods_)[i].id;
            item.foodName = (*foods_)[i].name;
            item.category = (*foods_)[i].category;
            item.price = (*foods_)[i].price;
            item.score = (*foods_)[i].score;
            item.similarity = sim;

            if (minHeap.size() < k) {
                minHeap.push(item);
            } else if (sim > minHeap.top().similarity) {
                minHeap.pop();
                minHeap.push(item);
            }
        }

        // 从堆中提取结果（最小堆输出是升序，需要反转）
        SeqList<RecommendItem> results;
        while (!minHeap.empty()) {
            results.push_back(minHeap.top());
            minHeap.pop();
        }
        // 反转为降序
        for (int i = 0; i < results.size() / 2; i++) {
            RecommendItem temp = results[i];
            results[i] = results[results.size() - 1 - i];
            results[results.size() - 1 - i] = temp;
        }

        return results;
    }

    /**
     * 基于用户对象推荐
     *
     * 时间复杂度: O(n * d + n * log k)
     */
    SeqList<RecommendItem> recommendByUser(const User& user, int k = 10) const {
        return recommendByPreference(user.preference, k);
    }

    /**
     * 基于指定美食推荐相似美食
     *
     * 时间复杂度: O(n * d + n * log k)
     */
    SeqList<RecommendItem> recommendSimilar(int foodId, int k = 5) const {
        if (!idIndex_.contains(foodId)) return SeqList<RecommendItem>();

        int idx = idIndex_.get(foodId);
        const PreferenceVector& foodVec = foodVectors_[idx];

        Heap<RecommendItem> minHeap(true);

        for (int i = 0; i < foods_->size(); i++) {
            if ((*foods_)[i].id == foodId) continue;  // 跳过自身

            double sim = Similarity::cosineSimilarity(foodVec, foodVectors_[i]);

            RecommendItem item;
            item.foodId = (*foods_)[i].id;
            item.foodName = (*foods_)[i].name;
            item.category = (*foods_)[i].category;
            item.price = (*foods_)[i].price;
            item.score = (*foods_)[i].score;
            item.similarity = sim;

            if (minHeap.size() < k) {
                minHeap.push(item);
            } else if (sim > minHeap.top().similarity) {
                minHeap.pop();
                minHeap.push(item);
            }
        }

        SeqList<RecommendItem> results;
        while (!minHeap.empty()) {
            results.push_back(minHeap.top());
            minHeap.pop();
        }
        for (int i = 0; i < results.size() / 2; i++) {
            RecommendItem temp = results[i];
            results[i] = results[results.size() - 1 - i];
            results[results.size() - 1 - i] = temp;
        }
        return results;
    }

    // ==================== 输出 ====================

    /**
     * 打印推荐结果
     */
    static void printResults(const SeqList<RecommendItem>& results,
                              const std::string& title = "推荐结果") {
        std::cout << std::endl << "=== " << title << " ===" << std::endl;
        if (results.empty()) {
            std::cout << "  无推荐结果" << std::endl;
            return;
        }

        std::cout << "  " << std::left
                  << std::setw(4) << "排名"
                  << std::setw(16) << "名称"
                  << std::setw(8) << "分类"
                  << std::setw(10) << "价格"
                  << std::setw(8) << "评分"
                  << "相似度" << std::endl;
        std::cout << "  " << std::string(65, '-') << std::endl;

        for (int i = 0; i < results.size(); i++) {
            const RecommendItem& item = results[i];
            std::cout << "  " << std::left
                      << std::setw(4) << (i + 1)
                      << std::setw(16) << item.foodName
                      << std::setw(8) << item.category
                      << "¥" << std::setw(9) << std::fixed << std::setprecision(0) << item.price
                      << "★" << std::setw(7) << std::fixed << std::setprecision(1) << item.score
                      << std::fixed << std::setprecision(4) << item.similarity
                      << std::endl;
        }
        std::cout << std::endl;
    }

    /**
     * 打印用户偏好向量
     */
    static void printUserPreference(const User& user) {
        std::cout << "用户偏好向量 [" << user.username << "]:" << std::endl;
        std::cout << "  价格偏好:   " << std::fixed << std::setprecision(2)
                  << user.preference[0] << std::endl;
        std::cout << "  辣度偏好:   " << user.preference[1] << std::endl;
        std::cout << "  甜度偏好:   " << user.preference[2] << std::endl;
        std::cout << "  地方特色:   " << user.preference[3] << std::endl;
        std::cout << "  早餐偏好:   " << user.preference[4] << std::endl;
        std::cout << "  汤类偏好:   " << user.preference[5] << std::endl;
        std::cout << "  评分偏好:   " << user.preference[6] << std::endl;
    }
};

#endif // RECOMMENDSERVICE_H
