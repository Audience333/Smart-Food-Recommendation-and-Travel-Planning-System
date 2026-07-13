#ifndef STACK_H
#define STACK_H

#include <stdexcept>

/**
 * 栈模板类（基于链表实现）
 * 手写实现，替代 std::stack
 *
 * 业务用途：
 *   - 操作历史记录（撤销功能）
 *   - DFS 深度优先搜索
 *   - 路径回溯
 *
 * 时间复杂度：
 *   - push O(1)
 *   - pop  O(1)
 *   - top  O(1)
 */
template<typename T>
class Stack {
private:
    struct Node {
        T       data;
        Node*   next;
        Node(const T& d, Node* n = nullptr) : data(d), next(n) {}
    };

    Node*   top_;
    int     size_;

public:
    Stack() : top_(nullptr), size_(0) {}

    ~Stack() {
        while (top_) {
            Node* del = top_;
            top_ = top_->next;
            delete del;
        }
    }

    // 拷贝构造
    Stack(const Stack& other) : top_(nullptr), size_(0) {
        if (!other.top_) return;
        // 反转链表后拷贝
        Node* p = other.top_;
        Stack<T> temp;
        while (p) {
            temp.push(p->data);
            p = p->next;
        }
        while (!temp.empty()) {
            push(temp.top());
            temp.pop();
        }
    }

    int  size()  const { return size_; }
    bool empty() const { return size_ == 0; }

    void push(const T& val) {
        top_ = new Node(val, top_);
        size_++;
    }

    void pop() {
        if (!top_) throw std::out_of_range("Stack: pop on empty");
        Node* del = top_;
        top_ = top_->next;
        delete del;
        size_--;
    }

    T& top() {
        if (!top_) throw std::out_of_range("Stack: top on empty");
        return top_->data;
    }

    const T& top() const {
        if (!top_) throw std::out_of_range("Stack: top on empty");
        return top_->data;
    }
};

#endif // STACK_H
