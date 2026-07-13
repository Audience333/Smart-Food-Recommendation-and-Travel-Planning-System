#ifndef TRIE_H
#define TRIE_H

#include <string>
#include "SeqList.h"

/**
 * Trie 树（前缀树）- 支持中文字符
 *
 * 实现方式：
 *   每个节点使用链表存储子节点（避免使用 map/unordered_map）
 *   支持 UTF-8 中文字符（按字节遍历）
 *
 * 业务用途：
 *   - 美食名称前缀搜索
 *   - 输入"单"返回所有以"单"开头的美食
 *
 * 时间复杂度：
 *   - insert:       O(m)，m 为字符串长度
 *   - searchPrefix: O(m + k)，m 为前缀长度，k 为结果数量
 *   - remove:       O(m)
 *   - search:       O(m)
 *
 * 空间复杂度：O(N * M)，N 为节点数，M 为平均子节点数
 */
class Trie {
private:
    // Trie 节点
    struct TrieNode {
        // 子节点链表节点
        struct ChildNode {
            char        ch;         // 字符（UTF-8 的一个字节）
            TrieNode*   node;       // 子节点指针
            ChildNode*  next;       // 下一个兄弟节点
            ChildNode(char c, TrieNode* n) : ch(c), node(n), next(nullptr) {}
        };

        ChildNode*  children;       // 子节点链表头
        bool        isEnd;          // 是否为完整字符串的结尾
        SeqList<int> foodIds;       // 结尾节点关联的美食 ID 列表

        TrieNode() : children(nullptr), isEnd(false) {}

        ~TrieNode() {
            ChildNode* p = children;
            while (p) {
                ChildNode* next = p->next;
                delete p->node;
                delete p;
                p = next;
            }
        }

        // 查找子节点
        ChildNode* findChild(char c) const {
            ChildNode* p = children;
            while (p) {
                if (p->ch == c) return p;
                p = p->next;
            }
            return nullptr;
        }

        // 添加子节点
        TrieNode* addChild(char c) {
            TrieNode* newNode = new TrieNode();
            ChildNode* child = new ChildNode(c, newNode);
            child->next = children;
            children = child;
            return newNode;
        }
    };

    TrieNode* root_;

    // 收集某个节点下的所有 food ID
    void collectAll(TrieNode* node, SeqList<int>& result) const {
        if (!node) return;
        if (node->isEnd) {
            for (int i = 0; i < node->foodIds.size(); i++) {
                result.push_back(node->foodIds[i]);
            }
        }
        TrieNode::ChildNode* p = node->children;
        while (p) {
            collectAll(p->node, result);
            p = p->next;
        }
    }

    // 递归删除节点
    void destroy(TrieNode* node) {
        if (!node) return;
        delete node;  // 析构函数会递归删除子节点
    }

public:
    Trie() : root_(new TrieNode()) {}

    ~Trie() {
        destroy(root_);
    }

    /**
     * 插入字符串，关联美食 ID
     * 时间复杂度: O(m)，m 为字符串长度
     */
    void insert(const std::string& key, int foodId) {
        TrieNode* current = root_;
        for (char c : key) {
            TrieNode::ChildNode* child = current->findChild(c);
            if (child) {
                current = child->node;
            } else {
                current = current->addChild(c);
            }
        }
        current->isEnd = true;
        // 避免重复插入
        if (current->foodIds.find(foodId) == -1) {
            current->foodIds.push_back(foodId);
        }
    }

    /**
     * 搜索精确匹配
     * 时间复杂度: O(m)
     * 返回: 匹配的 food ID 列表
     */
    SeqList<int> search(const std::string& key) const {
        TrieNode* current = root_;
        for (char c : key) {
            TrieNode::ChildNode* child = current->findChild(c);
            if (!child) return SeqList<int>();
            current = child->node;
        }
        if (current->isEnd) return current->foodIds;
        return SeqList<int>();
    }

    /**
     * 前缀搜索
     * 时间复杂度: O(m + k)，m 为前缀长度，k 为结果数量
     * 返回: 所有以 prefix 开头的 food ID
     */
    SeqList<int> searchPrefix(const std::string& prefix) const {
        TrieNode* current = root_;
        for (char c : prefix) {
            TrieNode::ChildNode* child = current->findChild(c);
            if (!child) return SeqList<int>();
            current = child->node;
        }
        // 收集该节点下所有 food ID
        SeqList<int> result;
        collectAll(current, result);
        return result;
    }

    /**
     * 检查是否存在某个键
     * 时间复杂度: O(m)
     */
    bool contains(const std::string& key) const {
        TrieNode* current = root_;
        for (char c : key) {
            TrieNode::ChildNode* child = current->findChild(c);
            if (!child) return false;
            current = child->node;
        }
        return current->isEnd;
    }

    /**
     * 删除指定键的指定 food ID
     * 时间复杂度: O(m)
     */
    bool remove(const std::string& key, int foodId) {
        TrieNode* current = root_;
        for (char c : key) {
            TrieNode::ChildNode* child = current->findChild(c);
            if (!child) return false;
            current = child->node;
        }
        if (!current->isEnd) return false;
        return current->foodIds.removeIf([foodId](const int& id) { return id == foodId; });
    }

    /**
     * 清空所有数据
     */
    void clear() {
        destroy(root_);
        root_ = new TrieNode();
    }
};

#endif // TRIE_H
