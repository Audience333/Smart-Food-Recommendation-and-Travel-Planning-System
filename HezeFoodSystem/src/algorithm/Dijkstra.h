#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include <string>
#include "../structure/Graph.h"
#include "../structure/Heap.h"
#include "../structure/SeqList.h"

/**
 * Dijkstra 单源最短路径算法
 *
 * 业务用途：
 *   - 计算两个地点之间的最短路线
 *   - 输出路径经过的所有节点
 *   - 计算总距离和预计时间
 *
 * 算法流程：
 *   1. 初始化距离数组为无穷大，起点距离为 0
 *   2. 将起点加入最小堆
 *   3. 循环取出距离最小的节点
 *   4. 如果该节点已处理，跳过
 *   5. 遍历邻接节点，松弛边
 *   6. 如果发现更短路径，更新距离和前驱，加入堆
 *   7. 重复直到堆为空
 *
 * 时间复杂度: O((V + E) log V)，V=顶点数，E=边数
 * 空间复杂度: O(V)
 */
class Dijkstra {
public:
    /**
     * 路径结果结构体
     */
    struct PathResult {
        SeqList<int>    path;           // 路径节点索引列表
        double          totalDistance;  // 总距离（米）
        double          totalTime;      // 总时间（分钟）
        bool            found;          // 是否找到路径

        PathResult() : totalDistance(0), totalTime(0), found(false) {}
    };

private:
    // 堆节点（用于优先队列）
    struct HeapNode {
        int     vertex;     // 节点索引
        double  distance;   // 到起点的距离

        HeapNode() : vertex(0), distance(0) {}
        HeapNode(int v, double d) : vertex(v), distance(d) {}

        // 最小堆比较
        bool operator<(const HeapNode& other) const {
            return distance < other.distance;
        }
        bool operator>(const HeapNode& other) const {
            return distance > other.distance;
        }
    };

    static const double INF;  // 无穷大

public:
    /**
     * 计算最短路径（按距离）
     *
     * @param graph 城市图
     * @param start 起点索引
     * @param end 终点索引
     * @return 路径结果
     *
     * 时间复杂度: O((V + E) log V)
     * 空间复杂度: O(V)
     */
    template<typename T>
    static PathResult findShortestPath(const Graph<T>& graph, int start, int end) {
        PathResult result;
        int n = graph.vertexCount();
        if (n == 0 || start < 0 || start >= n || end < 0 || end >= n) {
            return result;
        }

        if (start == end) {
            result.path.push_back(start);
            result.found = true;
            return result;
        }

        // 初始化
        SeqList<double> dist;
        SeqList<double> time;
        SeqList<int> prev;
        SeqList<int> visited;
        for (int i = 0; i < n; i++) {
            dist.push_back(INF);
            time.push_back(0);
            prev.push_back(-1);
            visited.push_back(0);
        }
        dist[start] = 0;

        // 最小堆
        Heap<HeapNode> minHeap(true);
        minHeap.push(HeapNode(start, 0));

        while (!minHeap.empty()) {
            HeapNode current = minHeap.top();
            minHeap.pop();

            int u = current.vertex;

            // 如果已访问，跳过
            if (visited[u]) continue;
            visited[u] = 1;

            // 如果当前节点是终点，可以提前结束
            if (u == end) break;

            // 遍历邻接节点
            auto* neighbor = graph.getNeighbors(u);
            while (neighbor) {
                int v = neighbor->edge.to;
                double w = neighbor->edge.distance;

                // 松弛操作
                if (!visited[v] && dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    time[v] = time[u] + neighbor->edge.time;
                    prev[v] = u;
                    minHeap.push(HeapNode(v, dist[v]));
                }
                neighbor = neighbor->next;
            }
        }

        // 检查是否找到路径
        if (dist[end] >= INF) {
            return result;  // 不可达
        }

        // 回溯路径
        result.found = true;
        result.totalDistance = dist[end];
        result.totalTime = time[end];

        SeqList<int> reversePath;
        int current = end;
        while (current != -1) {
            reversePath.push_back(current);
            current = prev[current];
        }
        // 反转路径
        for (int i = reversePath.size() - 1; i >= 0; i--) {
            result.path.push_back(reversePath[i]);
        }

        return result;
    }

    /**
     * 计算最短路径（按时间）
     */
    template<typename T>
    static PathResult findFastestPath(const Graph<T>& graph, int start, int end) {
        PathResult result;
        int n = graph.vertexCount();
        if (n == 0 || start < 0 || start >= n || end < 0 || end >= n) {
            return result;
        }

        if (start == end) {
            result.path.push_back(start);
            result.found = true;
            return result;
        }

        // 初始化（这里用 time 作为主权重）
        SeqList<double> dist;
        SeqList<double> timeArr;
        SeqList<int> prev;
        SeqList<int> visited;
        for (int i = 0; i < n; i++) {
            dist.push_back(0);
            timeArr.push_back(INF);
            prev.push_back(-1);
            visited.push_back(0);
        }
        timeArr[start] = 0;

        Heap<HeapNode> minHeap(true);
        minHeap.push(HeapNode(start, 0));

        while (!minHeap.empty()) {
            HeapNode current = minHeap.top();
            minHeap.pop();

            int u = current.vertex;
            if (visited[u]) continue;
            visited[u] = 1;
            if (u == end) break;

            auto* neighbor = graph.getNeighbors(u);
            while (neighbor) {
                int v = neighbor->edge.to;
                double t = neighbor->edge.time;

                if (!visited[v] && timeArr[u] + t < timeArr[v]) {
                    timeArr[v] = timeArr[u] + t;
                    dist[v] = dist[u] + neighbor->edge.distance;
                    prev[v] = u;
                    minHeap.push(HeapNode(v, timeArr[v]));
                }
                neighbor = neighbor->next;
            }
        }

        if (timeArr[end] >= INF) return result;

        result.found = true;
        result.totalDistance = dist[end];
        result.totalTime = timeArr[end];

        SeqList<int> reversePath;
        int current = end;
        while (current != -1) {
            reversePath.push_back(current);
            current = prev[current];
        }
        for (int i = reversePath.size() - 1; i >= 0; i--) {
            result.path.push_back(reversePath[i]);
        }

        return result;
    }
};

const double Dijkstra::INF = 1e18;

#endif // DIJKSTRA_H
