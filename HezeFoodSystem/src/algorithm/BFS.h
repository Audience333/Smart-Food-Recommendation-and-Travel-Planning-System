#ifndef BFS_H
#define BFS_H

#include <string>
#include "../structure/Graph.h"
#include "../structure/Queue.h"
#include "../structure/SeqList.h"

/**
 * 广度优先搜索（BFS）算法
 *
 * 业务用途：
 *   - 搜索附近 N 层范围内的美食/景点
 *   - "附近有什么好吃的"
 *   - 计算两点间的最少换乘次数
 *
 * 算法流程：
 *   1. 将起点加入队列，标记为已访问
 *   2. 循环取出队头节点
 *   3. 遍历当前节点的所有邻接节点
 *   4. 如果邻接节点未访问，加入队列
 *   5. 重复直到达到指定层数或队列为空
 *
 * 时间复杂度: O(V + E)，V=顶点数，E=边数
 * 空间复杂度: O(V)
 */
class BFS {
public:
    /**
     * 搜索结果结构体
     */
    struct SearchResult {
        int     vertexIndex;    // 节点索引
        int     layer;          // 所在层数（距离起点几跳）
        double  totalDistance;  // 累计距离（米）
        double  totalTime;      // 累计时间（分钟）

        SearchResult() : vertexIndex(0), layer(0), totalDistance(0), totalTime(0) {}
        SearchResult(int v, int l, double d, double t)
            : vertexIndex(v), layer(l), totalDistance(d), totalTime(t) {}
    };

    /**
     * 搜索指定层数范围内的所有节点
     *
     * @param graph 城市图
     * @param start 起点索引
     * @param maxLayer 最大层数（搜索范围）
     * @return 搜索结果列表
     *
     * 时间复杂度: O(V + E)
     * 空间复杂度: O(V)
     */
    template<typename T>
    static SeqList<SearchResult> searchLayers(const Graph<T>& graph,
                                               int start, int maxLayer) {
        SeqList<SearchResult> results;
        int n = graph.vertexCount();
        if (n == 0 || start < 0 || start >= n) return results;

        // visited 数组（用 SeqList 模拟）
        SeqList<int> visited;
        for (int i = 0; i < n; i++) visited.push_back(0);

        // 距离和时间数组
        SeqList<double> dist;
        SeqList<double> time;
        for (int i = 0; i < n; i++) {
            dist.push_back(0);
            time.push_back(0);
        }

        // BFS 队列：存储 {节点索引, 当前层数}
        Queue<int> nodeQueue;
        Queue<int> layerQueue;

        nodeQueue.enqueue(start);
        layerQueue.enqueue(0);
        visited[start] = 1;

        while (!nodeQueue.empty()) {
            int current = nodeQueue.front();
            int layer = layerQueue.front();
            nodeQueue.dequeue();
            layerQueue.dequeue();

            // 记录结果（排除起点自身）
            if (current != start) {
                results.push_back(SearchResult(current, layer, dist[current], time[current]));
            }

            // 如果已达最大层数，不再扩展
            if (layer >= maxLayer) continue;

            // 遍历邻接节点
            auto* neighbor = graph.getNeighbors(current);
            while (neighbor) {
                int next = neighbor->edge.to;
                if (!visited[next]) {
                    visited[next] = 1;
                    dist[next] = dist[current] + neighbor->edge.distance;
                    time[next] = time[current] + neighbor->edge.time;
                    nodeQueue.enqueue(next);
                    layerQueue.enqueue(layer + 1);
                }
                neighbor = neighbor->next;
            }
        }

        return results;
    }

    /**
     * 搜索指定范围（米）内的所有节点
     *
     * @param graph 城市图
     * @param start 起点索引
     * @param maxDistance 最大距离（米）
     * @return 搜索结果列表
     */
    template<typename T>
    static SeqList<SearchResult> searchByDistance(const Graph<T>& graph,
                                                   int start, double maxDistance) {
        SeqList<SearchResult> results;
        int n = graph.vertexCount();
        if (n == 0 || start < 0 || start >= n) return results;

        SeqList<int> visited;
        for (int i = 0; i < n; i++) visited.push_back(0);

        SeqList<double> dist;
        SeqList<double> time;
        for (int i = 0; i < n; i++) {
            dist.push_back(0);
            time.push_back(0);
        }

        Queue<int> nodeQueue;
        nodeQueue.enqueue(start);
        visited[start] = 1;

        while (!nodeQueue.empty()) {
            int current = nodeQueue.front();
            nodeQueue.dequeue();

            if (current != start) {
                results.push_back(SearchResult(current, 0, dist[current], time[current]));
            }

            auto* neighbor = graph.getNeighbors(current);
            while (neighbor) {
                int next = neighbor->edge.to;
                double newDist = dist[current] + neighbor->edge.distance;
                if (!visited[next] && newDist <= maxDistance) {
                    visited[next] = 1;
                    dist[next] = newDist;
                    time[next] = time[current] + neighbor->edge.time;
                    nodeQueue.enqueue(next);
                }
                neighbor = neighbor->next;
            }
        }

        return results;
    }

    /**
     * 计算两点间的最短跳数（最少换乘）
     *
     * @return 跳数，不可达返回 -1
     */
    template<typename T>
    static int shortestHops(const Graph<T>& graph, int start, int end) {
        int n = graph.vertexCount();
        if (n == 0 || start < 0 || start >= n || end < 0 || end >= n) return -1;
        if (start == end) return 0;

        SeqList<int> visited;
        SeqList<int> hops;
        for (int i = 0; i < n; i++) {
            visited.push_back(0);
            hops.push_back(-1);
        }

        Queue<int> queue;
        queue.enqueue(start);
        visited[start] = 1;
        hops[start] = 0;

        while (!queue.empty()) {
            int current = queue.front();
            queue.dequeue();

            auto* neighbor = graph.getNeighbors(current);
            while (neighbor) {
                int next = neighbor->edge.to;
                if (!visited[next]) {
                    visited[next] = 1;
                    hops[next] = hops[current] + 1;
                    if (next == end) return hops[next];
                    queue.enqueue(next);
                }
                neighbor = neighbor->next;
            }
        }

        return -1;  // 不可达
    }
};

#endif // BFS_H
