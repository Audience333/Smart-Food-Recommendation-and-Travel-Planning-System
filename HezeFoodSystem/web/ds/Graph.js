/**
 * 邻接表无向图 (Adjacency List Undirected Graph)
 * 
 * 用于搭建城市商圈路网模型，存储POI顶点和道路边
 * 支撑上层算法: Dijkstra最短路径 / BFS限时美食圈搜索
 * 
 * 存储结构设计:
 *   vertices[]:     顶点数组, 存储 {id, name, lng, lat, type} 对象
 *   adjList[]:      邻接表, adjList[i] 存储顶点i的所有邻接边数组
 *   idToIndex{}:    ID→索引哈希映射, O(1)根据POI ID查找顶点索引
 * 
 * 顶点类型(type):
 *   - 'food':       美食POI (餐厅/小吃摊/饮品店)
 *   - 'attraction': 景点POI (公园/博物馆/地标)
 *   - 'transport':  交通枢纽POI (火车站/公交站)
 *   - 'intersection': 道路交叉点(辅助节点)
 * 
 * 边的权重:
 *   - distance: 两顶点间的实际距离(米) - 用于最短路径计算
 *   - time:     两顶点间的预估通行时间(分钟) - 用于时间最优路径
 * 
 * 图示例(菏泽市区小型路网):
 *   vertices: [火车站, 曹州牡丹园, 银座商城, 小吃街A, ...]
 *   edges:   火车站-曹州牡丹园(5000m,30min), 曹州牡丹园-小吃街A(800m,5min)...
 * 
 * 使用示例:
 *   var graph = new AdjacencyGraph();
 *   var idx1 = graph.addVertex({ id: 1, name: '火车站', lng: 115.5, lat: 35.2, type: 'transport' });
 *   var idx2 = graph.addVertex({ id: 5, name: '曹州牡丹园', lng: 115.48, lat: 35.25, type: 'attraction' });
 *   graph.addEdge(1, 5, 5000, 30);
 */
class AdjacencyGraph {
    constructor() {
        this.vertices = [];       // 顶点数组: [{id, name, lng, lat, type}, ...]
        this.adjList = [];        // 邻接表: adjList[i] = [{to, toId, distance, time}, ...]
        this.idToIndex = {};      // 顶点ID → 数组下标的O(1)映射表
    }

    /**
     * 添加顶点到图中
     * 
     * 自动分配索引号, 同步更新idToIndex哈希映射
     * 为新顶点创建空的邻接边列表
     * 
     * @param {Object} vertex - 顶点信息
     *   @property {number} vertex.id - POI唯一标识ID
     *   @property {string} vertex.name - POI名称
     *   @property {number} vertex.lng - 经度 (longitude)
     *   @property {number} vertex.lat - 纬度 (latitude)
     *   @property {string} vertex.type - 顶点类型 ('food'/'attraction'/'transport'/'intersection')
     * @returns {number} 新顶点的数组索引
     */
    addVertex(vertex) {
        var index = this.vertices.length;
        this.vertices.push(vertex);
        this.adjList.push([]);        // 为新顶点创建空邻接列表
        this.idToIndex[vertex.id] = index; // 注册ID→索引映射
        return index;
    }

    /**
     * 添加无向边 (双向通行道路)
     * 
     * 无向图特性: 一条边需要在两端顶点的邻接表中各添加一条记录
     * 表示两个方向均可通行
     * 
     * 操作:
     *   adjList[fromIndex].push({ to: toIndex, toId: toId, distance, time })
     *   adjList[toIndex].push({ to: fromIndex, toId: fromId, distance, time })
     * 
     * 边对象字段说明:
     *   - to:        目标顶点在vertices中的索引(整数, 用于O(1)访问)
     *   - toId:      目标顶点的原始ID(用于路径输出时查找)
     *   - distance:  路段距离(米), Dijkstra距离优先的权重
     *   - time:      通行时间(分钟), Dijkstra时间优先的权重
     * 
     * @param {number} fromId - 起点的POI ID
     * @param {number} toId - 终点的POI ID
     * @param {number} distance - 道路距离, 单位: 米
     * @param {number} time - 预估通行时间, 单位: 分钟
     * @returns {boolean} true=添加成功, false=顶点不存在
     */
    addEdge(fromId, toId, distance, time) {
        var fi = this.idToIndex[fromId];
        var ti = this.idToIndex[toId];
        if (fi === undefined || ti === undefined) return false;
        // 无向图: 在两端各插入一条方向相反的边记录
        this.adjList[fi].push({ to: ti, toId: toId, distance: distance, time: time });
        this.adjList[ti].push({ to: fi, toId: fromId, distance: distance, time: time });
        return true;
    }

    /**
     * 获取指定顶点的所有邻接边
     * 
     * 返回该顶点连接的所有边的信息
     * 用于Dijkstra/BFS算法中的邻居扩展
     * 
     * @param {number} vertexIndex - 顶点在vertices数组中的索引
     * @returns {Array} 邻居边数组 [{to, toId, distance, time}, ...]
     *   - 顶点不存在时返回空数组[]
     */
    getNeighbors(vertexIndex) {
        return this.adjList[vertexIndex] || [];
    }

    /**
     * 获取顶点总数
     * @returns {number} |V| 图中顶点数量
     */
    vertexCount() { return this.vertices.length; }

    /**
     * 获取边总数 (无向边数量 = 邻接表总条目 / 2)
     * 因为每条无向边在邻接表中被记录两次
     * @returns {number} |E| 图中边的数量
     */
    edgeCount() {
        var count = 0;
        for (var i = 0; i < this.adjList.length; i++) {
            count += this.adjList[i].length;
        }
        return count / 2; // 无向边被算了两次，除以2得到实际边数
    }

    /**
     * 根据POI ID获取顶点信息
     * @param {number} id - POI的原始ID
     * @returns {Object|null} 顶点对象 {id, name, lng, lat, type}, 未找到返回null
     */
    getVertexById(id) {
        var index = this.idToIndex[id];
        return index !== undefined ? this.vertices[index] : null;
    }

    /**
     * 根据POI ID获取顶点在数组中的索引
     * @param {number} id - POI的原始ID
     * @returns {number} 顶点索引, 未找到返回-1
     */
    getIndexById(id) {
        var idx = this.idToIndex[id];
        return idx !== undefined ? idx : -1;
    }

    /**
     * 获取图中所有指定类型的顶点
     * @param {string} type - 顶点类型 ('food'/'attraction'/'transport')
     * @returns {Array} 符合条件的顶点数组
     */
    getVerticesByType(type) {
        var result = [];
        for (var i = 0; i < this.vertices.length; i++) {
            if (this.vertices[i].type === type) {
                result.push(this.vertices[i]);
            }
        }
        return result;
    }

    /**
     * 获取所有顶点
     * @returns {Array} 顶点数组的拷贝
     */
    getAllVertices() {
        return this.vertices.slice();
    }
}

// 支持CommonJS模块加载
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { AdjacencyGraph };
}
