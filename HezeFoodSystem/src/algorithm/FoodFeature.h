#ifndef FOODFEATURE_H
#define FOODFEATURE_H

#include <string>
#include "../model/Food.h"
#include "../model/User.h"
#include "../structure/SeqList.h"

/**
 * 美食特征向量生成器
 *
 * 将 Food 转换为 7 维特征向量，与用户偏好向量维度对齐
 *
 * 向量维度设计（归一化到 0-1）：
 *   [0] priceFeature       价格特征（0=低价，1=高价）
 *   [1] spicyFeature       辣度特征（0=不辣，1=重辣）
 *   [2] sweetFeature       甜度特征（0=不甜，1=重甜）
 *   [3] localFeature       地方特色（0=普通，1=强烈地方特色）
 *   [4] morningFeature     早餐特征（0=非早餐，1=典型早餐）
 *   [5] soupFeature        汤类特征（0=非汤类，1=典型汤类）
 *   [6] scoreFeature       评分特征（0=低分，1=满分）
 */
class FoodFeature {
public:
    static const int DIM = 7;  // 与 PreferenceVector 一致

    /**
     * 将 Food 转换为特征向量
     *
     * 转换规则：
     *   价格: price / 300（归一化到 0-1，300 元为上限）
     *   辣度: 基于标签关键词判断
     *   甜度: 基于标签关键词判断
     *   地方特色: 标签包含"特色"/"地方"等关键词
     *   早餐: 标签包含"早餐"或分类为早餐相关
     *   汤类: 分类为"汤类"
     *   评分: score / 5.0
     *
     * 时间复杂度: O(m)，m 为标签字符串长度
     * 空间复杂度: O(1)
     */
    static PreferenceVector extract(const Food& food) {
        PreferenceVector vec;

        // [0] 价格特征: 归一化到 0-1
        vec[0] = food.price / 300.0;
        if (vec[0] > 1.0) vec[0] = 1.0;

        // [1] 辣度特征: 基于标签判断
        vec[1] = detectSpicy(food.tags, food.name);

        // [2] 甜度特征: 基于标签和分类判断
        vec[2] = detectSweet(food.tags, food.category, food.name);

        // [3] 地方特色: 标签包含"特色"/"地方"/区县名
        vec[3] = detectLocal(food.tags, food.name);

        // [4] 早餐特征
        vec[4] = detectMorning(food.tags, food.category);

        // [5] 汤类特征
        vec[5] = detectSoup(food.tags, food.category, food.name);

        // [6] 评分特征
        vec[6] = food.score / 5.0;

        vec.normalize();
        return vec;
    }

    /**
     * 批量提取特征向量
     */
    static SeqList<PreferenceVector> extractBatch(const SeqList<Food>& foods) {
        SeqList<PreferenceVector> vectors;
        for (int i = 0; i < foods.size(); i++) {
            vectors.push_back(extract(foods[i]));
        }
        return vectors;
    }

private:
    // 辣度检测
    static double detectSpicy(const std::string& tags, const std::string& name) {
        if (tags.find("辣") != std::string::npos) return 0.8;
        if (name.find("胡辣") != std::string::npos) return 0.7;
        if (tags.find("烧烤") != std::string::npos) return 0.5;
        return 0.1;  // 默认低辣度
    }

    // 甜度检测
    static double detectSweet(const std::string& tags, const std::string& category,
                               const std::string& name) {
        if (category == "甜品") return 0.9;
        if (tags.find("甜品") != std::string::npos) return 0.8;
        if (name.find("糕") != std::string::npos) return 0.7;
        if (name.find("饼") != std::string::npos && category == "甜品") return 0.7;
        return 0.1;
    }

    // 地方特色检测
    static double detectLocal(const std::string& tags, const std::string& name) {
        // 区县名关键词
        const std::string districts[] = {
            "单县", "曹县", "郓城", "巨野", "东明", "鄄城", "定陶", "成武", "菏泽"
        };
        for (const auto& d : districts) {
            if (name.find(d) != std::string::npos) return 0.9;
            if (tags.find(d) != std::string::npos) return 0.9;
        }
        if (tags.find("特色") != std::string::npos) return 0.7;
        if (tags.find("地方") != std::string::npos) return 0.7;
        if (tags.find("传统") != std::string::npos) return 0.6;
        if (tags.find("老字号") != std::string::npos) return 0.6;
        return 0.2;
    }

    // 早餐检测
    static double detectMorning(const std::string& tags, const std::string& category) {
        if (tags.find("早餐") != std::string::npos) return 0.9;
        if (category == "汤类") return 0.5;  // 汤类常作为早餐
        if (tags.find("面食") != std::string::npos) return 0.4;
        return 0.1;
    }

    // 汤类检测
    static double detectSoup(const std::string& tags, const std::string& category,
                              const std::string& name) {
        if (category == "汤类") return 0.9;
        if (tags.find("汤品") != std::string::npos) return 0.8;
        if (name.find("汤") != std::string::npos) return 0.8;
        if (name.find("粥") != std::string::npos) return 0.5;
        return 0.1;
    }
};

#endif // FOODFEATURE_H
