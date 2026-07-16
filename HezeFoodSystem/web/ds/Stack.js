/**
 * 顺序栈（基于数组实现）
 * 
 * 用于操作历史记录的撤销(Undo)和重做(Redo)
 * 采用顺序存储结构（数组），栈顶由current指针管理
 * 
 * 栈的索引布局:
 *   [已执行的操作 | 待重做的操作]
 *   0 ... current ... data.length-1
 *   - current 指向最后一个已执行操作的位置
 *   - current 之后是已撤销但可重做的操作
 *   - 新操作会丢弃 current 之后的所有记录
 * 
 * 应用场景:
 *   - 用户收藏/取消收藏的撤销重做
 *   - 筛选条件变更的历史回退
 *   - 任何需要Undo/Redo的操作序列
 * 
 * 使用示例:
 *   var stack = new SeqStack(50);
 *   stack.push({ type: 'fav', data: {...}, time: Date.now() });
 *   stack.undo(); // 撤销
 *   stack.redo(); // 重做
 */
class SeqStack {
    /**
     * @param {number} maxSize - 最大容量(默认50)
     *   - 当栈元素数超过maxSize时, 自动移除最旧的记录(队列式淘汰)
     *   - 防止内存无限增长
     */
    constructor(maxSize) {
        this.data = [];         // 数组存储栈元素(顺序存储)
        this.current = -1;      // 当前栈顶指针(-1表示空栈, 无任何操作记录)
        this.maxSize = maxSize || 50; // 最大容量限制
    }

    /**
     * 入栈 — 执行新操作时调用
     * 
     * 重要行为:
     *   1. 丢弃current指针之后的所有记录 (新操作使之前的redo历史失效)
     *   2. 将新操作追加到数组末尾
     *   3. 若超出maxSize限制, 移除数组头部最旧记录
     * 
     * 示例:
     *   初始状态: [A, B, C] current=2
     *   undo一次: [A, B, C] current=1 (C进入redo区)
     *   push(D):   先丢弃C → [A, B] → 追加D → [A, B, D] current=2
     * 
     * @param {Object} item - 操作记录对象
     *   @property {string} item.type - 操作类型 ('fav'/'unfav'/'filter'等)
     *   @property {*} item.data - 操作相关数据
     *   @property {number} item.time - 操作时间戳
     */
    push(item) {
        // 丢弃current指针之后的所有待重做记录
        this.data = this.data.slice(0, this.current + 1);
        // 追加新操作
        this.data.push(item);
        // 容量控制: 超过最大容量时移除最旧记录(队首), 否则current后移
        if (this.data.length > this.maxSize) {
            this.data.shift(); // 移除最旧记录, current位置保持不变
        } else {
            this.current++;    // 指针后移到新记录位置
        }
    }

    /**
     * 撤销(Undo) — 回退到上一个操作状态
     * 
     * 将current指针前移一位, 返回被撤销的操作记录
     * 被撤销的记录进入"可重做"区域 (current+1到data.length-1之间)
     * 
     * @returns {Object|null} 被撤销的操作记录, 不可撤销时返回null
     */
    undo() {
        if (!this.canUndo()) return null;
        var item = this.data[this.current];
        this.current--;
        return item;
    }

    /**
     * 重做(Redo) — 前进到下一个操作状态
     * 
     * 将current指针后移一位, 返回被重做的操作记录
     * 
     * @returns {Object|null} 被重做的操作记录, 不可重做时返回null
     */
    redo() {
        if (!this.canRedo()) return null;
        this.current++;
        return this.data[this.current];
    }

    /**
     * 检查是否可以执行撤销操作
     * @returns {boolean} current >= 0 表示栈中还有已执行的操作
     */
    canUndo() { return this.current >= 0; }

    /**
     * 检查是否可以执行重做操作
     * @returns {boolean} current < data.length-1 表示还有被撤销但可重做的操作
     */
    canRedo() { return this.current < this.data.length - 1; }

    /**
     * 查看栈顶元素（不弹出，不影响指针）
     * @returns {Object|null} 当前操作记录, 空栈返回null
     */
    top() { return this.current >= 0 ? this.data[this.current] : null; }

    /**
     * 判断栈是否为空
     * @returns {boolean} current < 0 表示无任何操作记录
     */
    empty() { return this.current < 0; }

    /**
     * 获取栈中所有元素数量（包括已撤销但可重做的）
     * @returns {number}
     */
    size() { return this.data.length; }

    /**
     * 获取所有操作记录（用于前端渲染历史列表）
     * @returns {Array} 完整操作历史数组
     */
    getAll() { return this.data; }

    /**
     * 获取当前指针位置
     * @returns {number} current索引值, -1为空
     */
    getCurrent() { return this.current; }

    /**
     * 清空栈, 重置所有状态
     * 移除所有历史记录, 指针归位
     */
    clear() { this.data = []; this.current = -1; }
}

// 支持CommonJS模块加载
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { SeqStack };
}
