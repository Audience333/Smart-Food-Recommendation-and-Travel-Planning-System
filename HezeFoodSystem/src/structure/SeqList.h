#ifndef SEQLIST_H
#define SEQLIST_H

#include <stdexcept>

/**
 * 顺序表（动态数组）模板类
 * 手写实现，替代 std::vector
 *
 * 业务用途：
 *   - 美食/景点数据的主存储容器
 *   - 标签列表存储
 *   - 搜索结果列表
 *
 * 时间复杂度：
 *   - 随机访问 operator[]  : O(1)
 *   - 尾部插入 push_back   : O(1) 均摊（扩容时 O(n)）
 *   - 中间插入 insert      : O(n)
 *   - 按下标删除 erase     : O(n)
 *   - 按值查找 find        : O(n)
 *   - 按条件查找 findIf    : O(n)
 *
 * 空间复杂度：
 *   - 存储 n 个元素: O(n)
 *   - 扩容策略: 容量翻倍，均摊 O(1) 插入
 */
template<typename T>
class SeqList {
private:
    T*      data_;      // 动态数组指针
    int     size_;      // 当前元素个数
    int     capacity_;  // 数组容量

    // 扩容：容量翻倍
    // 均摊时间复杂度: O(1)
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

public:
    // ==================== 构造 / 析构 ====================

    SeqList() : data_(nullptr), size_(0), capacity_(0) {}

    explicit SeqList(int initCap)
        : data_(initCap > 0 ? new T[initCap] : nullptr),
          size_(0), capacity_(initCap) {}

    ~SeqList() { delete[] data_; }

    // 拷贝构造
    SeqList(const SeqList& other)
        : data_(other.capacity_ > 0 ? new T[other.capacity_] : nullptr),
          size_(other.size_), capacity_(other.capacity_) {
        for (int i = 0; i < size_; i++) {
            data_[i] = other.data_[i];
        }
    }

    // 拷贝赋值
    SeqList& operator=(const SeqList& other) {
        if (this != &other) {
            delete[] data_;
            capacity_ = other.capacity_;
            size_ = other.size_;
            data_ = capacity_ > 0 ? new T[capacity_] : nullptr;
            for (int i = 0; i < size_; i++) {
                data_[i] = other.data_[i];
            }
        }
        return *this;
    }

    // ==================== 容量相关 ====================

    int  size()     const { return size_; }
    int  capacity() const { return capacity_; }
    bool empty()    const { return size_ == 0; }

    // 预留空间
    void reserve(int newCap) {
        if (newCap > capacity_) {
            T* newData = new T[newCap];
            for (int i = 0; i < size_; i++) {
                newData[i] = data_[i];
            }
            delete[] data_;
            data_ = newData;
            capacity_ = newCap;
        }
    }

    // ==================== 元素访问 ====================

    T& operator[](int index) {
        if (index < 0 || index >= size_)
            throw std::out_of_range("SeqList: index " + std::to_string(index) +
                                     " out of range [0, " + std::to_string(size_) + ")");
        return data_[index];
    }

    const T& operator[](int index) const {
        if (index < 0 || index >= size_)
            throw std::out_of_range("SeqList: index out of range");
        return data_[index];
    }

    T& at(int index) { return operator[](index); }

    T& front() {
        if (size_ == 0) throw std::out_of_range("SeqList: empty");
        return data_[0];
    }

    T& back() {
        if (size_ == 0) throw std::out_of_range("SeqList: empty");
        return data_[size_ - 1];
    }

    // 获取底层数组指针（用于兼容C接口）
    T* data() { return data_; }

    // ==================== 插入操作 ====================

    // 尾部追加 O(1) 均摊
    void push_back(const T& val) {
        if (size_ >= capacity_) expand();
        data_[size_++] = val;
    }

    // 在指定位置插入 O(n)
    void insert(int index, const T& val) {
        if (index < 0 || index > size_)
            throw std::out_of_range("SeqList: insert index out of range");
        if (size_ >= capacity_) expand();
        for (int i = size_; i > index; i--) {
            data_[i] = data_[i - 1];
        }
        data_[index] = val;
        size_++;
    }

    // ==================== 删除操作 ====================

    // 删除指定位置元素 O(n)
    void erase(int index) {
        if (index < 0 || index >= size_)
            throw std::out_of_range("SeqList: erase index out of range");
        for (int i = index; i < size_ - 1; i++) {
            data_[i] = data_[i + 1];
        }
        size_--;
    }

    // 删除最后一个元素 O(1)
    void pop_back() {
        if (size_ == 0) throw std::out_of_range("SeqList: pop_back on empty");
        size_--;
    }

    // 清空 O(1)
    void clear() { size_ = 0; }

    // ==================== 查找操作 ====================

    // 线性查找，返回下标，未找到返回 -1  O(n)
    int find(const T& val) const {
        for (int i = 0; i < size_; i++) {
            if (data_[i] == val) return i;
        }
        return -1;
    }

    /**
     * 按条件查找（函数指针/lambda）
     * 返回第一个满足条件的元素下标，未找到返回 -1
     * 时间复杂度: O(n)
     *
     * 用法示例:
     *   int idx = foods.findIf([](const Food& f) { return f.id == 5; });
     */
    template<typename Predicate>
    int findIf(Predicate pred) const {
        for (int i = 0; i < size_; i++) {
            if (pred(data_[i])) return i;
        }
        return -1;
    }

    /**
     * 按条件查找所有满足条件的元素
     * 返回包含所有匹配元素的新顺序表
     * 时间复杂度: O(n)
     *
     * 用法示例:
     *   auto results = foods.findAll([](const Food& f) { return f.category == "汤类"; });
     */
    template<typename Predicate>
    SeqList<T> findAll(Predicate pred) const {
        SeqList<T> result;
        for (int i = 0; i < size_; i++) {
            if (pred(data_[i])) {
                result.push_back(data_[i]);
            }
        }
        return result;
    }

    /**
     * 按条件删除第一个满足条件的元素
     * 返回是否删除成功
     * 时间复杂度: O(n)
     */
    template<typename Predicate>
    bool removeIf(Predicate pred) {
        for (int i = 0; i < size_; i++) {
            if (pred(data_[i])) {
                erase(i);
                return true;
            }
        }
        return false;
    }

    // ==================== 排序操作 ====================

    /**
     * 插入排序（稳定排序，适合小数据量）
     * 时间复杂度: O(n²) 最坏, O(n) 最好（已有序）
     * 空间复杂度: O(1)
     *
     * 用法示例:
     *   foods.sort([](const Food& a, const Food& b) { return a.score > b.score; });
     */
    template<typename Compare>
    void sort(Compare comp) {
        for (int i = 1; i < size_; i++) {
            T key = data_[i];
            int j = i - 1;
            while (j >= 0 && comp(key, data_[j])) {
                data_[j + 1] = data_[j];
                j--;
            }
            data_[j + 1] = key;
        }
    }

    /**
     * 快速排序（适合大数据量）
     * 时间复杂度: O(n log n) 平均
     * 空间复杂度: O(log n) 递归栈
     */
    template<typename Compare>
    void quickSort(Compare comp) {
        if (size_ <= 1) return;
        quickSortHelper(0, size_ - 1, comp);
    }

private:
    template<typename Compare>
    void quickSortHelper(int low, int high, Compare comp) {
        if (low >= high) return;
        int pivotIdx = partition(low, high, comp);
        quickSortHelper(low, pivotIdx - 1, comp);
        quickSortHelper(pivotIdx + 1, high, comp);
    }

    template<typename Compare>
    int partition(int low, int high, Compare comp) {
        T pivot = data_[high];
        int i = low - 1;
        for (int j = low; j < high; j++) {
            if (comp(data_[j], pivot)) {
                i++;
                T temp = data_[i];
                data_[i] = data_[j];
                data_[j] = temp;
            }
        }
        T temp = data_[i + 1];
        data_[i + 1] = data_[high];
        data_[high] = temp;
        return i + 1;
    }

public:
    // ==================== 遍历操作 ====================

    /**
     * 对每个元素执行操作
     * 用法示例:
     *   foods.forEach([](const Food& f) { cout << f.name << endl; });
     */
    template<typename Func>
    void forEach(Func func) const {
        for (int i = 0; i < size_; i++) {
            func(data_[i]);
        }
    }
};

#endif // SEQLIST_H
