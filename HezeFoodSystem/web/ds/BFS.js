/**
 * 队列BFS广度优先搜索 (Breadth-First Search)
 * 
 * 从任意起点出发，按层遍历路网图，找出指定距离/跳数范围内的所有美食POI
 * 用于实现"限时美食圈"功能: 在距离起点X米范围内查找所有可到达的美食
 * 
 * BFS与Dijkstra的区别:
 *   - Dijkstra: 加权最短路径(单源单目标), 贪心+堆, 适用于精确导航
 *   - BFS:      无权/均匀遍历(单源多目标), 队列FIFO, 适用于范围查询
 * 
 * 算法核心思想:
 *   1. 从起点入队, 标记已访问
 *   2. 循环: 队首出队, 展开其所有未访问邻接点, 计算累积距离
 *   3. 若累积距离超过maxDistance阈值, 不入队(剪枝优化)
 *   4. 出队时若是美食POI, 收集到结果集
 *   5. 队列为空时终止
 * 
 * 手写队列说明:
 *   使用数组模拟FIFO队列: push入队, shift出队
 *   - 入队 O(1)
 *   - 出队 O(n) 数组shift
 *   对于中小规模路网(节点<1000)完全够用
 *   大规模场景可替换为循环队列优化O(1)出队
 * 
 * 时间复杂度: O(V + E)
 *   - 每个顶点入队至多一次 O(V)
 *   - 每条边被检查至多一次 O(E)
 * 
 * 空间复杂度: O(V)
 *   - 队列: 最坏包含所有顶点 O(V)
 *   - visited/dist/hops/prev: 各O(V)
 * 
 * 使用示例:
 *   var graph = new AdjacencyGraph();
 *   // ... 构建路网图 ...
 *   var bfs = new BFS(graph);
 *   var foods = bfs.nearbyFood(cityCenterId, 3000);
 *   // 返回距市中心3000米内的所有美食列表
 *   // [{ id: 10, name: '曹县烤全羊', distance: 1200, hops: 3, ... }, ...]
 */
class BFS {
    /**
     * @param {AdjacencyGraph} graph - 邻接表无向图
     *   - 存储顶点(含美食POI)和边的路网图
     */
    constructor(graph) {
        this.graph = graph;
    }

    /**
     * 限时美食圈查询: 找出距离起点不超过maxDistance的所有美食
     * 
     * 处理流程:
     *   1. 起点入队, 初始化dist[start]=0, hops[start]=0
     *   2. 循环出队:
     *      a. 若出队顶点是美食类型且距起点<=maxDistance, 收集到结果
     *      b. 遍历其所有未访问的邻接点:
     *         - 标记已访问
     *         - 累积距离 = 当前距离 + 边距离
     *         - 累积跳数 = 当前跳数 + 1
     *         - 无论累积距离是否超限, 都入队继续展开(因为其邻居可能更近)
     *           (注意: 这里不做距离剪枝, 保证所有可达顶点都被考虑)
     *   3. 结果按距离升序排序后返回
     * 
     * 性能说明:
     *   不按距离剪枝的原因:
     *   - BFS按层展开, 若提前剪枝可能漏掉通过短边绕路到达的美食
     *   - 所有顶点都会入队一次, 保证完整性
     *   - 结果收集时用dist[u] <= maxDistance过滤
     * 
     * @param {number} startId - 起点POI ID (可以是任意类型: 景点/交通枢纽/当前位置)
     * @param {number} maxDistance - 最大搜索距离, 单位: 米
     *   - 例如3000表示搜索3公里范围内的美食
     *   - 结果仅包含累积距离 <= maxDistance 的美食
     * @returns {Array} 范围内的美食列表, 按距离升序排列
     *   每项格式:
     *   {
     *     id: number,        // POI ID
     *     name: string,      // 美食名称
     *     distance: number,  // 从起点到此美食的累积距离(米)
     *     hops: number,      // 从起点到此美食经过的边数(跳数)
     *     lng: number,       // 经度
     *     lat: number,       // 纬度
     *     path: Array        // 从起点到此美食的顶点索引路径(用于地图绘制)
     *   }
     */
    nearbyFood(startId, maxDistance) {
        var startIdx = this.graph.getIndexById(startId);
        if (startIdx < 0) return [];

        var V = this.graph.vertexCount();
        var visited = new Array(V).fill(false); // 访问标记
        var dist = new Array(V).fill(Infinity);  // 累积距离
        var prev = new Array(V).fill(-1);        // 前驱节点(路径还原用)
        var hops = new Array(V).fill(Infinity);  // 跳数(经过的边数)

        // 手写队列 (FIFO先进先出)
        var queue = [];
        queue.push(startIdx);
        visited[startIdx] = true;
        dist[startIdx] = 0;
        hops[startIdx] = 0;

        var results = [];

        while (queue.length > 0) {
            // 出队: 取出队首元素
            var u = queue.shift();

            // 检查当前顶点是否是美食类型 (且不是起点本身)
            var vertex = this.graph.vertices[u];
            if (vertex && vertex.type === 'food' && u !== startIdx && dist[u] <= maxDistance) {
                results.push({
                    id: vertex.id,
                    name: vertex.name,
                    distance: dist[u],
                    hops: hops[u],
                    lng: vertex.lng,
                    lat: vertex.lat,
                    path: this._reconstructPath(prev, startIdx, u)
                });
            }

            // 展开所有邻接边
            var neighbors = this.graph.getNeighbors(u);
            for (var i = 0; i < neighbors.length; i++) {
                var e = neighbors[i];
                var v = e.to;
                if (visited[v]) continue; // 已访问则跳过(防止重复入队)
                visited[v] = true;
                dist[v] = dist[u] + e.distance;
                hops[v] = hops[u] + 1;
                prev[v] = u;
                // 入队 (即使距离已超限也继续展开, 因为邻居中的美食可能通过更短的后续边满足条件)
                queue.push(v);
            }
        }

        // 按距离升序排列结果
        results.sort(function(a, b) { return a.distance - b.distance; });
        return results;
    }

    /**
     * 路径还原: 从终点回溯到起点
     * 
     * 通过prev[]前驱数组, 从终点逐一回溯到起点
     * 构建从起点到终点的完整顶点索引路径
     * 
     * 安全机制: 限制最大回溯100步, 防止prev数组异常导致无限循环
     * 
     * @param {Array} prev - 前驱数组, prev[v]=u 表示u是v的前驱
     * @param {number} start - 起点顶点索引
     * @param {number} end - 终点顶点索引
     * @returns {Array} 从起点到终点的顶点索引数组
     * @private
     */
    _reconstructPath(prev, start, end) {
        var path = [];
        var current = end;
        while (current !== -1 && current !== start) {
            path.unshift(current);
            current = prev[current];
            if (path.length > 100) break; // 安全阀: 防止异常数据导致无限循环
        }
        if (current === start) path.unshift(start);
        return path;
    }
}

// 支持CommonJS模块加载
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { BFS };
}
