/**
 * 双向链表节点 (Doubly Linked List Node)
 * 
 * 每个节点包含:
 *   - data: 存储的收藏数据
 *   - prev: 指向前驱节点(上一个收藏)
 *   - next: 指向后继节点(下一个收藏)
 * 
 * 双向链表结构允许:
 *   - 正向遍历: 从旧到新浏览收藏
 *   - 反向遍历: 从新到旧浏览收藏
 *   - O(1)插入: 尾指针直接追加
 *   - O(1)删除: 已知节点时直接修改前后指针
 */
class DListNode {
    /**
     * @param {Object} data - 收藏数据对象
     *   @property {string|number} data.id - 收藏项唯一标识
     *   @property {string} data.type - 收藏类型 ('food'/'attraction'等)
     *   @property {string} data.name - 收藏项名称
     *   @property {number} data.addedAt - 添加时间戳
     */
    constructor(data) {
        this.data = data;    // 节点中存储的数据
        this.prev = null;    // 前驱节点指针 (初始为null)
        this.next = null;    // 后继节点指针 (初始为null)
    }
}

/**
 * 双向链表（带头结点/哨兵节点）
 * 
 * 用于收藏夹管理的完整CRUD操作:
 *   - 增(Insert):   尾插法O(1)追加新收藏
 *   - 删(Delete):   遍历查找O(n)定位后O(1)删除节点
 *   - 查(Search):   遍历查找O(n), 支持自定义匹配条件
 *   - 改(Update):   查找到节点后直接修改data
 *   - 遍历(Traverse): 正向/反向双向遍历, 回调处理
 *   - 持久化:       导出为普通数组存储到localStorage
 *   - 恢复:         从数组批量导入重建链表
 * 
 * 哨兵节点机制:
 *   head是头结点(哨兵), 不存储实际数据
 *   空链表: head.next === null, tail === head
 *   非空链表: head.next指向第一个实际节点, tail指向最后一个实际节点
 * 
 * 链表结构示意图:
 *   head(sentinel) ⟷ [收藏1] ⟷ [收藏2] ⟷ ... ⟷ [收藏N] ← tail
 * 
 * 使用示例:
 *   var favs = new DoublyLinkedList();
 *   favs.pushBack({ id: 1, type: 'food', name: '曹县烤全羊', addedAt: Date.now() });
 *   var node = favs.find(function(d) { return d.id === 1; });
 *   favs.removeNode(node);
 */
class DoublyLinkedList {
    constructor() {
        this.head = new DListNode(null); // 头结点(哨兵节点, 不存储实际数据)
        this.tail = this.head;           // 尾指针, 初始指向头结点(空链表标志)
        this.size_ = 0;                  // 链表实际节点数量(不含哨兵)
    }

    /**
     * 尾插法添加收藏 O(1)
     * 
     * 在链表末尾追加新节点, 更新尾指针
     * 这是收藏夹最常用的操作: 新收藏添加到末尾
     * 
     * 算法步骤:
     *   1. 创建新节点newNode
     *   2. 设置newNode.prev = tail (前驱为当前尾部)
     *   3. 设置tail.next = newNode (当前尾部的后继指向新节点)
     *   4. 更新tail = newNode (新节点成为新的尾部)
     *   5. size_++
     * 
     * @param {Object} data - 收藏数据
     */
    pushBack(data) {
        var node = new DListNode(data);
        node.prev = this.tail;
        this.tail.next = node;
        this.tail = node;
        this.size_++;
    }

    /**
     * 查找节点 O(n)
     * 
     * 从头结点的后继(第一个实际节点)开始, 顺序遍历
     * 每当predicate(data)返回true时停止, 返回该节点
     * 
     * 使用场景:
     *   - 按id查找: find(function(d) { return d.id === targetId; })
     *   - 按名称查找: find(function(d) { return d.name === '曹县烤全羊'; })
     * 
     * @param {Function} predicate - 判断函数, 签名为 function(node.data) => boolean
     * @returns {DListNode|null} 找到的节点, 未找到返回null
     */
    find(predicate) {
        var cur = this.head.next;
        while (cur) {
            if (predicate(cur.data)) return cur;
            cur = cur.next;
        }
        return null;
    }

    /**
     * 根据谓词查找所有匹配的节点 O(n)
     * 
     * @param {Function} predicate - 判断函数
     * @returns {Array} 匹配的节点数组
     */
    findAll(predicate) {
        var results = [];
        var cur = this.head.next;
        while (cur) {
            if (predicate(cur.data)) results.push(cur);
            cur = cur.next;
        }
        return results;
    }

    /**
     * 删除指定节点 O(1)
     * 
     * 已知节点引用时直接修改前后指针即可完成删除
     * 不需重新遍历链表
     * 
     * 边界处理:
     *   - 删除节点为null或哨兵节点: 直接返回
     *   - 删除节点是最后一个节点: 需要更新tail指针
     * 
     * 指针操作:
     *   删除node: ... ⟷ prevNode ⟷ node ⟷ nextNode ⟷ ...
     *   变为:     ... ⟷ prevNode ⟷ nextNode ⟷ ...
     *   即: prevNode.next = nextNode, nextNode.prev = prevNode
     * 
     * @param {DListNode} node - 要删除的节点引用
     */
    removeNode(node) {
        if (!node || node === this.head) return;
        // 断开前驱与后继的连接
        node.prev.next = node.next;
        if (node.next) {
            node.next.prev = node.prev;
        } else {
            // 被删除的是最后一个节点, 更新尾指针为前驱
            this.tail = node.prev;
        }
        this.size_--;
    }

    /**
     * 删除第一个匹配条件的节点 O(n)
     * 
     * 先调用find定位节点, 再调用removeNode删除
     * 
     * @param {Function} predicate - 判断函数
     * @returns {boolean} true=成功删除, false=未找到匹配
     */
    remove(predicate) {
        var node = this.find(predicate);
        if (node) { this.removeNode(node); return true; }
        return false;
    }

    /**
     * 检查链表中是否存在满足条件的节点
     * @param {Function} predicate - 判断函数
     * @returns {boolean}
     */
    contains(predicate) { return this.find(predicate) !== null; }

    /**
     * 获取链表实际节点数量
     * @returns {number}
     */
    size() { return this.size_; }

    /**
     * 判断链表是否为空
     * @returns {boolean} size_ === 0 表示无实际节点
     */
    empty() { return this.size_ === 0; }

    /**
     * 正向遍历链表 (从旧到新)
     * 
     * 从头结点的后继开始, 沿next指针遍历到尾结点
     * 对每个节点执行callback(data, index)
     * 
     * 用途:
     *   - 按添加时间顺序展示收藏列表
     *   - 导出数据为数组(配合toArray)
     *   - 统计/汇总所有收藏数据
     * 
     * @param {Function} callback - 回调函数 function(data, index)
     */
    forwardTraverse(callback) {
        var cur = this.head.next;
        var i = 0;
        while (cur) {
            callback(cur.data, i);
            cur = cur.next;
            i++;
        }
    }

    /**
     * 反向遍历链表 (从新到旧)
     * 
     * 从尾结点开始, 沿prev指针遍历到头结点
     * 对每个实际节点执行callback(data, index)
     * 
     * 用途:
     *   - 按时间倒序展示最近收藏
     *   - "最近添加"视图
     * 
     * @param {Function} callback - 回调函数 function(data, index)
     */
    backwardTraverse(callback) {
        var cur = this.tail;
        var i = this.size_ - 1;
        while (cur && cur !== this.head) {
            callback(cur.data, i);
            cur = cur.prev;
            i--;
        }
    }

    /**
     * 导出为普通数组 (用于localStorage/JSON持久化)
     * 
     * 顺序遍历, 将每个节点的data推入数组
     * 数组顺序 = 链表正向遍历顺序 (从旧到新)
     * 
     * 持久化流程:
     *   var json = JSON.stringify(favs.toArray());
     *   localStorage.setItem('favorites', json);
     * 
     * @returns {Array} 收藏数据数组
     */
    toArray() {
        var arr = [];
        this.forwardTraverse(function(data) { arr.push(data); });
        return arr;
    }

    /**
     * 从普通数组批量导入数据 (用于localStorage恢复)
     * 
     * 先清空现有链表, 再依次尾插法添加数组元素
     * 恢复后的顺序 = 数组原有顺序
     * 
     * 恢复流程:
     *   var json = localStorage.getItem('favorites');
     *   if (json) { favs.fromArray(JSON.parse(json)); }
     * 
     * @param {Array} arr - 收藏数据数组 (可以为null/undefined, 视为空数组)
     */
    fromArray(arr) {
        // 重置链表为空
        this.head.next = null;
        this.tail = this.head;
        this.size_ = 0;
        // 逐项追加
        var self = this;
        if (arr) { arr.forEach(function(item) { self.pushBack(item); }); }
    }

    /**
     * 清空链表
     * 将所有节点从内存中解除引用, 重置为空链表状态
     */
    clear() { this.head.next = null; this.tail = this.head; this.size_ = 0; }
}

// 支持CommonJS模块加载
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { DoublyLinkedList, DListNode };
}
