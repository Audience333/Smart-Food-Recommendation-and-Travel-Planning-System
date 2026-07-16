/**
 * Dijkstra 最短路径算法（小顶堆优化版本）
 * 
 * 基于贪心策略的最短路径算法，使用小顶堆（最小堆）优化取最小值操作
 * 找到从起点到终点的最短路径，支持按距离或时间计算权重
 * 
 * 算法核心思想:
 *   1. 维护dist[]数组记录从起点到各顶点的最短距离, 初始化为Infinity
 *   2. 维护小顶堆存储 {vertex, distance}，贪心选取当前距离最小的顶点扩展
 *   3. 对每个顶点, 松弛其所有邻接边: 若通过当前顶点可缩短邻居距离, 则更新
 *   4. 用prev[]数组记录前驱节点, 用于路径还原
 *   5. 当堆空或终点被访问时终止
 * 
 * 时间复杂度: O((V+E) log V)
 *   - V: 顶点数, E: 边数
 *   - 每个顶点最多入堆一次 O(V log V)
 *   - 每条边被松弛一次 O(E log V) (因为堆操作)
 * 
 * 空间复杂度: O(V)
 *   - dist数组: O(V)
 *   - prev数组: O(V)
 *   - visited数组: O(V)
 *   - 堆: 最坏O(V)
 * 
 * 与邻接表图的关系:
 *   - 依赖AdjacencyGraph提供顶点和边的存储
 *   - 通过getNeighbors()遍历邻接边
 *   - 通过顶点ID映射到数组索引
 * 
 * 使用示例:
 *   var graph = new AdjacencyGraph();
 *   // ... 构建路网图 ...
 *   var dijkstra = new Dijkstra(graph);
 *   var result = dijkstra.shortestPath(startPoiiD, endPoiID, 'distance');
 *   // result = { distance: 5200, path: [0, 2, 5], vertices: [{...}, {...}, {...}] }
 */
class Dijkstra {
    /**
     * @param {AdjacencyGraph} graph - 邻接表无向图
     *   - 需提前构建好顶点和边
     *   - 图在构造函数中存储引用, 不复制
     */
    constructor(graph) {
        this.graph = graph;
    }

    /**
     * 查找从起点到终点的最短路径
     * 
     * 算法流程:
     *   1. 通过ID查找起点/终点的数组索引
     *   2. 初始化 dist[start]=0, 其余为Infinity
     *   3. 初始化小顶堆 [{ vertex: startIdx, distance: 0 }]
     *   4. 循环: 从堆中取出距离最小的顶点
     *      - 若该顶点是终点, 提前终止
     *      - 若该顶点已访问过, 跳过 (懒删除处理)
     *      - 遍历该顶点的所有邻接边, 执行松弛操作
     *      - 将更新后的邻居入堆
     *   5. 若dist[end]仍是Infinity, 返回null (不可达)
     *   6. 否则通过prev[]数组还原完整路径
     * 
     * @param {number} startId - 起点POI ID
     * @param {number} endId - 终点POI ID
     * @param {string} weightType - 边权重类型 ('distance' 距离优先 | 'time' 时间优先)
     *   - 默认: 'distance'
     * @returns {Object|null} 最短路径结果, 不可达时返回null
     *   @property {number} result.distance - 总路径长度 (距离优先时单位: 米, 时间优先时单位: 分钟)
     *   @property {Array} result.path - 从起点到终点的顶点索引数组 [startIdx, ..., endIdx]
     *   @property {Array} result.vertices - 从起点到终点的完整顶点信息数组
     */
    shortestPath(startId, endId, weightType) {
        weightType = weightType || 'distance';
        var V = this.graph.vertexCount();
        var startIdx = this.graph.getIndexById(startId);
        var endIdx = this.graph.getIndexById(endId);
        if (startIdx < 0 || endIdx < 0) return null;

        // 初始化数据结构
        var dist = new Array(V).fill(Infinity);    // 最短距离数组
        var prev = new Array(V).fill(-1);           // 前驱节点数组 (用于路径还原)
        var visited = new Array(V).fill(false);     // 访问标记数组 (懒删除辅助)
        dist[startIdx] = 0;

        // 手写小顶堆: [{vertex, distance}, ...] 按distance排序
        var heap = [{ vertex: startIdx, distance: 0 }];

        // 主循环: 贪心扩展
        while (heap.length > 0) {
            // 从堆顶取出当前距离最小的顶点
            var u = heap[0].vertex;
            var du = heap[0].distance;
            this._heapRemoveMin(heap);

            // 提前终止优化: 已到达终点
            if (u === endIdx) break;

            // 懒删除: 跳过已被标记为访问的顶点 (堆中有旧记录)
            if (visited[u]) continue;
            visited[u] = true;

            // 遍历所有邻接边, 执行松弛操作
            var neighbors = this.graph.getNeighbors(u);
            for (var i = 0; i < neighbors.length; i++) {
                var edge = neighbors[i];
                var v = edge.to;
                if (visited[v]) continue;

                // 根据权重类型选择边的权值
                var w = (weightType === 'time') ? edge.time : edge.distance;
                var newDist = du + w;

                // 松弛: 若通过u到v的距离更短, 更新
                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    prev[v] = u;
                    // 入堆 (可能产生重复条目, 由懒删除处理)
                    this._heapPush(heap, { vertex: v, distance: newDist });
                }
            }
        }

        // 检查终点是否可达
        if (dist[endIdx] === Infinity) return null;

        // 从终点回溯起点还原路径
        var path = this._reconstructPath(prev, startIdx, endIdx);

        var self = this;
        return {
            distance: dist[endIdx],  // 总路径长度
            path: path,              // 顶点索引数组
            vertices: path.map(function(idx) {
                return self.graph.vertices[idx];
            })                       // 完整顶点信息数组
        };
    }

    /**
     * 小顶堆: 上浮 (插入操作)
     * 
     * 将堆末尾元素逐级与父节点比较, 若小于父节点则交换
     * 保证堆顶始终是最小元素
     * 
     * 堆的数组表示 (完全二叉树):
     *   - 节点i的父节点: Math.floor((i-1)/2)
     *   - 节点i的左子: 2*i+1, 右子: 2*i+2
     * 
     * @param {Array} heap - 堆数组 [{vertex, distance}, ...]
     * @param {Object} item - 入堆的元素
     * @private
     */
    _heapPush(heap, item) {
        heap.push(item);
        var i = heap.length - 1; // 新插入元素的位置(末尾)
        while (i > 0) {
            var p = Math.floor((i - 1) / 2); // 父节点索引
            if (heap[p].distance <= heap[i].distance) break; // 堆序已满足
            // 交换: 子节点值更小, 上浮
            var tmp = heap[p]; heap[p] = heap[i]; heap[i] = tmp;
            i = p;
        }
    }

    /**
     * 小顶堆: 下沉 (删除最小值操作)
     * 
     * 将堆尾元素搬到堆顶, 然后逐级与较小的子节点比较
     * 若大于子节点则交换下沉, 直至恢复堆序
     * 
     * @param {Array} heap - 堆数组
     * @private
     */
    _heapRemoveMin(heap) {
        if (heap.length <= 1) {
            heap.pop();
            return;
        }
        // 将最后一个元素搬到堆顶, 然后下沉
        heap[0] = heap.pop();
        var i = 0;
        while (true) {
            var left = 2 * i + 1;   // 左子节点索引
            var right = 2 * i + 2;  // 右子节点索引
            var smallest = i;
            // 找到i、left、right中值最小的
            if (left < heap.length && heap[left].distance < heap[smallest].distance) {
                smallest = left;
            }
            if (right < heap.length && heap[right].distance < heap[smallest].distance) {
                smallest = right;
            }
            if (smallest === i) break; // i已是最小, 堆序恢复
            // 交换下沉
            var tmp = heap[i]; heap[i] = heap[smallest]; heap[smallest] = tmp;
            i = smallest;
        }
    }

    /**
     * 路径还原: 从终点通过prev[]前驱数组回溯到起点
     * 
     * 工作机制:
     *   prev[v] = u 表示到达顶点v的最短路径中, v的前一个顶点是u
     *   从终点开始, 不断查找前驱, 直到到达起点, 将结果反转
     * 
     * 例如 prev = [-1, 0, 1, 2], start=0, end=3:
     *   回溯: 3 → prev[3]=2 → prev[2]=1 → prev[1]=0 → start
     *   结果: [0, 1, 2, 3]
     * 
     * @param {Array} prev - 前驱数组, prev[v] = 到达v时的前一个顶点索引
     * @param {number} start - 起点索引
     * @param {number} end - 终点索引
     * @returns {Array} 从起点到终点的完整顶点索引数组 [start, ..., end]
     * @private
     */
    _reconstructPath(prev, start, end) {
        var path = [];
        var current = end;
        while (current !== -1) {
            path.unshift(current); // 头部插入 (因为是反向回溯)
            if (current === start) break;
            current = prev[current];
        }
        return path;
    }
}

// 支持CommonJS模块加载
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { Dijkstra };
}
