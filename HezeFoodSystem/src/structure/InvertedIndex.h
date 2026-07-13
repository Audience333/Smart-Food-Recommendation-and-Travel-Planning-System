#ifndef INVERTEDINDEX_H
#define INVERTEDINDEX_H

#include <string>
#include "HashTable.h"
#include "SeqList.h"

/**
 * 倒排索引（基于自实现 HashTable）
 *
 * 数据结构：
 *   HashTable<string, SeqList<int>>
 *   键 = 标签字符串
 *   值 = 包含该标签的美食 ID 列表
 *
 * 业务用途：
 *   - 标签搜索：输入"早餐"返回所有早餐美食
 *   - 分类筛选：输入"汤类"返回所有汤类美食
 *   - 组合查询：多个标签取交集
 *
 * 时间复杂度：
 *   - insertTag:    O(1) 均摊（哈希插入）
 *   - searchByTag:  O(1) 均摊（哈希查找）
 *   - searchAll:    O(min_set * num_tags)（集合交集）
 *   - searchAny:    O(max_set * num_tags)（集合并集）
 *
 * 空间复杂度：O(T * L)，T 为标签数，L 为平均每标签关联的 ID 数
 */
class InvertedIndex {
private:
    HashTable<std::string, SeqList<int>> index_;

public:
    InvertedIndex() {}

    /**
     * 为某个 food ID 插入标签
     * 时间复杂度: O(1) 均摊
     *
     * @param tag 标签字符串
     * @param foodId 美食 ID
     */
    void insertTag(const std::string& tag, int foodId) {
        if (!index_.contains(tag)) {
            index_.put(tag, SeqList<int>());
        }
        SeqList<int>& ids = index_.get(tag);
        // 避免重复插入
        if (ids.find(foodId) == -1) {
            ids.push_back(foodId);
        }
    }

    /**
     * 批量为某个 food ID 插入多个标签
     * 时间复杂度: O(k)，k 为标签数量
     *
     * @param tags 逗号分隔的标签字符串（如 "早餐,地方特色,汤品"）
     * @param foodId 美食 ID
     */
    void insertTags(const std::string& tags, int foodId) {
        // 手动解析逗号分隔的标签
        std::string current;
        for (size_t i = 0; i <= tags.length(); i++) {
            if (i == tags.length() || tags[i] == ',') {
                if (!current.empty()) {
                    insertTag(current, foodId);
                    current.clear();
                }
            } else {
                current += tags[i];
            }
        }
    }

    /**
     * 根据单个标签搜索
     * 时间复杂度: O(1) 均摊
     *
     * @param tag 标签
     * @return 包含该标签的 food ID 列表
     */
    SeqList<int> searchByTag(const std::string& tag) const {
        if (index_.contains(tag)) {
            return index_.get(tag);
        }
        return SeqList<int>();
    }

    /**
     * 多标签交集搜索（必须同时包含所有标签）
     * 时间复杂度: O(min_set_size * num_tags)
     *
     * @param tags 标签列表
     * @return 同时包含所有标签的 food ID 列表
     */
    SeqList<int> searchAll(const SeqList<std::string>& tags) const {
        if (tags.empty()) return SeqList<int>();

        // 获取第一个标签的结果作为基准
        SeqList<int> result = searchByTag(tags[0]);

        // 与后续标签结果取交集
        for (int i = 1; i < tags.size(); i++) {
            SeqList<int> tagResult = searchByTag(tags[i]);
            SeqList<int> intersection;
            for (int j = 0; j < result.size(); j++) {
                if (tagResult.find(result[j]) != -1) {
                    intersection.push_back(result[j]);
                }
            }
            result = intersection;
        }
        return result;
    }

    /**
     * 多标签并集搜索（包含任一标签即可）
     * 时间复杂度: O(max_set_size * num_tags)
     *
     * @param tags 标签列表
     * @return 包含任一标签的 food ID 列表
     */
    SeqList<int> searchAny(const SeqList<std::string>& tags) const {
        SeqList<int> result;
        for (int i = 0; i < tags.size(); i++) {
            SeqList<int> tagResult = searchByTag(tags[i]);
            for (int j = 0; j < tagResult.size(); j++) {
                if (result.find(tagResult[j]) == -1) {
                    result.push_back(tagResult[j]);
                }
            }
        }
        return result;
    }

    /**
     * 获取所有标签
     */
    SeqList<std::string> getAllTags() const {
        SeqList<std::string> tags;
        // 遍历哈希表的所有桶
        auto buckets = index_.rawBuckets();
        for (int i = 0; i < index_.bucketCount(); i++) {
            auto* p = buckets[i];
            while (p) {
                tags.push_back(p->key);
                p = p->next;
            }
        }
        return tags;
    }

    /**
     * 检查标签是否存在
     */
    bool hasTag(const std::string& tag) const {
        return index_.contains(tag);
    }

    /**
     * 获取标签数量
     */
    int tagCount() const {
        return index_.size();
    }

    /**
     * 清空索引
     */
    void clear() {
        // HashTable 没有 clear 方法，重新构造
        index_ = HashTable<std::string, SeqList<int>>();
    }
};

#endif // INVERTEDINDEX_H
