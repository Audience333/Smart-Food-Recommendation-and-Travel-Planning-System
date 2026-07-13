#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdexcept>

/**
 * 双向链表模板类
 * 手写实现，替代 std::list
 *
 * 业务用途：
 *   - 用户收藏列表（频繁增删）
 *   - 操作历史记录（撤销功能）
 *   - 路线途经点序列
 *
 * 时间复杂度：
 *   - 头尾插入 O(1)
 *   - 中间插入（已知位置）O(1)
 *   - 按值查找 O(n)
 *   - 删除（已知位置）O(1)
 */
template<typename T>
class LinkedList {
private:
    // 链表节点
    struct Node {
        T       data;
        Node*   prev;
        Node*   next;
        Node(const T& d, Node* p = nullptr, Node* n = nullptr)
            : data(d), prev(p), next(n) {}
    };

    Node*   head_;      // 哨兵头节点
    Node*   tail_;      // 哨兵尾节点
    int     size_;

public:
    // ---- 迭代器 ----
    class Iterator {
    private:
        Node* current;
    public:
        explicit Iterator(Node* p) : current(p) {}

        T& operator*()  { return current->data; }
        T* operator->() { return &(current->data); }

        Iterator& operator++() {
            current = current->next;
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            current = current->next;
            return tmp;
        }

        Iterator& operator--() {
            current = current->prev;
            return *this;
        }

        Iterator operator--(int) {
            Iterator tmp = *this;
            current = current->prev;
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            return current == other.current;
        }
        bool operator!=(const Iterator& other) const {
            return current != other.current;
        }

        // 允许 LinkedList 访问 current
        friend class LinkedList;
    };

    // 构造 / 析构
    LinkedList() : size_(0) {
        head_ = new Node(T());  // 哨兵头
        tail_ = new Node(T());  // 哨兵尾
        head_->next = tail_;
        tail_->prev = head_;
    }

    ~LinkedList() {
        clear();
        delete head_;
        delete tail_;
    }

    // 拷贝构造
    LinkedList(const LinkedList& other) : size_(0) {
        head_ = new Node(T());
        tail_ = new Node(T());
        head_->next = tail_;
        tail_->prev = head_;
        for (Node* p = other.head_->next; p != other.tail_; p = p->next) {
            push_back(p->data);
        }
    }

    // 拷贝赋值
    LinkedList& operator=(const LinkedList& other) {
        if (this != &other) {
            clear();
            for (Node* p = other.head_->next; p != other.tail_; p = p->next) {
                push_back(p->data);
            }
        }
        return *this;
    }

    // ---- 容量 ----
    int  size()  const { return size_; }
    bool empty() const { return size_ == 0; }

    // ---- 访问 ----
    T& front() {
        if (size_ == 0) throw std::out_of_range("LinkedList: empty");
        return head_->next->data;
    }

    T& back() {
        if (size_ == 0) throw std::out_of_range("LinkedList: empty");
        return tail_->prev->data;
    }

    // ---- 插入 ----
    void push_front(const T& val) {
        Node* newNode = new Node(val, head_, head_->next);
        head_->next->prev = newNode;
        head_->next = newNode;
        size_++;
    }

    void push_back(const T& val) {
        Node* newNode = new Node(val, tail_->prev, tail_);
        tail_->prev->next = newNode;
        tail_->prev = newNode;
        size_++;
    }

    // ---- 删除 ----
    void pop_front() {
        if (size_ == 0) throw std::out_of_range("LinkedList: pop_front on empty");
        Node* del = head_->next;
        head_->next = del->next;
        del->next->prev = head_;
        delete del;
        size_--;
    }

    void pop_back() {
        if (size_ == 0) throw std::out_of_range("LinkedList: pop_back on empty");
        Node* del = tail_->prev;
        tail_->prev = del->prev;
        del->prev->next = tail_;
        delete del;
        size_--;
    }

    // 删除指定迭代器位置的节点
    Iterator erase(Iterator it) {
        Node* del = it.current;
        if (del == head_ || del == tail_)
            throw std::out_of_range("LinkedList: cannot erase sentinel");
        Node* nextNode = del->next;
        del->prev->next = nextNode;
        nextNode->prev = del->prev;
        delete del;
        size_--;
        return Iterator(nextNode);
    }

    void clear() {
        Node* p = head_->next;
        while (p != tail_) {
            Node* next = p->next;
            delete p;
            p = next;
        }
        head_->next = tail_;
        tail_->prev = head_;
        size_ = 0;
    }

    // ---- 查找 ----
    // 线性查找，返回第一个匹配的迭代器
    Iterator find(const T& val) const {
        for (Node* p = head_->next; p != tail_; p = p->next) {
            if (p->data == val) return Iterator(p);
        }
        return Iterator(tail_); // end()
    }

    // ---- 迭代器接口 ----
    Iterator begin() { return Iterator(head_->next); }
    Iterator end()   { return Iterator(tail_); }
};

#endif // LINKEDLIST_H
