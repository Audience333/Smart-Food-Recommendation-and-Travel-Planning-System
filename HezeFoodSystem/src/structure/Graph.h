#ifndef GRAPH_H
#define GRAPH_H

#include <stdexcept>
#include <string>
#include <iostream>
#include <iomanip>

/**
 * 图模板类（邻接表实现）
 * 手写实现，用于城市路线网络
 *
 * 业务用途：
 *   - 景点/美食之间的路线连接
 *   - Dijkstra 最短路径计算
 *   - BFS 广度优先搜索
 *
 * 时间复杂度：
 *   - 添加顶点 O(1) 均摊
 *   - 添加边 O(1)
 *   - 查询邻接点 O(degree)
 *   - 空间 O(V + E)
 */
template<typename T>
class Graph {
public:
    // 边结构
    struct Edge {
        int     to;         // 目标节点索引
        double  distance;   // 距离（米）
        double  time;       // 预计时间（分钟）
        Edge(int t, double d, double tm) : to(t), distance(d), time(tm) {}
    };

private:
    // 邻接表节点
    struct AdjNode {
        Edge    edge;
        AdjNode* next;
        AdjNode(int to, double dist, double time, AdjNode* n = nullptr)
            : edge(to, dist, time), next(n) {}
    };

    AdjNode**   adjList_;       // 邻接表数组
    T*          vertices_;      // 顶点数据数组
    int         vertexCount_;   // 顶点数量
    int         edgeCount_;     // 边数量
    int         capacity_;      // 容量

    void expandCapacity() {
        int newCap = capacity_ * 2;
        AdjNode** newAdj = new AdjNode*[newCap]();
        T* newVertices = new T[newCap];
        for (int i = 0; i < vertexCount_; i++) {
            newAdj[i] = adjList_[i];
            newVertices[i] = vertices_[i];
        }
        delete[] adjList_;
        delete[] vertices_;
        adjList_ = newAdj;
        vertices_ = newVertices;
        capacity_ = newCap;
    }

public:
    explicit Graph(int initCapacity = 64)
        : vertexCount_(0), edgeCount_(0), capacity_(initCapacity) {
        adjList_ = new AdjNode*[capacity_]();
        vertices_ = new T[capacity_];
    }

    ~Graph() {
        clear();
        delete[] adjList_;
        delete[] vertices_;
    }

    // ==================== 顶点操作 ====================

    /**
     * 添加顶点
     * 时间复杂度: O(1) 均摊
     * @return 顶点索引
     */
    int addVertex(const T& data) {
        if (vertexCount_ >= capacity_) expandCapacity();
        vertices_[vertexCount_] = data;
        adjList_[vertexCount_] = nullptr;
        return vertexCount_++;
    }

    int vertexCount() const { return vertexCount_; }
    int edgeCount()   const { return edgeCount_; }

    T& getVertex(int index) {
        if (index < 0 || index >= vertexCount_)
            throw std::out_of_range("Graph: vertex index out of range");
        return vertices_[index];
    }

    const T& getVertex(int index) const {
        if (index < 0 || index >= vertexCount_)
            throw std::out_of_range("Graph: vertex index out of range");
        return vertices_[index];
    }

    // ==================== 边操作 ====================

    /**
     * 添加无向边（双向）
     * 时间复杂度: O(1)
     */
    void addEdge(int from, int to, double distance, double time) {
        if (from < 0 || from >= vertexCount_ || to < 0 || to >= vertexCount_)
            throw std::out_of_range("Graph: vertex index out of range");
        adjList_[from] = new AdjNode(to, distance, time, adjList_[from]);
        adjList_[to] = new AdjNode(from, distance, time, adjList_[to]);
        edgeCount_++;
    }

    /**
     * 添加有向边
     * 时间复杂度: O(1)
     */
    void addDirectedEdge(int from, int to, double distance, double time) {
        if (from < 0 || from >= vertexCount_ || to < 0 || to >= vertexCount_)
            throw std::out_of_range("Graph: vertex index out of range");
        adjList_[from] = new AdjNode(to, distance, time, adjList_[from]);
        edgeCount_++;
    }

    // ==================== 邻接查询 ====================

    /**
     * 获取节点的所有邻接边（返回链表头指针）
     * 时间复杂度: O(1)
     */
    AdjNode* getNeighbors(int vertex) const {
        if (vertex < 0 || vertex >= vertexCount_)
            throw std::out_of_range("Graph: vertex index out of range");
        return adjList_[vertex];
    }

    /**
     * 获取节点的度数
     * 时间复杂度: O(degree)
     */
    int getDegree(int vertex) const {
        if (vertex < 0 || vertex >= vertexCount_) return 0;
        int count = 0;
        for (AdjNode* p = adjList_[vertex]; p; p = p->next) count++;
        return count;
    }

    /**
     * 判断两点间是否有边
     * 时间复杂度: O(degree)
     */
    bool hasEdge(int from, int to) const {
        for (AdjNode* p = adjList_[from]; p; p = p->next) {
            if (p->edge.to == to) return true;
        }
        return false;
    }

    /**
     * 获取边的距离
     * 时间复杂度: O(degree)
     */
    double getEdgeDistance(int from, int to) const {
        for (AdjNode* p = adjList_[from]; p; p = p->next) {
            if (p->edge.to == to) return p->edge.distance;
        }
        return -1.0;
    }

    /**
     * 获取边的时间
     * 时间复杂度: O(degree)
     */
    double getEdgeTime(int from, int to) const {
        for (AdjNode* p = adjList_[from]; p; p = p->next) {
            if (p->edge.to == to) return p->edge.time;
        }
        return -1.0;
    }

    /**
     * 清空图
     */
    void clear() {
        for (int i = 0; i < vertexCount_; i++) {
            AdjNode* p = adjList_[i];
            while (p) {
                AdjNode* del = p;
                p = p->next;
                delete del;
            }
            adjList_[i] = nullptr;
        }
        vertexCount_ = 0;
        edgeCount_ = 0;
    }

    /**
     * 显示图的邻接关系
     */
    template<typename NameFunc>
    void displayGraph(NameFunc getName) const {
        std::cout << std::endl << "=== 城市地图 (" << vertexCount_
                  << " 个地点, " << edgeCount_ << " 条道路) ===" << std::endl;
        for (int i = 0; i < vertexCount_; i++) {
            std::cout << "  [" << i << "] " << getName(vertices_[i]) << " -> ";
            AdjNode* p = adjList_[i];
            if (!p) {
                std::cout << "(无连接)";
            }
            while (p) {
                std::cout << getName(vertices_[p->edge.to])
                          << "(" << std::fixed << std::setprecision(0)
                          << p->edge.distance << "m/" << std::setprecision(1)
                          << p->edge.time << "min)";
                if (p->next) std::cout << " -> ";
                p = p->next;
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
};

#endif // GRAPH_H
