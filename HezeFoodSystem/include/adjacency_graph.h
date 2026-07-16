/*
 * ============================================================================
 * adjacency_graph.h — 邻接表无向图（手写C++实现）
 * ============================================================================
 *
 * 【数据结构说明】
 *   使用邻接表存储无向图。每个顶点维护一个出边列表，无向图的每条边
 *   在两端顶点的邻接表中各存储一份。
 *
 * 【顶点与边的设计】
 *   Vertex: id(唯一标识), name(名称), lng/lat(经纬度), type(类型)
 *   Edge:   to(邻接表索引), toId(目标顶点ID), distance(距离米), time(时间分钟)
 *
 * 【idToIndex 映射】
 *   idToIndex[顶点ID] = 顶点在vertices数组中的索引，支持O(1)定位。
 *   由于实际ID范围较小(1~500)，使用固定大小数组实现。
 *
 * 【时间复杂度】
 *   addVertex:     O(1) 均摊
 *   addEdge:       O(1) 均摊
 *   getNeighbors:  O(degree)
 *
 * 【空间复杂度】
 *   O(V + E)，V=顶点数，E=边数
 *
 * 【使用场景】
 *   用于表示城市路网中的POI节点（景点、美食）及其连接关系。
 *   配合Dijkstra和BFS算法实现最短路径查找和周边搜索。
 *
 * 【使用示例】
 *   AdjacencyGraph graph;
 *   graph.addVertex(1, "曹州牡丹园", 115.49, 35.28, "spot");
 *   graph.addVertex(101, "菏泽烧牛肉", 115.50, 35.24, "food");
 *   graph.addEdge(1, 101, 5000.0, 15.0);
 *   auto neighbors = graph.getNeighbors(1);
 * ============================================================================
 */

#ifndef ADJACENCY_GRAPH_H
#define ADJACENCY_GRAPH_H

#include <cstddef>
#include <cstring>
#include <algorithm>

/* ====================================================================
 * 顶点结构体
 * ====================================================================
 * 存储图中一个节点的基本信息：ID、名称、经纬度坐标、类型。
 */
struct Vertex {
    int id;                 // 唯一标识符
    char name[256];         // 名称（固定长度字符数组）
    double lng, lat;        // 经纬度坐标（WGS84）
    char type[32];          // 类型："spot"（景点）或 "food"（美食）
};

/* ====================================================================
 * 边结构体
 * ====================================================================
 * 存储在邻接表中，表示从当前顶点出发到目标顶点的一条有向边。
 * 无向图会在两端顶点各存储一条边。
 */
struct Edge {
    int to;                 // 目标顶点在vertices数组中的索引
    int toId;               // 目标顶点的ID
    double distance;        // 距离（米）
    double time;            // 预计通行时间（分钟）
};

// 初始容量常量
static const int AG_INIT_VERTEX_CAP = 512;   // 顶点数组初始容量
static const int AG_INIT_EDGE_CAP = 16;      // 每个邻接表的初始容量

/* ====================================================================
 * AdjacencyGraph — 邻接表无向图类
 * ====================================================================
 * 不使用STL容器，全部使用原始动态数组实现。
 * 顶点数组：Vertex* vertices，动态扩容（×2策略）
 * 邻接表：  Edge** adjList[i] 为每个顶点的出边数组
 */
class AdjacencyGraph {
private:
    Vertex* vertices;           // 顶点动态数组
    int vertexCount;            // 当前顶点数量
    int vertexCapacity;         // 顶点数组容量

    Edge** adjList;             // 邻接表：adjList[i] 是第i个顶点的出边数组
    int* adjCount;              // 每个顶点的邻接边数量
    int* adjCapacity;           // 每个顶点的邻接数组容量

    int* idToIndex;             // ID→索引映射：idToIndex[id] = 顶点索引
    int idToIndexSize;          // 映射表大小

    /* ---- 确保idToIndex数组能容纳指定ID ---- */
    void ensureIdMap(int maxId) {
        if (maxId >= idToIndexSize) {
            int newSize = maxId + 256;
            int* newMap = new int[newSize];
            for (int i = 0; i < newSize; i++) {
                newMap[i] = -1;  // -1表示不存在
            }
            // 拷贝已有映射
            int copyCount = idToIndexSize;
            if (copyCount > newSize) copyCount = newSize;
            for (int i = 0; i < copyCount; i++) {
                newMap[i] = idToIndex[i];
            }
            delete[] idToIndex;
            idToIndex = newMap;
            idToIndexSize = newSize;
        }
    }

    /* ---- 扩展顶点数组容量 ---- */
    void expandVertices() {
        int newCap = vertexCapacity * 2;
        if (newCap < AG_INIT_VERTEX_CAP) newCap = AG_INIT_VERTEX_CAP;

        Vertex* newVerts = new Vertex[newCap];
        for (int i = 0; i < vertexCount; i++) {
            newVerts[i] = vertices[i];
        }
        delete[] vertices;
        vertices = newVerts;

        Edge** newAdj = new Edge*[newCap];
        int* newAdjCnt = new int[newCap];
        int* newAdjCap = new int[newCap];
        for (int i = 0; i < vertexCount; i++) {
            newAdj[i] = adjList[i];
            newAdjCnt[i] = adjCount[i];
            newAdjCap[i] = adjCapacity[i];
        }
        delete[] adjList;
        delete[] adjCount;
        delete[] adjCapacity;
        adjList = newAdj;
        adjCount = newAdjCnt;
        adjCapacity = newAdjCap;

        vertexCapacity = newCap;
    }

    /* ---- 扩展指定顶点的邻接表容量 ---- */
    void expandAdjacency(int idx) {
        int newCap = adjCapacity[idx] * 2;
        if (newCap < AG_INIT_EDGE_CAP) newCap = AG_INIT_EDGE_CAP;
        Edge* newEdges = new Edge[newCap];
        for (int i = 0; i < adjCount[idx]; i++) {
            newEdges[i] = adjList[idx][i];
        }
        delete[] adjList[idx];
        adjList[idx] = newEdges;
        adjCapacity[idx] = newCap;
    }

public:
    /* ---- 构造：分配初始容量 ---- */
    AdjacencyGraph() : vertexCount(0), vertexCapacity(AG_INIT_VERTEX_CAP) {
        vertices = new Vertex[vertexCapacity];
        adjList = new Edge*[vertexCapacity];
        adjCount = new int[vertexCapacity];
        adjCapacity = new int[vertexCapacity];
        for (int i = 0; i < vertexCapacity; i++) {
            adjList[i] = nullptr;
            adjCount[i] = 0;
            adjCapacity[i] = 0;
        }

        idToIndexSize = 1024;  // 初始ID映射表大小
        idToIndex = new int[idToIndexSize];
        for (int i = 0; i < idToIndexSize; i++) {
            idToIndex[i] = -1;
        }
    }

    /* ---- 析构：释放所有动态内存 ---- */
    ~AdjacencyGraph() {
        for (int i = 0; i < vertexCount; i++) {
            if (adjList[i] != nullptr) {
                delete[] adjList[i];
            }
        }
        delete[] vertices;
        delete[] adjList;
        delete[] adjCount;
        delete[] adjCapacity;
        delete[] idToIndex;
    }

    /* ====================================================================
     * addVertex — 添加顶点
     * ====================================================================
     * 向图中添加一个POI节点。如果ID已存在则忽略。
     * 参数：
     *   id   - 唯一标识符
     *   name - 节点名称
     *   lng  - 经度
     *   lat  - 纬度
     *   type - 类型字符串（"spot"或"food"）
     * 返回：新顶点的索引；若ID已存在则返回已有索引
     */
    int addVertex(int id, const char* name, double lng, double lat, const char* type) {
        // 确保ID映射表足够大
        ensureIdMap(id);

        // 检查ID是否已存在
        if (idToIndex[id] != -1) {
            return idToIndex[id];
        }

        // 扩展顶点数组（如需要）
        if (vertexCount >= vertexCapacity) {
            expandVertices();
        }

        int idx = vertexCount;
        vertices[idx].id = id;
        // 安全拷贝名称（限制长度避免溢出）
        int nameLen = (int)strlen(name);
        if (nameLen > 255) nameLen = 255;
        for (int i = 0; i < nameLen; i++) vertices[idx].name[i] = name[i];
        vertices[idx].name[nameLen] = '\0';

        vertices[idx].lng = lng;
        vertices[idx].lat = lat;

        int typeLen = (int)strlen(type);
        if (typeLen > 31) typeLen = 31;
        for (int i = 0; i < typeLen; i++) vertices[idx].type[i] = type[i];
        vertices[idx].type[typeLen] = '\0';

        // 初始化邻接表
        adjList[idx] = nullptr;
        adjCount[idx] = 0;
        adjCapacity[idx] = 0;

        idToIndex[id] = idx;
        vertexCount++;

        return idx;
    }

    /* ====================================================================
     * addEdge — 添加无向边
     * ====================================================================
     * 在两个顶点之间建立连接（分别在两端邻接表中各添加一条边）。
     * 参数：
     *   fromId/toId - 起点和终点顶点ID
     *   distance    - 距离（米）
     *   time        - 预计通行时间（分钟）
     * 返回：成功返回true，任一顶点不存在返回false
     */
    bool addEdge(int fromId, int toId, double distance, double time) {
        int fromIdx = getIndexById(fromId);
        int toIdx = getIndexById(toId);
        if (fromIdx == -1 || toIdx == -1) return false;

        // 在from的邻接表中添加边
        if (adjCount[fromIdx] >= adjCapacity[fromIdx]) {
            expandAdjacency(fromIdx);
        }
        adjList[fromIdx][adjCount[fromIdx]].to = toIdx;
        adjList[fromIdx][adjCount[fromIdx]].toId = toId;
        adjList[fromIdx][adjCount[fromIdx]].distance = distance;
        adjList[fromIdx][adjCount[fromIdx]].time = time;
        adjCount[fromIdx]++;

        // 在to的邻接表中添加反向边
        if (adjCount[toIdx] >= adjCapacity[toIdx]) {
            expandAdjacency(toIdx);
        }
        adjList[toIdx][adjCount[toIdx]].to = fromIdx;
        adjList[toIdx][adjCount[toIdx]].toId = fromId;
        adjList[toIdx][adjCount[toIdx]].distance = distance;
        adjList[toIdx][adjCount[toIdx]].time = time;
        adjCount[toIdx]++;

        return true;
    }

    /* ====================================================================
     * getNeighbors — 获取指定顶点的所有邻接边
     * ====================================================================
     * 参数：vertexId - 顶点ID
     * 返回：若顶点存在，返回其邻接边数组指针和数量；
     *       注意返回的是内部数组指针，调用者不应释放
     */
    const Edge* getNeighbors(int vertexId, int& outCount) const {
        int idx = getIndexById(vertexId);
        if (idx == -1) {
            outCount = 0;
            return nullptr;
        }
        outCount = adjCount[idx];
        return adjList[idx];
    }

    /* ---- 获取顶点总数 ---- */
    int vertexCount_() const { return vertexCount; }

    /* ---- 获取边总数（无向图每条边算一次，除以2） ---- */
    int edgeCount() const {
        int total = 0;
        for (int i = 0; i < vertexCount; i++) {
            total += adjCount[i];
        }
        return total / 2;  // 无向图，每条边被计数两次
    }

    /* ---- 根据ID查找索引 ---- */
    int getIndexById(int id) const {
        if (id < 0 || id >= idToIndexSize) return -1;
        return idToIndex[id];
    }

    /* ---- 根据索引获取顶点指针 ---- */
    const Vertex* getVertexByIndex(int idx) const {
        if (idx < 0 || idx >= vertexCount) return nullptr;
        return &vertices[idx];
    }

    /* ---- 根据ID获取顶点指针 ---- */
    const Vertex* getVertexById(int id) const {
        int idx = getIndexById(id);
        if (idx == -1) return nullptr;
        return &vertices[idx];
    }

    /* ---- 判断顶点是否存在 ---- */
    bool hasVertex(int id) const {
        return getIndexById(id) != -1;
    }

    /* ====================================================================
     * getVertexIndex — 获取顶点索引（兼容旧接口）
     * ====================================================================
     * 同 getIndexById，提供更语义化的别名。
     */
    int getVertexIndex(int id) const {
        return getIndexById(id);
    }
};

#endif // ADJACENCY_GRAPH_H
