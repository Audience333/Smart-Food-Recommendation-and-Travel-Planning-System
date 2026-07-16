/*
 * ============================================================================
 * bfs.h — 广度优先搜索（手写队列实现，查找周边美食）
 * ============================================================================
 *
 * 【算法说明】
 *   广度优先搜索（BFS）从起点出发，逐层遍历所有可达节点。
 *   本实现使用手写循环队列（数组实现），不使用STL queue。
 *   用于查找指定起点周边一定距离范围内的美食POI。
 *
 * 【手写队列设计】
 *   CircularQueue: 基于固定大小数组的循环队列
 *   - front: 队首索引（出队端）
 *   - rear:  队尾索引（入队端）
 *   - 空队列判断：front == -1
 *   - 满队列判断：(rear+1) % capacity == front
 *
 * 【时间复杂度】
 *   O(V + E)，V=顶点数，E=边数（BFS遍历）
 *
 * 【空间复杂度】
 *   O(V)，队列 + visited数组 + 距离数组
 *
 * 【使用场景】
 *   在路网中查找某景点周边的美食集群，例如：
 *   从曹州牡丹园出发，查找5公里范围内的所有美食。
 *
 * 【使用示例】
 *   BFS bfs(graph);
 *   auto nearby = bfs.nearbyFood(1, 5000.0);
 *   for (auto& item : nearby)
 *       printf("%s (%.1f米)\n", item.name.c_str(), item.distance);
 * ============================================================================
 */

#ifndef BFS_H
#define BFS_H

#include "adjacency_graph.h"
#include <cstddef>
#include <cfloat>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

using std::vector;
using std::string;
using std::pair;

/* ====================================================================
 * NearbyFoodItem — 附近美食结果项
 * ====================================================================
 * 存储通过BFS搜索到的周边美食POI信息。
 */
struct NearbyFoodItem {
    int id;                 // POI ID
    string name;            // POI名称
    double distance;        // 累计路径距离（米）
    double lng, lat;        // 经纬度

    // 用于排序
    bool operator<(const NearbyFoodItem& other) const {
        return distance < other.distance;
    }
};

/* ====================================================================
 * CircularQueue — 手写循环队列（整数索引队列，用于BFS）
 * ====================================================================
 * 基于固定大小数组实现，避免STL queue依赖。
 */
class CircularQueue {
private:
    int* data;           // 队列数组
    int capacity;        // 队列容量
    int front;           // 队首索引
    int rear;            // 队尾索引
    int count;           // 当前元素数量

public:
    /* ---- 构造：分配指定容量 ---- */
    explicit CircularQueue(int cap = 1024) : capacity(cap), front(0), rear(-1), count(0) {
        data = new int[capacity];
    }

    /* ---- 析构 ---- */
    ~CircularQueue() { delete[] data; }

    /* ---- 入队 ---- */
    void enqueue(int val) {
        if (count >= capacity) return;  // 队列满，静默忽略（实际使用中容量足够）
        rear = (rear + 1) % capacity;
        data[rear] = val;
        count++;
    }

    /* ---- 出队 ---- */
    int dequeue() {
        if (count <= 0) return -1;
        int val = data[front];
        front = (front + 1) % capacity;
        count--;
        return val;
    }

    /* ---- 判空 ---- */
    bool empty() const { return count == 0; }

    /* ---- 获取元素数量 ---- */
    int size() const { return count; }
};

/* ====================================================================
 * BFS — 广度优先搜索类
 * ====================================================================
 * 从指定顶点出发，按层遍历图，收集距离范围内的美食POI。
 * 返回结果按距离升序排列。
 */
class BFS {
private:
    const AdjacencyGraph& graph;

public:
    /* ---- 构造：绑定到指定图 ---- */
    explicit BFS(const AdjacencyGraph& g) : graph(g) {}

    /* ====================================================================
     * nearbyFood — 查找周边美食
     * ====================================================================
     * 从起点出发执行BFS，当累计距离超过 maxDistance 时停止该分支的扩展。
     * 收集所有距离 ≤ maxDistance 的顶点中 type=="food" 的POI。
     *
     * 参数：
     *   startId     - 起点顶点ID
     *   maxDistance - 最大搜索距离（米）
     * 返回：附近美食的 NearbyFoodItem 列表，按距离升序排列
     */
    vector<NearbyFoodItem> nearbyFood(int startId, double maxDistance) {
        vector<NearbyFoodItem> result;
        int V = graph.vertexCount_();
        if (V == 0) return result;

        int startIdx = graph.getIndexById(startId);
        if (startIdx == -1) return result;

        // 手写访问标记和距离数组
        bool* visited = new bool[V];
        double* distance = new double[V];
        for (int i = 0; i < V; i++) {
            visited[i] = false;
            distance[i] = DBL_MAX;
        }

        // 手写循环队列
        CircularQueue queue(V + 16);

        // 起点入队
        visited[startIdx] = true;
        distance[startIdx] = 0;
        queue.enqueue(startIdx);

        // 如果起点本身就是美食，也加入结果
        {
            const Vertex* sv = graph.getVertexByIndex(startIdx);
            if (sv != nullptr && strcmp(sv->type, "food") == 0) {
                NearbyFoodItem item;
                item.id = sv->id;
                item.name = string(sv->name);
                item.distance = 0;
                item.lng = sv->lng;
                item.lat = sv->lat;
                result.push_back(item);
            }
        }

        // BFS主循环
        while (!queue.empty()) {
            int u = queue.dequeue();

            int neighborCount = 0;
            const Edge* edges = graph.getNeighbors(
                graph.getVertexByIndex(u)->id, neighborCount);

            for (int i = 0; i < neighborCount; i++) {
                int v = edges[i].to;  // 邻接顶点索引

                if (visited[v]) continue;

                double newDist = distance[u] + edges[i].distance;
                if (newDist > maxDistance) continue;  // 超出搜索范围，剪枝

                visited[v] = true;
                distance[v] = newDist;
                queue.enqueue(v);

                // 检查是否为美食POI
                const Vertex* nv = graph.getVertexByIndex(v);
                if (nv != nullptr && strcmp(nv->type, "food") == 0) {
                    NearbyFoodItem item;
                    item.id = nv->id;
                    item.name = string(nv->name);
                    item.distance = newDist;
                    item.lng = nv->lng;
                    item.lat = nv->lat;
                    result.push_back(item);
                }
            }
        }

        // 清理手写数组
        delete[] visited;
        delete[] distance;

        // 按距离升序排列（手写冒泡排序用于小数据量）
        int n = (int)result.size();
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                if (result[j].distance > result[j + 1].distance) {
                    NearbyFoodItem tmp = result[j];
                    result[j] = result[j + 1];
                    result[j + 1] = tmp;
                }
            }
        }

        return result;
    }
};

#endif // BFS_H
