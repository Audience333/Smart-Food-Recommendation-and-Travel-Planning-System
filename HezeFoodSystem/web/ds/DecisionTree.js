/**
 * 决策树节点
 * 每个节点包含一个判断条件和两个子节点(true分支/false分支)
 * 
 * 决策树用于多条件筛选美食数据，替代前端的activeCategories Set简单过滤
 * 通过链式AND结构串联多个筛选条件，支持：
 *   - 分类筛选（美食种类: 小吃/正餐/饮品等）
 *   - 营业状态筛选（是否正在营业中）
 *   - 评分筛选（最低评分阈值）
 *   - 价格筛选（最高价格阈值）
 * 
 * 使用方式:
 *   var tree = new DecisionTree();
 *   tree.build(activeCategories, openOnly, minScore, maxPrice);
 *   var filtered = tree.filter(foods);
 */
class TreeNode {
    /**
     * 构造决策树节点
     * @param {Function} condition - 判断函数, 签名为 function(food) => boolean
     *   - food: 美食对象, 包含 category/opentime/score/price 等字段
     * @param {Object|null} trueBranch - 条件满足时的子节点
     *   - TreeNode: 继续下一个条件判断
     *   - null: 接受该美食项（通过所有筛选）
     *   - undefined: 等同于null
     * @param {Object|null} falseBranch - 条件不满足时的子节点
     *   - TreeNode: 继续备用路径判断
     *   - null: 拒绝该美食项（未通过筛选）
     *   - undefined: 等同于null
     */
    constructor(condition, trueBranch, falseBranch) {
        this.condition = condition;      // 判断函数: function(food) => bool
        this.trueBranch = trueBranch;    // 条件满足时的子节点(TreeNode或null=接受)
        this.falseBranch = falseBranch;  // 条件不满足时的子节点(TreeNode或null=拒绝)
    }

    /**
     * 递归评估一条美食是否通过决策树所有筛选条件
     * 
     * 算法逻辑:
     *   1. 对当前节点执行condition(food)判断
     *   2. 如果条件为true:
     *      - 若trueBranch为null/undefined, 表示已通过所有筛选, 返回true
     *      - 否则递归进入trueBranch继续判断
     *   3. 如果条件为false:
     *      - 若falseBranch为null/undefined, 表示被拒绝, 返回false
     *      - 否则递归进入falseBranch继续判断
     * 
     * @param {Object} food - 美食对象
     *   @property {string} food.category - 美食分类
     *   @property {string} food.opentime - 营业时间
     *   @property {number} food.score - 评分
     *   @property {number} food.price - 价格
     * @returns {boolean} true=通过筛选, false=被过滤
     */
    evaluate(food) {
        var result = this.condition(food);
        if (result) {
            // 条件满足, 检查是否还有后续条件
            if (this.trueBranch === null) return true;
            if (this.trueBranch === undefined) return true;
            return this.trueBranch.evaluate(food);
        } else {
            // 条件不满足, 检查是否有备用路径
            if (this.falseBranch === null) return false;
            if (this.falseBranch === undefined) return false;
            return this.falseBranch.evaluate(food);
        }
    }
}

/**
 * 决策树筛选器
 * 
 * 构建多条件筛选决策树，对美食数据进行逐层过滤
 * 所有条件以AND关系串联: 分类匹配 AND 营业中 AND 评分达标 AND 价格达标
 * 
 * 树形结构示例（4个条件时）:
 *     [分类条件]
 *      /      \
 *   true    false→拒绝
 *    /
 *  [营业条件]
 *   /      \
 * true   false→拒绝
 *  /
 * ...
 * 
 * 性能分析:
 *   - 构建时间: O(k) k=条件数量
 *   - 筛选时间: O(n*k) n=美食数量, 单条评估最多k次条件判断
 *   - 空间复杂度: O(k)
 */
class DecisionTree {
    constructor() {
        this.root = null; // TreeNode 根节点, build之前为null
    }

    /**
     * 根据筛选参数动态构建决策树
     * 
     * 构建过程:
     *   1. 依次检查每个筛选条件是否生效(activeCategories非空/openOnly为true/minScore>0/maxPrice>0)
     *   2. 为每个生效条件创建TreeNode节点
     *   3. 将节点链式串联: 前一节点的trueBranch→下一节点, falseBranch→null(拒绝)
     *   4. 最后一个节点的trueBranch→null(接受)
     *   5. 若没有任何生效条件, 创建一个恒真节点, 所有美食都通过
     * 
     * @param {Set} activeCategories - 当前活跃的分类集合
     *   - Set为空或null时不过滤分类
     *   - 例如: new Set(['小吃', '饮品'])
     * @param {boolean} openOnly - 是否仅显示营业中的美食
     *   - true: 根据opentime字段判断当前是否在营业时间内
     *   - false: 不按营业状态过滤
     * @param {number} minScore - 最低评分筛选 (0=不过滤)
     *   - 大于0时, 只显示 score >= minScore 的美食
     * @param {number} maxPrice - 最高价格筛选 (0=不过滤)
     *   - 大于0时, 只显示 price <= maxPrice 的美食
     * @returns {DecisionTree} this (支持链式调用)
     */
    build(activeCategories, openOnly, minScore, maxPrice) {
        this.root = null;
        var self = this;

        // 将要创建的节点存入数组, 稍后串联
        var nodes = [];

        // 条件1: 分类匹配 - 检查food.category是否在活跃分类集合中
        if (activeCategories && activeCategories.size > 0) {
            nodes.push(new TreeNode(
                function(food) { return activeCategories.has(food.category); },
                null, null
            ));
        }

        // 条件2: 营业状态 - 检查当前时间是否在food.opentime范围内
        if (openOnly) {
            nodes.push(new TreeNode(
                function(food) {
                    // 无营业时间信息时默认通过
                    if (!food.opentime || food.opentime === '-') return true;
                    return self._isOpen(food.opentime);
                },
                null, null
            ));
        }

        // 条件3: 最低评分 - food.score >= minScore 才通过
        if (minScore > 0) {
            nodes.push(new TreeNode(
                function(food) { return food.score >= minScore; },
                null, null
            ));
        }

        // 条件4: 最高价格 - food.price <= maxPrice 才通过
        if (maxPrice > 0) {
            nodes.push(new TreeNode(
                function(food) { return food.price <= maxPrice; },
                null, null
            ));
        }

        // 将节点串联成链式决策树 (所有条件AND关系)
        if (nodes.length === 0) {
            // 无筛选条件: 创建恒真根节点, 所有美食无条件通过
            this.root = new TreeNode(function() { return true; }, null, null);
        } else {
            // 有筛选条件: 串联所有节点
            this.root = nodes[0];
            var current = this.root;
            for (var i = 1; i < nodes.length; i++) {
                // 前一节点条件满足时进入下一条件, 不满足则直接拒绝
                current.trueBranch = nodes[i];
                current.falseBranch = null;
                current = nodes[i];
            }
            // 最后一个节点: 条件满足→接受, 不满足→拒绝
            current.trueBranch = null;
            current.falseBranch = null;
        }

        return this;
    }

    /**
     * 对美食数组批量执行决策树筛选
     * 
     * 遍历所有美食, 逐条调用root.evaluate()判断, 保留通过的美食
     * 
     * @param {Array} foods - 美食数据数组
     *   - 每项为包含 category/opentime/score/price 等字段的对象
     * @returns {Array} 通过所有筛选条件的美食数组（保持原顺序）
     */
    filter(foods) {
        if (!this.root) return foods;
        var result = [];
        for (var i = 0; i < foods.length; i++) {
            if (this.root.evaluate(foods[i])) {
                result.push(foods[i]);
            }
        }
        return result;
    }

    /**
     * 判断当前时间是否在给定的营业时间段内
     * 
     * 支持格式:
     *   - "24小时" / "全天" → 始终营业
     *   - "06:00-14:00" → 单个时间段
     *   - "06:00-14:00,17:00-02:00" → 多个时间段（逗号分隔）
     *   - 跨午夜时段: 结束时间 <= 开始时间时, 结束时间+24小时处理
     *     （如 "17:00-02:00" 表示17:00到次日凌晨2:00）
     * 
     * @param {string} opentime - 营业时间字符串
     * @returns {boolean} 当前是否在营业时间内
     * @private
     */
    _isOpen(opentime) {
        // 24小时营业或全天营业 → 始终返回true
        if (opentime === '24小时' || opentime.indexOf('全天') >= 0) return true;

        // 获取当前时间的分钟数形式 (小时*60 + 分钟)
        var now = new Date();
        var min = now.getHours() * 60 + now.getMinutes();

        // 按逗号拆分多个时间段
        var parts = opentime.split(',');
        for (var i = 0; i < parts.length; i++) {
            var seg = parts[i].trim().split('-');
            if (seg.length === 2) {
                // 解析起止时间 时:分
                var s = seg[0].trim().split(':');
                var e = seg[1].trim().split(':');
                if (s.length >= 2 && e.length >= 2) {
                    // 转换为分钟数
                    var start = parseInt(s[0]) * 60 + parseInt(s[1]);
                    var end = parseInt(e[0]) * 60 + parseInt(e[1]);
                    // 跨午夜处理: 结束时间<=开始时间, 结束时间+24小时
                    if (end <= start) end += 24 * 60;
                    // 判断当前时间是否在 [start, end] 范围内
                    if (min >= start && min <= end) return true;
                }
            }
        }
        return false;
    }
}

// 支持CommonJS模块加载
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { DecisionTree, TreeNode };
}
