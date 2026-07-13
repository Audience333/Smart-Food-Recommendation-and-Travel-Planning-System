#ifndef SIMILARITY_H
#define SIMILARITY_H

#include <cmath>
#include "../model/User.h"

/**
 * 相似度计算算法
 *
 * 实现余弦相似度、欧氏距离等算法
 *
 * 余弦相似度公式：
 *   cos(A, B) = (A · B) / (|A| × |B|)
 *   其中：
 *     A · B = Σ(A[i] × B[i])         // 点积
 *     |A|   = √(Σ A[i]²)             // 向量模长
 *
 * 取值范围：[0, 1]
 *   1 表示完全匹配
 *   0 表示完全不相关
 *
 * 时间复杂度: O(d)，d 为向量维度
 * 空间复杂度: O(1)
 */
class Similarity {
public:
    /**
     * 余弦相似度
     *
     * @param a 向量 A
     * @param b 向量 B
     * @return 相似度 [0, 1]
     *
     * 时间复杂度: O(d)，d = PreferenceVector::DIM = 7
     * 空间复杂度: O(1)
     */
    static double cosineSimilarity(const PreferenceVector& a,
                                    const PreferenceVector& b) {
        const int d = PreferenceVector::DIM;
        double dotProduct = 0.0;   // A · B
        double normA = 0.0;        // |A|²
        double normB = 0.0;        // |B|²

        for (int i = 0; i < d; i++) {
            dotProduct += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }

        // 避免除零
        if (normA < 1e-10 || normB < 1e-10) return 0.0;

        return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
    }

    /**
     * 加权余弦相似度
     *
     * 对不同维度赋予不同权重，突出用户关注的维度
     *
     * @param a 向量 A
     * @param b 向量 B
     * @param weights 权重向量
     * @return 加权相似度 [0, 1]
     *
     * 时间复杂度: O(d)
     * 空间复杂度: O(1)
     */
    static double weightedCosineSimilarity(const PreferenceVector& a,
                                            const PreferenceVector& b,
                                            const PreferenceVector& weights) {
        const int d = PreferenceVector::DIM;
        double dotProduct = 0.0;
        double normA = 0.0;
        double normB = 0.0;

        for (int i = 0; i < d; i++) {
            double w = weights[i];
            dotProduct += w * a[i] * b[i];
            normA += w * a[i] * a[i];
            normB += w * b[i] * b[i];
        }

        if (normA < 1e-10 || normB < 1e-10) return 0.0;

        return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
    }

    /**
     * 欧氏距离
     *
     * @param a 向量 A
     * @param b 向量 B
     * @return 距离值（越小越相似）
     *
     * 时间复杂度: O(d)
     * 空间复杂度: O(1)
     */
    static double euclideanDistance(const PreferenceVector& a,
                                     const PreferenceVector& b) {
        const int d = PreferenceVector::DIM;
        double sum = 0.0;

        for (int i = 0; i < d; i++) {
            double diff = a[i] - b[i];
            sum += diff * diff;
        }

        return std::sqrt(sum);
    }

    /**
     * 欧氏距离转换为相似度
     *
     * similarity = 1 / (1 + distance)
     *
     * @return 相似度 [0, 1]
     */
    static double euclideanSimilarity(const PreferenceVector& a,
                                       const PreferenceVector& b) {
        double dist = euclideanDistance(a, b);
        return 1.0 / (1.0 + dist);
    }
};

#endif // SIMILARITY_H
