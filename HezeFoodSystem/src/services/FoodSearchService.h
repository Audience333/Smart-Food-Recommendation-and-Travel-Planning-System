#ifndef FOODSEARCHSERVICE_H
#define FOODSEARCHSERVICE_H

#include <string>
#include <iostream>
#include <iomanip>
#include "../model/Food.h"
#include "../structure/SeqList.h"
#include "../structure/Trie.h"
#include "../structure/InvertedIndex.h"
#include "../structure/BST.h"
#include "../structure/HashTable.h"

/**
 * 美食智能搜索服务
 *
 * 集成三种索引结构：
 *   1. Trie 树    — 名称前缀搜索
 *   2. 倒排索引   — 标签搜索
 *   3. BST 树     — 评分范围查询
 *
 * 搜索流程：
 *   用户输入 → 选择搜索方式 → 查询对应索引 → 返回 food ID → 映射到 Food 对象
 */
class FoodSearchService {
private:
    const SeqList<Food>*   foods_;          // 美食数据指针（不拥有）
    HashTable<int, int>    idIndex_;         // food ID → foods_ 下标

    Trie                   trie_;            // 名称前缀索引
    InvertedIndex          tagIndex_;        // 标签倒排索引
    BST<double, int>       scoreIndex_;      // 评分 BST 索引

    bool                   built_;           // 索引是否已构建

    // 获取 food ID 对应的 Food 对象
    const Food* getFoodById(int id) const {
        if (idIndex_.contains(id)) {
            int idx = idIndex_.get(id);
            return &((*foods_)[idx]);
        }
        return nullptr;
    }

    // 将 ID 列表转换为 Food 列表
    SeqList<const Food*> idsToFoods(const SeqList<int>& ids) const {
        SeqList<const Food*> result;
        for (int i = 0; i < ids.size(); i++) {
            const Food* f = getFoodById(ids[i]);
            if (f) result.push_back(f);
        }
        return result;
    }

public:
    FoodSearchService() : foods_(nullptr), built_(false) {}

    /**
     * 构建索引
     * 时间复杂度: O(n * (m + t))
     *   n = 美食数量，m = 平均名称长度，t = 平均标签数
     */
    void build(const SeqList<Food>& foods) {
        foods_ = &foods;
        built_ = true;

        std::cout << "[FoodSearchService] 构建索引..." << std::endl;

        for (int i = 0; i < foods.size(); i++) {
            const Food& f = foods[i];

            // 1. ID 索引
            idIndex_.put(f.id, i);

            // 2. Trie 索引（名称）
            trie_.insert(f.name, f.id);

            // 3. 倒排索引（标签 + 分类）
            tagIndex_.insertTags(f.tags, f.id);
            tagIndex_.insertTag(f.category, f.id);

            // 4. BST 索引（评分）
            scoreIndex_.insert(f.score, f.id);
        }

        std::cout << "[FoodSearchService] 索引构建完成" << std::endl;
        std::cout << "  美食数量: " << foods.size() << std::endl;
        std::cout << "  标签数量: " << tagIndex_.tagCount() << std::endl;
        std::cout << "  评分节点: " << scoreIndex_.nodeCount() << std::endl;
    }

    // ==================== 搜索功能 ====================

    /**
     * 1. 名称前缀搜索
     *
     * 流程: 用户输入前缀 → Trie.searchPrefix → 返回匹配 food ID → 映射到 Food
     *
     * 时间复杂度: O(m + k)，m 为前缀长度，k 为结果数
     */
    SeqList<const Food*> searchByName(const std::string& prefix) const {
        if (!built_) return SeqList<const Food*>();
        SeqList<int> ids = trie_.searchPrefix(prefix);
        return idsToFoods(ids);
    }

    /**
     * 2. 标签搜索
     *
     * 流程: 用户输入标签 → InvertedIndex.searchByTag → 返回匹配 food ID
     *
     * 时间复杂度: O(1) 均摊
     */
    SeqList<const Food*> searchByTag(const std::string& tag) const {
        if (!built_) return SeqList<const Food*>();
        SeqList<int> ids = tagIndex_.searchByTag(tag);
        return idsToFoods(ids);
    }

    /**
     * 3. 评分筛选（最低评分）
     *
     * 流程: 用户输入最低评分 → BST.rangeGreater → 返回满足条件的 food ID
     *
     * 时间复杂度: O(log n + k)
     */
    SeqList<const Food*> searchByMinScore(double minScore) const {
        if (!built_) return SeqList<const Food*>();
        SeqList<int> ids = scoreIndex_.rangeGreater(minScore);
        return idsToFoods(ids);
    }

    /**
     * 4. 评分范围查询
     *
     * 时间复杂度: O(log n + k)
     */
    SeqList<const Food*> searchByScoreRange(double minScore, double maxScore) const {
        if (!built_) return SeqList<const Food*>();
        SeqList<int> ids = scoreIndex_.rangeBetween(minScore, maxScore);
        return idsToFoods(ids);
    }

    /**
     * 5. 多标签组合搜索（交集）
     *
     * 流程: 多个标签 → InvertedIndex.searchAll → 取交集
     *
     * 时间复杂度: O(min_set * num_tags)
     */
    SeqList<const Food*> searchByAllTags(const SeqList<std::string>& tags) const {
        if (!built_) return SeqList<const Food*>();
        SeqList<int> ids = tagIndex_.searchAll(tags);
        return idsToFoods(ids);
    }

    /**
     * 6. 组合搜索：标签 + 最低评分
     *
     * 流程: 标签搜索 ∩ 评分筛选
     *
     * 时间复杂度: O(min(tag_result, score_result))
     */
    SeqList<const Food*> searchByTagAndScore(const std::string& tag, double minScore) const {
        if (!built_) return SeqList<const Food*>();

        SeqList<int> tagIds = tagIndex_.searchByTag(tag);
        SeqList<int> scoreIds = scoreIndex_.rangeGreater(minScore);

        // 取交集
        SeqList<int> result;
        for (int i = 0; i < tagIds.size(); i++) {
            if (scoreIds.find(tagIds[i]) != -1) {
                result.push_back(tagIds[i]);
            }
        }
        return idsToFoods(result);
    }

    /**
     * 7. 组合搜索：名称前缀 + 标签
     */
    SeqList<const Food*> searchByNameAndTag(const std::string& prefix,
                                             const std::string& tag) const {
        if (!built_) return SeqList<const Food*>();

        SeqList<int> nameIds = trie_.searchPrefix(prefix);
        SeqList<int> tagIds = tagIndex_.searchByTag(tag);

        SeqList<int> result;
        for (int i = 0; i < nameIds.size(); i++) {
            if (tagIds.find(nameIds[i]) != -1) {
                result.push_back(nameIds[i]);
            }
        }
        return idsToFoods(result);
    }

    // ==================== 辅助功能 ====================

    /**
     * 获取所有标签
     */
    SeqList<std::string> getAllTags() const {
        return tagIndex_.getAllTags();
    }

    /**
     * 获取搜索结果的格式化输出
     */
    static void printResults(const SeqList<const Food*>& results,
                              const std::string& title = "搜索结果") {
        std::cout << std::endl << "=== " << title << " ===" << std::endl;
        if (results.empty()) {
            std::cout << "  未找到匹配的美食" << std::endl;
            return;
        }
        std::cout << "  找到 " << results.size() << " 条结果:" << std::endl;
        std::cout << "  " << std::string(70, '-') << std::endl;

        for (int i = 0; i < results.size(); i++) {
            const Food* f = results[i];
            std::cout << "  " << std::left << std::setw(4) << (i + 1)
                      << std::setw(16) << f->name
                      << std::setw(8) << f->category
                      << "¥" << std::setw(7) << std::fixed << std::setprecision(0) << f->price
                      << "★" << std::setw(5) << std::fixed << std::setprecision(1) << f->score
                      << f->tags << std::endl;
        }
        std::cout << std::endl;
    }
};

#endif // FOODSEARCHSERVICE_H
