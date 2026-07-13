#ifndef QUEUE_H
#define QUEUE_H

#include <stdexcept>

/**
 * 队列模板类（基于链表实现）
 * 手写实现，替代 std::queue
 *
 * 业务用途：
 *   - BFS 广度优先搜索
 *   - 路线规划中的层次遍历
 *   - 任务排队
 *
 * 时间复杂度：
 *   - enqueue O(1)
 *   - dequeue O(1)
 *   - front   O(1)
 */
template<typename T>
class Queue {
private:
    struct Node {
        T       data;
        Node*   next;
        Node(const T& d, Node* n = nullptr) : data(d), next(n) {}
    };

    Node*   front_;     // 队头
    Node*   rear_;      // 队尾
    int     size_;

public:
    Queue() : front_(nullptr), rear_(nullptr), size_(0) {}

    ~Queue() {
        while (front_) {
            Node* del = front_;
            front_ = front_->next;
            delete del;
        }
    }

    int  size()  const { return size_; }
    bool empty() const { return size_ == 0; }

    // 入队
    void enqueue(const T& val) {
        Node* newNode = new Node(val);
        if (!rear_) {
            front_ = rear_ = newNode;
        } else {
            rear_->next = newNode;
            rear_ = newNode;
        }
        size_++;
    }

    // 出队
    void dequeue() {
        if (!front_) throw std::out_of_range("Queue: dequeue on empty");
        Node* del = front_;
        front_ = front_->next;
        if (!front_) rear_ = nullptr;
        delete del;
        size_--;
    }

    // 访问队头
    T& front() {
        if (!front_) throw std::out_of_range("Queue: front on empty");
        return front_->data;
    }

    const T& front() const {
        if (!front_) throw std::out_of_range("Queue: front on empty");
        return front_->data;
    }
};

#endif // QUEUE_H
