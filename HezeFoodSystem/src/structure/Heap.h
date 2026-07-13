#ifndef HEAP_H
#define HEAP_H

#include <stdexcept>

/**
 * 最小堆模板类
 * 手写实现，替代 std::priority_queue
 *
 * 业务用途：
 *   - Top-K 推荐结果（维护最大的 K 个元素用最大堆）
 *   - Dijkstra 算法中的优先队列（最小堆）
 *   - 排序（堆排序）
 *
 * 时间复杂度：
 *   - 插入 O(log n)
 *   - 取堆顶 O(1)
 *   - 删除堆顶 O(log n)
 *   - 建堆 O(n)
 */
template<typename T>
class Heap {
private:
    T*      data_;
    int     size_;
    int     capacity_;
    bool    isMinHeap_;  // true=最小堆, false=最大堆

    void expand() {
        int newCap = capacity_ == 0 ? 8 : capacity_ * 2;
        T* newData = new T[newCap];
        for (int i = 0; i < size_; i++) {
            newData[i] = data_[i];
        }
        delete[] data_;
        data_ = newData;
        capacity_ = newCap;
    }

    // 比较：根据堆类型决定上浮/下沉方向
    bool compare(const T& a, const T& b) const {
        if (isMinHeap_) return a < b;
        else            return a > b;
    }

    // 上浮
    void siftUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (compare(data_[index], data_[parent])) {
                T temp = data_[index];
                data_[index] = data_[parent];
                data_[parent] = temp;
                index = parent;
            } else {
                break;
            }
        }
    }

    // 下沉
    void siftDown(int index) {
        while (true) {
            int left  = 2 * index + 1;
            int right = 2 * index + 2;
            int target = index;

            if (left < size_ && compare(data_[left], data_[target]))
                target = left;
            if (right < size_ && compare(data_[right], data_[target]))
                target = right;

            if (target != index) {
                T temp = data_[index];
                data_[index] = data_[target];
                data_[target] = temp;
                index = target;
            } else {
                break;
            }
        }
    }

public:
    // isMin: true=最小堆, false=最大堆
    explicit Heap(bool isMin = true)
        : data_(nullptr), size_(0), capacity_(0), isMinHeap_(isMin) {}

    ~Heap() { delete[] data_; }

    int  size()  const { return size_; }
    bool empty() const { return size_ == 0; }

    // 查看堆顶
    T& top() {
        if (size_ == 0) throw std::out_of_range("Heap: top on empty");
        return data_[0];
    }

    const T& top() const {
        if (size_ == 0) throw std::out_of_range("Heap: top on empty");
        return data_[0];
    }

    // 插入
    void push(const T& val) {
        if (size_ >= capacity_) expand();
        data_[size_] = val;
        siftUp(size_);
        size_++;
    }

    // 删除堆顶
    void pop() {
        if (size_ == 0) throw std::out_of_range("Heap: pop on empty");
        data_[0] = data_[size_ - 1];
        size_--;
        if (size_ > 0) siftDown(0);
    }

    // 清空
    void clear() { size_ = 0; }
};

#endif // HEAP_H
