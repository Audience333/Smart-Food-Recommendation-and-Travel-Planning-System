/*
 * ============================================================================
 * doubly_linked_list.h — 双向链表（带哨兵头节点）
 * ============================================================================
 *
 * 【数据结构说明】
 *   带哨兵头节点的双向循环链表。哨兵节点不存储数据，仅作为链表的入口和边界标记。
 *   这使得插入和删除操作无需判断空链表/首尾节点的边界情况，代码更简洁。
 *
 * 【哨兵节点设计】
 *   head(sentinel) ⇄ node0 ⇄ node1 ⇄ ... ⇄ nodeN ⇄ head(sentinel)
 *   空链表时：head.next == head.prev == &head（自环）
 *
 * 【时间复杂度】
 *   pushBack / pushFront: O(1)
 *   remove(known node):   O(1)
 *   removeIf / find:      O(n)
 *   contains:             O(n)
 *   forwardTraverse / backwardTraverse: O(n)
 *
 * 【空间复杂度】
 *   O(n)，n=元素数量（每个节点含两个指针和一个数据项）
 *
 * 【使用场景】
 *   适用于频繁在任意位置插入/删除、需要双向遍历的场景。
 *   在美食推荐系统中可用于管理推荐列表、历史记录等。
 *
 * 【使用示例】
 *   DoublyLinkedList<int> list;
 *   list.pushBack(10); list.pushBack(20); list.pushFront(5);
 *   auto* node = list.find([](int x){ return x > 15; });
 *   list.remove(node);
 *   list.forwardTraverse([](int x){ printf("%d ", x); });
 * ============================================================================
 */

#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

#include <cstddef>
#include <functional>
#include <vector>

using std::vector;

/* ====================================================================
 * DListNode — 双向链表节点
 * ====================================================================
 * prev: 指向前驱节点
 * next: 指向后继节点
 * data: 节点存储的数据
 */
template <typename T>
struct DListNode {
    T data;              // 节点数据
    DListNode<T>* prev;  // 前驱指针
    DListNode<T>* next;  // 后继指针

    DListNode() : prev(nullptr), next(nullptr) {}
    explicit DListNode(const T& val) : data(val), prev(nullptr), next(nullptr) {}
};

/* ====================================================================
 * DoublyLinkedList — 双向链表模板类
 * ====================================================================
 * 内部使用哨兵头节点 head，不存储数据。
 * 所有数据节点插入在 head 之后、head 之前（形成循环）。
 * head.next 指向第一个数据节点，head.prev 指向最后一个数据节点。
 */
template <typename T>
class DoublyLinkedList {
private:
    DListNode<T> head;   // 哨兵头节点（不存储数据）
    int nodeCount;       // 节点数量（不含哨兵）

    /* ---- 将新节点插入到指定节点之后 ---- */
    void insertAfter(DListNode<T>* node, DListNode<T>* newNode) {
        newNode->prev = node;
        newNode->next = node->next;
        node->next->prev = newNode;
        node->next = newNode;
    }

    /* ---- 从链表中摘除指定节点（不释放内存） ---- */
    void detach(DListNode<T>* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

public:
    /* ---- 构造：初始化哨兵节点的自环 ---- */
    DoublyLinkedList() : nodeCount(0) {
        head.prev = &head;
        head.next = &head;
    }

    /* ---- 析构：释放所有数据节点 ---- */
    ~DoublyLinkedList() {
        clear();
    }

    /* ---- 拷贝构造（深拷贝，不拷贝哨兵） ---- */
    DoublyLinkedList(const DoublyLinkedList& other) : nodeCount(0) {
        head.prev = &head;
        head.next = &head;
        // 从头到尾遍历并拷贝每个节点
        DListNode<T>* cur = other.head.next;
        while (cur != &(other.head)) {
            pushBack(cur->data);
            cur = cur->next;
        }
    }

    /* ---- 赋值操作符 ---- */
    DoublyLinkedList& operator=(const DoublyLinkedList& other) {
        if (this != &other) {
            clear();
            DListNode<T>* cur = other.head.next;
            while (cur != &(other.head)) {
                pushBack(cur->data);
                cur = cur->next;
            }
        }
        return *this;
    }

    /* ====================================================================
     * pushBack — 在链表末尾插入元素 O(1)
     * ====================================================================
     * 新节点插入在哨兵节点之前（即最后一个数据节点之后）。
     */
    DListNode<T>* pushBack(const T& val) {
        DListNode<T>* newNode = new DListNode<T>(val);
        insertAfter(head.prev, newNode);
        nodeCount++;
        return newNode;
    }

    /* ====================================================================
     * pushFront — 在链表头部插入元素 O(1)
     * ====================================================================
     * 新节点插入在哨兵节点之后（即第一个数据节点之前）。
     */
    DListNode<T>* pushFront(const T& val) {
        DListNode<T>* newNode = new DListNode<T>(val);
        insertAfter(&head, newNode);
        nodeCount++;
        return newNode;
    }

    /* ====================================================================
     * remove — 删除指定节点 O(1)
     * ====================================================================
     * 参数：node - 要删除的节点指针（必须属于本链表，不能是哨兵）
     * 注意：调用者需确保 node 有效且非哨兵节点
     */
    void remove(DListNode<T>* node) {
        if (node == nullptr || node == &head) return;
        detach(node);
        delete node;
        nodeCount--;
    }

    /* ====================================================================
     * removeIf — 按条件删除节点 O(n)
     * ====================================================================
     * 遍历链表，删除所有满足 predicate 的数据节点。
     * 参数：predicate - 判断函数，接受 T 的常量引用，返回是否应删除
     * 返回：删除的节点数量
     */
    int removeIf(bool (*predicate)(const T&)) {
        int removed = 0;
        DListNode<T>* cur = head.next;
        while (cur != &head) {
            DListNode<T>* nextNode = cur->next;  // 暂存后继（因为cur可能被删除）
            if (predicate(cur->data)) {
                detach(cur);
                delete cur;
                nodeCount--;
                removed++;
            }
            cur = nextNode;
        }
        return removed;
    }

    /* ====================================================================
     * find — 按条件查找节点 O(n)
     * ====================================================================
     * 返回第一个满足 predicate 的节点指针。
     * 参数：predicate - 判断函数
     * 返回：找到的节点指针，未找到返回 nullptr
     */
    DListNode<T>* find(bool (*predicate)(const T&)) const {
        DListNode<T>* cur = head.next;
        while (cur != &head) {
            if (predicate(cur->data)) return cur;
            cur = cur->next;
        }
        return nullptr;
    }

    /* ====================================================================
     * contains — 判断链表中是否存在满足条件的数据 O(n)
     * ====================================================================
     * 返回：存在至少一个满足条件的数据返回 true，否则 false
     */
    bool contains(bool (*predicate)(const T&)) const {
        return find(predicate) != nullptr;
    }

    /* ====================================================================
     * forwardTraverse — 正向遍历 O(n)
     * ====================================================================
     * 从第一个数据节点开始，依次向后（next方向）遍历到最后一个，
     * 对每个节点数据调用 callback 函数。
     * 参数：callback - 回调函数，接受 T& 引用
     */
    void forwardTraverse(void (*callback)(T&)) {
        DListNode<T>* cur = head.next;
        while (cur != &head) {
            callback(cur->data);
            cur = cur->next;
        }
    }

    /* ====================================================================
     * backwardTraverse — 反向遍历 O(n)
     * ====================================================================
     * 从最后一个数据节点开始，依次向前（prev方向）遍历到第一个，
     * 对每个节点数据调用 callback 函数。
     */
    void backwardTraverse(void (*callback)(T&)) {
        DListNode<T>* cur = head.prev;
        while (cur != &head) {
            callback(cur->data);
            cur = cur->prev;
        }
    }

    /* ====================================================================
     * toVector — 将链表转换为STL向量 O(n)
     * ====================================================================
     * 正向遍历收集所有数据到 vector 中。
     * 返回：包含所有链表数据的 vector
     */
    vector<T> toVector() const {
        vector<T> result;
        DListNode<T>* cur = head.next;
        while (cur != &head) {
            result.push_back(cur->data);
            cur = cur->next;
        }
        return result;
    }

    /* ====================================================================
     * fromVector — 从STL向量重建链表 O(n)
     * ====================================================================
     * 清空当前链表，从 vector 中依次 pushBack 添加元素。
     */
    void fromVector(const vector<T>& vec) {
        clear();
        int n = (int)vec.size();
        for (int i = 0; i < n; i++) {
            pushBack(vec[i]);
        }
    }

    /* ---- 获取元素数量 ---- */
    int size() const { return nodeCount; }

    /* ---- 判断链表是否为空 ---- */
    bool empty() const { return nodeCount == 0; }

    /* ====================================================================
     * clear — 清空链表 O(n)
     * ====================================================================
     * 删除所有数据节点，重置哨兵的自环。
     */
    void clear() {
        DListNode<T>* cur = head.next;
        while (cur != &head) {
            DListNode<T>* toDelete = cur;
            cur = cur->next;
            delete toDelete;
        }
        head.next = &head;
        head.prev = &head;
        nodeCount = 0;
    }

    /* ---- 获取第一个数据节点（哨兵之后） ---- */
    DListNode<T>* first() const {
        if (empty()) return nullptr;
        return head.next;
    }

    /* ---- 获取最后一个数据节点（哨兵之前） ---- */
    DListNode<T>* last() const {
        if (empty()) return nullptr;
        return head.prev;
    }
};

#endif // DOUBLY_LINKED_LIST_H
