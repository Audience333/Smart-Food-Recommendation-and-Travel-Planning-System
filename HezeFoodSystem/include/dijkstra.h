/*
 * ============================================================================
 * dijkstra.h — Dijkstra最短路径算法 + 路径还原（手写C++实现）
 * ============================================================================
 *
 * 【算法说明】
 *   Dijkstra算法用于在带非负权重的图中查找单源最短路径。
 *   本实现包含：
 *     1. 手写最小二叉堆（不使用STL priority_queue）
 *     2. 路径还原（_reconstructPath方法）
 *     3. 支持按距离或时间作为权重
 *
 * 【手写最小堆设计】
 *   MinHeap 存储 (顶点索引, 当前最短距离) 对。
 *   标准二叉堆操作：siftUp（上滤）和 siftDown（下滤）。
 *
 * 【时间复杂度】
 *   O((V + E) log V)，V=顶点数，E=边数
 *   使用原始二叉堆实现。
 *
 * 【空间复杂度】
 *   O(V)，存储距离数组、前驱数组、访问标记、堆
 *
 * 【使用场景】
 *   在路网图中查找两点之间的最短驾车/步行路径。
 *
 * 【使用示例】
 *   Dijkstra dijkstra(graph);
 *   auto result = dijkstra.shortestPath(1, 101, "distance");
 *   printf("距离: %.1f米, 路径顶点数: %zu\n", result.distance, result.path.size());
 * ============================================================================
 */

#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "adjacency_graph.h"
#include <cstddef>
#include <cfloat>
#include <vector>

using std::vector;

/* ====================================================================
 * 最短路径结果结构体
 * ====================================================================
 * distance: 最短距离（米）或时间（分钟），取决于weightType
 * path:     从起点到终点的顶点ID序列（含起点和终点）
 */
struct ShortestPathResult {
    double distance;
    vector<int> path;
};

/* ====================================================================
 * HeapNode — 二叉堆节点（存储顶点索引和距离）
 * ====================================================================
 */
struct HeapNode {
    int vertexIndex;      // 顶点在图中的索引
    double dist;          // 当前最短距离估算值
};

/* ====================================================================
 * Dijkstra — Dijkstra最短路径算法类
 * ====================================================================
 *
 * 【算法流程】
 *   1) 初始化：起点距离=0，其他顶点距离=∞
 *   2) 将起点加入最小堆
 *   3) 循环：从堆中弹出距离最小的顶点
 *   4) 对弹出顶点的所有邻接边执行松弛操作
 *   5) 若邻接点距离被更新，将其加入堆
 *   6) 重复直到堆空或找到终点
 *   7) 通过前驱数组还原路径
 *
 * 【权重类型】
 *   "distance" - 按距离（米）计算最短路径
 *   "time"     - 按时间（分钟）计算最短路径
 *   其他值     - 默认按距离
 */
class Dijkstra {
private:
    const AdjacencyGraph& graph;       // 图的常量引用

    /* ---- 手写最小二叉堆：上滤操作 ---- */
    static void siftUp(HeapNode* heap, int idx) {
        while (idx > 0) {
            int parent = (idx - 1) / 2;
            if (heap[idx].dist < heap[parent].dist) {
                // 交换当前节点和父节点
                HeapNode tmp = heap[idx];
                heap[idx] = heap[parent];
                heap[parent] = tmp;
                idx = parent;
            } else {
                break;
            }
        }
    }

    /* ---- 手写最小二叉堆：下滤操作 ---- */
    static void siftDown(HeapNode* heap, int heapSize, int idx) {
        while (true) {
            int left = 2 * idx + 1;
            int right = 2 * idx + 2;
            int smallest = idx;

            if (left < heapSize && heap[left].dist < heap[smallest].dist) {
                smallest = left;
            }
            if (right < heapSize && heap[right].dist < heap[smallest].dist) {
                smallest = right;
            }
            if (smallest != idx) {
                HeapNode tmp = heap[idx];
                heap[idx] = heap[smallest];
                heap[smallest] = tmp;
                idx = smallest;
            } else {
                break;
            }
        }
    }

    /* ====================================================================
     * _reconstructPath — 根据前驱数组还原路径
     * ====================================================================
     * 从终点开始，通过 prev 数组反向追溯到起点，然后反转得到正向路径。
     * 参数：
     *   prev    - 前驱顶点索引数组（prev[i]=i的前驱顶点索引）
     *   startIdx - 起点在vertices中的索引
     *   endIdx   - 终点在vertices中的索引
     * 返回：从起点到终点的顶点ID序列
     */
    vector<int> reconstructPath(int* prev, int startIdx, int endIdx) const {
        vector<int> path;

        // 从终点反向追溯到起点
        int current = endIdx;
        while (current != -1) {
            const Vertex* v = graph.getVertexByIndex(current);
            if (v != nullptr) {
                path.push_back(v->id);
            }
            if (current == startIdx) break;
            current = prev[current];
        }

        // 反转得到正向路径
        int n = (int)path.size();
        for (int i = 0; i < n / 2; i++) {
            int tmp = path[i];
            path[i] = path[n - 1 - i];
            path[n - 1 - i] = tmp;
        }

        return path;
    }

public:
    /* ---- 构造：绑定到指定图 ---- */
    explicit Dijkstra(const AdjacencyGraph& g) : graph(g) {}

    /* ====================================================================
     * shortestPath — 计算两点间最短路径
     * ====================================================================
     * 参数：
     *   startId    - 起点顶点ID
     *   endId      - 终点顶点ID
     *   weightType - 权重类型："distance"（按距离）或 "time"（按时间）
     * 返回：ShortestPathResult，包含最短距离和路径序列
     *       若路径不存在，distance=DBL_MAX，path为空
     */
    ShortestPathResult shortestPath(int startId, int endId, const char* weightType = "distance") {
        ShortestPathResult result;
        result.distance = DBL_MAX;

        int V = graph.vertexCount_();
        if (V == 0) return result;

        int startIdx = graph.getIndexById(startId);
        int endIdx = graph.getIndexById(endId);
        if (startIdx == -1 || endIdx == -1) return result;
        if (startIdx == endIdx) {
            result.distance = 0;
            result.path.push_back(startId);
            return result;
        }

        // 分配手写数组（不依赖STL容器）
        double* dist = new double[V];     // 最短距离估算值
        bool* visited = new bool[V];      // 访问标记
        int* prev = new int[V];           // 前驱顶点索引（用于路径还原）

        for (int i = 0; i < V; i++) {
            dist[i] = DBL_MAX;
            visited[i] = false;
            prev[i] = -1;
        }
        dist[startIdx] = 0;

        // 手写最小二叉堆
        int heapCapacity = V + 16;
        HeapNode* heap = new HeapNode[heapCapacity];
        int heapSize = 0;

        // 将起点加入堆
        heap[heapSize].vertexIndex = startIdx;
        heap[heapSize].dist = 0;
        heapSize++;

        // 判断使用距离还是时间作为边权重
        bool useTime = (weightType[0] == 't' || weightType[0] == 'T');

        // 主循环：从堆中弹出最小距离顶点，松弛其邻接边
        while (heapSize > 0) {
            // 弹出堆顶（最小距离顶点）
            HeapNode top = heap[0];
            heap[0] = heap[heapSize - 1];
            heapSize--;
            if (heapSize > 0) {
                siftDown(heap, heapSize, 0);
            }

            int u = top.vertexIndex;
            if (visited[u]) continue;
            visited[u] = true;

            // 找到终点可提前退出
            if (u == endIdx) break;

            // 松弛所有邻接边
            int neighborCount = 0;
            const Edge* edges = graph.getNeighbors(
                graph.getVertexByIndex(u)->id, neighborCount);

            for (int i = 0; i < neighborCount; i++) {
                int v = edges[i].to;  // 邻接顶点索引

                if (visited[v]) continue;

                // 根据权重类型选择边的权重
                double weight = useTime ? edges[i].time : edges[i].distance;
                double newDist = dist[u] + weight;

                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    prev[v] = u;

                    // 将更新后的顶点加入堆
                    if (heapSize >= heapCapacity) {
                        // 堆扩容（实际场景很少触发）
                        int newCap = heapCapacity * 2;
                        HeapNode* newHeap = new HeapNode[newCap];
                        for (int j = 0; j < heapSize; j++) {
                            newHeap[j] = heap[j];
                        }
                        delete[] heap;
                        heap = newHeap;
                        heapCapacity = newCap;
                    }
                    heap[heapSize].vertexIndex = v;
                    heap[heapSize].dist = newDist;
                    heapSize++;
                    siftUp(heap, heapSize - 1);
                }
            }
        }

        // 收集结果
        if (dist[endIdx] < DBL_MAX) {
            result.distance = dist[endIdx];
            result.path = reconstructPath(prev, startIdx, endIdx);
        }

        // 释放手写数组
        delete[] dist;
        delete[] visited;
        delete[] prev;
        delete[] heap;

        return result;
    }
};

#endif // DIJKSTRA_H
