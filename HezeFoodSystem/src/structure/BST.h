#ifndef BST_H
#define BST_H

#include "SeqList.h"

/**
 * 二叉搜索树（BST）模板类
 *
 * 业务用途：
 *   - 评分索引：按评分值排序存储 food ID
 *   - 范围查询：查询评分 >= 4.5 的所有美食
 *   - 支持重复键（同一评分可对应多个 food ID）
 *
 * 时间复杂度：
 *   - 插入:     O(log n) 平均，O(n) 最坏（退化为链表）
 *   - 查找:     O(log n) 平均
 *   - 范围查询: O(log n + k)，k 为结果数量
 *   - 删除:     O(log n) 平均
 *
 * 空间复杂度: O(n)
 */
template<typename K, typename V>
class BST {
private:
    struct TreeNode {
        K           key;        // 键（如评分数值）
        SeqList<V>  values;     // 值列表（同一键对应多个 food ID）
        TreeNode*   left;
        TreeNode*   right;

        TreeNode(const K& k, const V& v)
            : left(nullptr), right(nullptr) {
            key = k;
            values.push_back(v);
        }
    };

    TreeNode*   root_;
    int         size_;      // 节点数量

    // 递归插入
    TreeNode* insertNode(TreeNode* node, const K& key, const V& value) {
        if (!node) {
            size_++;
            return new TreeNode(key, value);
        }
        if (key < node->key) {
            node->left = insertNode(node->left, key, value);
        } else if (key > node->key) {
            node->right = insertNode(node->right, key, value);
        } else {
            // 键相同，添加到值列表
            if (node->values.find(value) == -1) {
                node->values.push_back(value);
            }
        }
        return node;
    }

    // 递归查找
    TreeNode* findNode(TreeNode* node, const K& key) const {
        if (!node) return nullptr;
        if (key < node->key) return findNode(node->left, key);
        if (key > node->key) return findNode(node->right, key);
        return node;
    }

    // 范围查询：收集 key >= minKey 的所有值
    void rangeQueryGreater(TreeNode* node, const K& minKey, SeqList<V>& result) const {
        if (!node) return;

        if (node->key >= minKey) {
            // 当前节点满足条件，先遍历左子树（可能还有更小但满足条件的）
            rangeQueryGreater(node->left, minKey, result);
            // 添加当前节点的值
            for (int i = 0; i < node->values.size(); i++) {
                result.push_back(node->values[i]);
            }
            // 遍历右子树（都满足条件）
            rangeQueryGreater(node->right, minKey, result);
        } else {
            // 当前节点不满足条件，只遍历右子树
            rangeQueryGreater(node->right, minKey, result);
        }
    }

    // 范围查询：收集 key <= maxKey 的所有值
    void rangeQueryLess(TreeNode* node, const K& maxKey, SeqList<V>& result) const {
        if (!node) return;

        if (node->key <= maxKey) {
            // 遍历左子树（都满足条件）
            rangeQueryLess(node->left, maxKey, result);
            // 添加当前节点的值
            for (int i = 0; i < node->values.size(); i++) {
                result.push_back(node->values[i]);
            }
            // 遍历右子树
            rangeQueryLess(node->right, maxKey, result);
        } else {
            // 当前节点不满足条件，只遍历左子树
            rangeQueryLess(node->left, maxKey, result);
        }
    }

    // 范围查询：收集 minKey <= key <= maxKey 的所有值
    void rangeQueryBetween(TreeNode* node, const K& minKey, const K& maxKey,
                           SeqList<V>& result) const {
        if (!node) return;

        if (node->key >= minKey) {
            rangeQueryBetween(node->left, minKey, maxKey, result);
        }
        if (node->key >= minKey && node->key <= maxKey) {
            for (int i = 0; i < node->values.size(); i++) {
                result.push_back(node->values[i]);
            }
        }
        if (node->key <= maxKey) {
            rangeQueryBetween(node->right, minKey, maxKey, result);
        }
    }

    // 中序遍历（升序）
    void inorder(TreeNode* node, SeqList<V>& result) const {
        if (!node) return;
        inorder(node->left, result);
        for (int i = 0; i < node->values.size(); i++) {
            result.push_back(node->values[i]);
        }
        inorder(node->right, result);
    }

    // 释放所有节点
    void destroy(TreeNode* node) {
        if (!node) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

public:
    BST() : root_(nullptr), size_(0) {}

    ~BST() {
        destroy(root_);
    }

    /**
     * 插入键值对
     * 时间复杂度: O(log n) 平均
     */
    void insert(const K& key, const V& value) {
        root_ = insertNode(root_, key, value);
    }

    /**
     * 查找指定键
     * 时间复杂度: O(log n) 平均
     * 返回: 对应的值列表
     */
    SeqList<V> search(const K& key) const {
        TreeNode* node = findNode(root_, key);
        if (node) return node->values;
        return SeqList<V>();
    }

    /**
     * 范围查询：key >= minKey
     * 时间复杂度: O(log n + k)
     */
    SeqList<V> rangeGreater(const K& minKey) const {
        SeqList<V> result;
        rangeQueryGreater(root_, minKey, result);
        return result;
    }

    /**
     * 范围查询：key <= maxKey
     * 时间复杂度: O(log n + k)
     */
    SeqList<V> rangeLess(const K& maxKey) const {
        SeqList<V> result;
        rangeQueryLess(root_, maxKey, result);
        return result;
    }

    /**
     * 范围查询：minKey <= key <= maxKey
     * 时间复杂度: O(log n + k)
     */
    SeqList<V> rangeBetween(const K& minKey, const K& maxKey) const {
        SeqList<V> result;
        rangeQueryBetween(root_, minKey, maxKey, result);
        return result;
    }

    /**
     * 获取所有值（升序排列）
     * 时间复杂度: O(n)
     */
    SeqList<V> getAll() const {
        SeqList<V> result;
        inorder(root_, result);
        return result;
    }

    /**
     * 检查是否包含某个键
     */
    bool contains(const K& key) const {
        return findNode(root_, key) != nullptr;
    }

    /**
     * 获取节点数量
     */
    int nodeCount() const { return size_; }

    /**
     * 清空树
     */
    void clear() {
        destroy(root_);
        root_ = nullptr;
        size_ = 0;
    }
};

#endif // BST_H
