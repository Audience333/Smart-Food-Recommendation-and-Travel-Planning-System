/*
 * ============================================================================
 * decision_tree.h — 决策树多条件筛选器（手写C++实现）
 * ============================================================================
 *
 * 【数据结构说明】
 *   决策树用于对数据进行多条件链式筛选。每个节点包含一个判断条件函数指针，
 *   满足条件则进入 trueBranch 继续判断，不满足则进入 falseBranch。
 *   build() 将一组条件链式串联，filter() 对数据集执行筛选。
 *
 * 【时间复杂度】
 *   建树: O(n)，n=条件数量
 *   筛选: O(m * n)，m=数据项数量，n=条件数量
 *
 * 【空间复杂度】
 *   O(n)，n=条件数量（每个条件一个节点）
 *
 * 【使用示例】
 *   DecisionTree<FoodItem> dt;
 *   dt.build({isCheap, isHighRated, isOpenNow});
 *   auto results = dt.filter(allFoods);
 * ============================================================================
 */

#ifndef DECISION_TREE_H
#define DECISION_TREE_H

#include <cstddef>
#include <vector>

using std::vector;

/* ====================================================================
 * TreeNode — 决策树节点
 * ====================================================================
 * 每个节点存储一个条件判断函数指针和两个子分支指针。
 * 条件函数返回 true → 走 trueBranch
 * 条件函数返回 false → 走 falseBranch
 * 两个子分支都为 nullptr 表示叶子节点（筛选通过）
 */
template <typename T>
struct TreeNode {
    // 条件判断函数：接受数据项的常量引用，返回是否满足条件
    bool (*condition)(const T&);
    // 左子节点：条件满足时的下一个判断节点
    TreeNode<T>* trueBranch;
    // 右子节点：条件不满足时的下一个判断节点
    TreeNode<T>* falseBranch;

    /* ---- 构造 ---- */
    TreeNode() : condition(nullptr), trueBranch(nullptr), falseBranch(nullptr) {}
};

/* ====================================================================
 * DecisionTree — 决策树筛选器
 * ====================================================================
 * 将多个条件构建成链式决策树，对数据集逐条遍历筛选。
 * 筛选通过的标志：从根节点出发，按条件结果走到叶子节点（两个分支都为空）。
 * 默认配置下，falseBranch 始终为 nullptr（即任一条件不满足则淘汰），
 * trueBranch 指向下一个条件节点。
 */
template <typename T>
class DecisionTree {
private:
    TreeNode<T>* root;       // 根节点指针
    int nodeCount;           // 节点总数

    /* ---- 递归删除子树 ---- */
    void deleteTree(TreeNode<T>* node) {
        if (node == nullptr) return;
        deleteTree(node->trueBranch);
        deleteTree(node->falseBranch);
        delete node;
    }

    /* ---- 对单项数据执行树遍历判断 ---- */
    bool evaluate(const T& item) const {
        TreeNode<T>* current = root;
        while (current != nullptr) {
            if (current->condition == nullptr) {
                // 无条件节点：两个分支都为空 → 通过
                if (current->trueBranch == nullptr && current->falseBranch == nullptr)
                    return true;
                // 否则继续遍历（虽然这种节点不太可能出现）
                current = current->trueBranch;
                continue;
            }
            // 根据条件结果选择分支
            if (current->condition(item)) {
                if (current->trueBranch == nullptr)
                    return true;                // 到达叶子节点，筛选通过
                current = current->trueBranch;
            } else {
                if (current->falseBranch == nullptr)
                    return false;               // 条件不满足且无false分支，淘汰
                current = current->falseBranch;
            }
        }
        return true;  // 空树：所有数据通过
    }

public:
    /* ---- 构造与析构 ---- */
    DecisionTree() : root(nullptr), nodeCount(0) {}

    ~DecisionTree() {
        deleteTree(root);
        root = nullptr;
    }

    /* ====================================================================
     * build — 根据条件列表构建决策树
     * ====================================================================
     * 将传入的条件函数指针数组链式串联：
     *   条件0 → trueBranch → 条件1 → trueBranch → ... → 条件n-1 → 通过
     *   任一条件不满足 → falseBranch(nullptr) → 淘汰
     * 参数：conditions - 条件函数指针的向量
     * 注意：会先清空已有树结构
     */
    void build(const vector<bool(*)(const T&)>& conditions) {
        // 清空旧树
        deleteTree(root);
        root = nullptr;
        nodeCount = 0;

        if (conditions.empty()) return;

        // 手写动态数组存储节点（不使用STL容器）
        int n = (int)conditions.size();
        TreeNode<T>** nodes = new TreeNode<T>*[n];
        for (int i = 0; i < n; i++) {
            nodes[i] = new TreeNode<T>();
            nodes[i]->condition = conditions[i];
            nodeCount++;
        }

        // 链式串联：第i个节点的trueBranch指向第i+1个节点
        // falseBranch全部为nullptr（不满足则淘汰）
        for (int i = 0; i < n - 1; i++) {
            nodes[i]->trueBranch = nodes[i + 1];
        }
        // 最后一个节点的trueBranch设为nullptr（叶子节点，通过）

        root = nodes[0];
        delete[] nodes;  // 只释放指针数组，节点本身由树管理
    }

    /* ====================================================================
     * filter — 对数据集执行决策树筛选
     * ====================================================================
     * 遍历数据项，对每一项调用 evaluate() 判断是否通过全部条件。
     * 返回通过筛选的数据项列表。
     * 参数：items - 待筛选的数据集
     * 返回：通过所有条件的数据项向量
     */
    vector<T> filter(const vector<T>& items) const {
        vector<T> result;
        int n = (int)items.size();
        for (int i = 0; i < n; i++) {
            if (evaluate(items[i])) {
                result.push_back(items[i]);
            }
        }
        return result;
    }

    /* ---- 获取节点总数 ---- */
    int size() const { return nodeCount; }

    /* ---- 判断树是否为空 ---- */
    bool empty() const { return root == nullptr; }
};

#endif // DECISION_TREE_H
