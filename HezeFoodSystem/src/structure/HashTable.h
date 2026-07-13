#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdexcept>
#include <string>

/**
 * 哈希表模板类（开链法）
 * 手写实现，替代 std::unordered_map
 *
 * 业务用途：
 *   - 美食/景点 ID 索引（O(1) 查找）
 *   - 分类索引（按 category 快速筛选）
 *   - 标签倒排索引
 *   - 名称→下标映射
 *
 * 时间复杂度：
 *   - 插入 O(1) 均摊
 *   - 查找 O(1) 均摊
 *   - 删除 O(1) 均摊
 *
 * 冲突解决：开链法（每个桶位维护一个链表）
 */
template<typename K, typename V>
class HashTable {
private:
    // 键值对节点
    struct KVNode {
        K       key;
        V       value;
        KVNode* next;
        KVNode(const K& k, const V& v, KVNode* n = nullptr)
            : key(k), value(v), next(n) {}
    };

    KVNode**    buckets_;       // 桶数组
    int         bucketCount_;   // 桶数量
    int         size_;          // 元素总数
    double      loadFactor_;    // 负载因子阈值

    // 哈希函数（通用版，对 int/string 等类型特化在下方）
    int hash(const K& key) const {
        return static_cast<int>(
            static_cast<size_t>(key) % bucketCount_
        );
    }

    // 扩容
    void rehash() {
        int newBucketCount = bucketCount_ * 2;
        KVNode** newBuckets = new KVNode*[newBucketCount]();
        for (int i = 0; i < bucketCount_; i++) {
            KVNode* p = buckets_[i];
            while (p) {
                KVNode* next = p->next;
                int idx = static_cast<int>(
                    static_cast<size_t>(p->key) % newBucketCount
                );
                p->next = newBuckets[idx];
                newBuckets[idx] = p;
                p = next;
            }
        }
        delete[] buckets_;
        buckets_ = newBuckets;
        bucketCount_ = newBucketCount;
    }

public:
    explicit HashTable(int initBucketCount = 16, double lf = 0.75)
        : bucketCount_(initBucketCount), size_(0), loadFactor_(lf) {
        buckets_ = new KVNode*[bucketCount_]();
    }

    ~HashTable() {
        clear();
        delete[] buckets_;
    }

    // ---- 容量 ----
    int  size()       const { return size_; }
    bool empty()      const { return size_ == 0; }
    int  bucketCount() const { return bucketCount_; }

    // ---- 插入/更新 ----
    void put(const K& key, const V& value) {
        if (static_cast<double>(size_ + 1) / bucketCount_ > loadFactor_) {
            rehash();
        }
        int idx = hash(key);
        // 查找是否已存在
        for (KVNode* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) {
                p->value = value;  // 更新
                return;
            }
        }
        // 头插法
        buckets_[idx] = new KVNode(key, value, buckets_[idx]);
        size_++;
    }

    // ---- 查找 ----
    V& get(const K& key) {
        int idx = hash(key);
        for (KVNode* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) return p->value;
        }
        throw std::out_of_range("HashTable: key not found");
    }

    const V& get(const K& key) const {
        int idx = hash(key);
        for (KVNode* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) return p->value;
        }
        throw std::out_of_range("HashTable: key not found");
    }

    // 检查是否存在
    bool contains(const K& key) const {
        int idx = hash(key);
        for (KVNode* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) return true;
        }
        return false;
    }

    // operator[] 便捷访问
    V& operator[](const K& key) {
        int idx = hash(key);
        for (KVNode* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) return p->value;
        }
        // 不存在则插入默认值
        if (static_cast<double>(size_ + 1) / bucketCount_ > loadFactor_) {
            rehash();
            idx = hash(key);
        }
        V defaultVal = V();
        buckets_[idx] = new KVNode(key, defaultVal, buckets_[idx]);
        size_++;
        return buckets_[idx]->value;
    }

    // ---- 删除 ----
    bool remove(const K& key) {
        int idx = hash(key);
        KVNode* prev = nullptr;
        for (KVNode* p = buckets_[idx]; p; p = p->next) {
            if (p->key == key) {
                if (prev) prev->next = p->next;
                else      buckets_[idx] = p->next;
                delete p;
                size_--;
                return true;
            }
            prev = p;
        }
        return false;
    }

    void clear() {
        for (int i = 0; i < bucketCount_; i++) {
            KVNode* p = buckets_[i];
            while (p) {
                KVNode* del = p;
                p = p->next;
                delete del;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }

    // ---- 遍历支持 ----
    // 获取所有键
    // 注意：需要 K 类型支持默认构造
    // 此函数仅用于调试和展示
    struct Entry {
        K key;
        V value;
    };

    // 获取桶数组指针（供外部遍历）
    KVNode** rawBuckets() const { return buckets_; }
};

// ---- string 类型的哈希特化 ----
// 使用 FNV-1a 哈希算法
template<>
inline int HashTable<std::string, int>::hash(const std::string& key) const {
    unsigned long long h = 14695981039346656037ULL;
    for (char c : key) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ULL;
    }
    return static_cast<int>(h % bucketCount_);
}

#endif // HASHTABLE_H
