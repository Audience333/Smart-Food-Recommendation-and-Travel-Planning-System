/*
 * ============================================================================
 * seq_stack.h — 顺序栈（数组实现，支持撤销/重做）
 * ============================================================================
 *
 * 【数据结构说明】
 *   基于动态数组的顺序栈，除标准栈操作外，额外支持 undo/redo 机制。
 *   undo() 不删除元素，而是将 current 指针回退一步；
 *   redo() 将 current 指针前进一步。
 *   这使得栈可以记录操作历史并支持撤销与重做的遍历。
 *
 * 【时间复杂度】
 *   push/pop/top/undo/redo: O(1)
 *   clear: O(1)（仅重置指针）
 *
 * 【空间复杂度】
 *   O(maxSize)，预分配固定容量
 *
 * 【使用场景】
 *   适用于需要操作历史回溯的编辑器、状态管理、路径记录等场景。
 *   在美食推荐系统中可用于记录用户的浏览/筛选历史。
 *
 * 【使用示例】
 *   SeqStack<int> stack(100);
 *   stack.push(10); stack.push(20); stack.push(30);
 *   int val = stack.undo();  // val=30, 回到状态[10,20]
 *   int val2 = stack.redo(); // val2=30, 回到状态[10,20,30]
 * ============================================================================
 */

#ifndef SEQ_STACK_H
#define SEQ_STACK_H

#include <cstddef>
#include <stdexcept>

/* ====================================================================
 * SeqStack — 顺序栈模板类
 * ====================================================================
 * 使用原始动态数组 T* data 存储元素，int maxSize 限制容量。
 * 关键指针：
 *   topIndex  — 栈顶索引（指向最后一个有效元素的位置）
 *   current   — undo/redo 遍历指针（指向当前可见的栈顶位置）
 *                调用 undo 时 current--, 调用 redo 时 current++
 *                push 操作会截断 redo 历史（新数据从 current+1 开始写入）
 */
template <typename T>
class SeqStack {
private:
    T* data;            // 原始数组数据区
    int maxSize;        // 最大容量
    int topIndex;       // 栈顶索引（-1 表示空栈）
    int current;        // undo/redo 当前指针

public:
    /* ---- 构造：分配 maxSize 大小的数组 ---- */
    SeqStack(int maxSize = 256) : maxSize(maxSize), topIndex(-1), current(-1) {
        data = new T[maxSize];
    }

    /* ---- 析构：释放数组 ---- */
    ~SeqStack() {
        delete[] data;
    }

    /* ---- 拷贝构造（深拷贝） ---- */
    SeqStack(const SeqStack& other)
        : maxSize(other.maxSize), topIndex(other.topIndex), current(other.current) {
        data = new T[maxSize];
        for (int i = 0; i <= topIndex; i++) {
            data[i] = other.data[i];
        }
    }

    /* ---- 赋值操作符（深拷贝） ---- */
    SeqStack& operator=(const SeqStack& other) {
        if (this != &other) {
            delete[] data;
            maxSize = other.maxSize;
            topIndex = other.topIndex;
            current = other.current;
            data = new T[maxSize];
            for (int i = 0; i <= topIndex; i++) {
                data[i] = other.data[i];
            }
        }
        return *this;
    }

    /* ====================================================================
     * push — 入栈操作
     * ====================================================================
     * 将元素压入栈顶。如果 current < topIndex（即之前有 undo 操作），
     * 则截断后面的重做历史：新元素从 current+1 位置开始覆盖写入。
     * 参数：item - 要入栈的元素
     * 抛出：容量满时抛出 std::overflow_error
     */
    void push(const T& item) {
        if (current + 1 >= maxSize) {
            throw std::overflow_error("SeqStack overflow: max capacity reached");
        }
        // 如果 current < topIndex，说明之前 undo 过，截断旧数据
        // 新数据写入 current+1 位置
        current++;
        data[current] = item;
        topIndex = current;  // 更新栈顶（截断了旧的重做记录）
    }

    /* ====================================================================
     * pop — 出栈操作
     * ====================================================================
     * 移除并返回栈顶元素（基于 current 指针）。
     * 返回：栈顶元素的拷贝
     * 抛出：栈空时抛出 std::underflow_error
     */
    T pop() {
        if (current < 0) {
            throw std::underflow_error("SeqStack underflow: stack is empty");
        }
        T val = data[current];
        current--;
        topIndex = current;  // 截断
        return val;
    }

    /* ====================================================================
     * top — 查看栈顶元素
     * ====================================================================
     * 返回栈顶元素的常量引用（不删除）。
     * 抛出：栈空时抛出 std::underflow_error
     */
    const T& top() const {
        if (current < 0) {
            throw std::underflow_error("SeqStack underflow: stack is empty");
        }
        return data[current];
    }

    /* ====================================================================
     * undo — 撤销操作
     * ====================================================================
     * 将 current 指针回退一步，返回回退前指向的元素（被撤销的项）。
     * 不真正删除数据，数据仍在数组中，可通过 redo() 恢复。
     * 返回：被撤销的栈顶元素
     * 抛出：无法继续回退时抛出 std::underflow_error
     */
    T undo() {
        if (current < 0) {
            throw std::underflow_error("SeqStack undo failed: no more history");
        }
        T val = data[current];
        current--;
        // 注意：topIndex 不变，保留 redo 历史
        return val;
    }

    /* ====================================================================
     * redo — 重做操作
     * ====================================================================
     * 将 current 指针前进一步，返回前进后指向的元素（恢复的项）。
     * 返回：被恢复的元素
     * 抛出：无法继续重做时抛出 std::underflow_error
     */
    T redo() {
        if (current + 1 > topIndex) {
            throw std::underflow_error("SeqStack redo failed: no more future");
        }
        current++;
        return data[current];
    }

    /* ---- 判断栈是否为空 ---- */
    bool empty() const { return current < 0; }

    /* ---- 获取当前可见元素数量 ---- */
    int size() const { return current + 1; }

    /* ---- 获取最大容量 ---- */
    int capacity() const { return maxSize; }

    /* ---- 获取物理栈顶索引（含被undo的数据） ---- */
    int totalSize() const { return topIndex + 1; }

    /* ---- 是否可以undo ---- */
    bool canUndo() const { return current >= 0; }

    /* ---- 是否可以redo ---- */
    bool canRedo() const { return current < topIndex; }

    /* ====================================================================
     * clear — 清空栈
     * ====================================================================
     * 重置所有指针，不释放数组内存。清空后 undo/redo 历史均不可用。
     */
    void clear() {
        topIndex = -1;
        current = -1;
    }
};

#endif // SEQ_STACK_H
