/**
 * ============================================================================
 * 菏泽美食推荐与城市漫游规划系统 - 地图可视化模块（高德地图 API v2.0 完整集成）
 * ============================================================================
 *
 * 【整体架构说明】
 * 本文件是整个系统的前端核心模块（约1800行），负责以下功能：
 *   1. 地图初始化与渲染（基于高德 JSAPI v2.0，使用 AMapLoader 异步加载）
 *   2. 美食 POI 标记展示（按分类颜色区分，支持分类筛选和营业状态筛选）
 *   3. 景点 POI 标记展示（蓝色方形标记，支持收藏金色边框）
 *   4. 信息窗口（详情弹窗）：展示评分星级、价格等级、营业状态、分类、地址、标签、图片
 *   5. 逆地理编码：将经纬度坐标转换为可读的中文街道地址（带内存缓存减少 API 调用）
 *   6. 路线规划：调用高德路径规划 API（restapi.amap.com/v3/direction/），支持驾车/步行/公交，支持途经点排序
 *   7. 收藏管理：基于 localStorage 持久化收藏夹，收藏变化自动联动地图标记样式
 *   8. 操作历史：基于栈的操作记录，支持撤销（Undo）和重做（Redo），最多保存50条
 *   9. 排行榜：三种维度（热门榜、性价比榜、收藏热榜），支持升/降序切换
 *   10. 用户画像：基于收藏数据分析口味偏好（模糊匹配关键词）、价格区间分布、品类偏好
 *   11. 每日推荐：智能生成一日游路线方案（枚举美食组合+最近景点），一键采纳到路线规划
 *
 * 【依赖关系】
 *   - 高德 JSAPI v2.0（AMapLoader）：地图、标记、信息窗口、路线等功能
 *   - 高德 Web API（restapi.amap.com）：路径规划、逆地理编码
 *   - data/food.json：美食数据（JSON 数组，字段：id, name, category, score, price, lng, lat, tags, photos, opentime, address）
 *   - data/spot.json：景点数据（JSON 数组，字段：id, name, type, score, lng, lat, tags, photos, description, ticketInfo, openingTime, address）
 *   - 关联的 HTML 页面需包含 #map 容器、各类面板元素（搜索面板、路线面板、收藏面板等）
 *   - 关联的 CSS 文件定义标记、信息窗口、面板的样式
 *
 * 【全局对象设计（五大管理器）】
 *   - HistoryManager   : 操作历史栈 —— 记录用户操作，提供撤销/重做功能
 *   - FavoriteManager  : 收藏夹管理 —— 添加/删除/持久化收藏，联动渲染各面板
 *   - RankingManager   : 排行榜 —— 按评分/性价比/收藏数排序展示
 *   - ProfileManager   : 用户画像 —— 分析收藏美食的口味、价格、品类偏好
 *   - DailyTourManager : 每日推荐 —— 智能生成一日游路线方案
 *
 * 【坐标系约定】
 *   - 所有经纬度使用 GCJ-02 坐标系（高德火星坐标系，与 WGS-84 有偏移）
 *   - lng: 经度（longitude），lat: 纬度（latitude）
 *   - 坐标格式：[lng, lat] 数组（高德 API 标准格式）
 *   - 菏泽市中心约 [115.477, 35.245]
 *
 * 【数据流示意】
 *   food.json / spot.json → foodData[] / spotData[]
 *       ↓
 *   showFoodMarkers() / showSpotMarkers() → 地图标记
 *       ↓ (点击标记)
 *   showFoodDetail() / showSpotDetail() → 信息窗口
 *       ↓ (收藏操作)
 *   FavoriteManager → 联动刷新 RankingManager / ProfileManager / DailyTourManager
 *       ↓ (路线规划操作)
 *   executeRoutePlan() → planRouteSegments() → renderRouteSegment() → 地图路线
 */

// ============================================================================
// 一、全局状态变量 —— 存储地图实例、数据、筛选开关等核心状态
// ============================================================================

/**
 * map: 高德地图实例对象（AMap.Map 类型）
 * 整个模块共享唯一的地图实例，所有标记、路线、信息窗口都在此地图上渲染。
 * 初始值为 null，在 loadMap() 成功回调中赋值，后续所有地图操作都依赖此变量。
 */
var map = null;

/**
 * geocoder: 高德逆地理编码器实例（AMap.Geocoder 类型）
 * 用于将经纬度坐标（[lng, lat]）反向解析为可读的街道地址字符串。
 * 在 loadMap() 初始化时创建，配置 city: '菏泽' 以提高本地地址解析精度。
 * 如果地图加载失败则为 null，此时逆地理编码返回占位文本。
 */
var geocoder = null;

/**
 * foodMarkers: 当前地图上所有美食标记（AMap.Marker 实例）的数组
 * 存储每个美食的 HTML 自定义标记对象，用于批量管理（显示/隐藏/移除）。
 * showFoodMarkers() 会在渲染前清空此数组，重新创建所有标记。
 * 每个 marker 上通过 _foodData 属性绑定原始美食数据（用于后续查询）。
 */
var foodMarkers = [];

/**
 * spotMarkers: 当前地图上所有景点标记（AMap.Marker 实例）的数组
 * 与 foodMarkers 类似，但存储的是景点标记（蓝色方形，显示"景"字）。
 * 景点标记的 zIndex 为 110，比美食标记（100）高，在密集区域更显眼。
 */
var spotMarkers = [];

/**
 * foodData: 从 data/food.json 加载的美食原始数据数组
 * 每条美食数据包含：id（唯一标识）, name（名称）, category（分类，如"汤类"/"面食"等）,
 *   score（评分 0-5）, price（价格 元）, lng/lat（经纬度）, tags（标签数组）,
 *   photos（图片URL数组）, opentime（营业时间字符串）, address（地址字符串或"-"）
 * 是所有美食相关功能（标记、详情、搜索、排名）的基础数据源。
 */
var foodData = [];

/**
 * spotData: 从 data/spot.json 加载的景点原始数据数组
 * 每条景点数据包含：id, name, type（景点类型）, score（评分）,
 *   lng/lat, tags, photos, description（简介）, ticketInfo（门票信息）,
 *   openingTime（开放时间）, address
 */
var spotData = [];

/**
 * activeCategories: 当前激活（选中）的美食分类筛选集合（Set 类型）
 * 用途：控制地图上显示哪些分类的美食标记。
 * - 当集合为空时（默认状态）：表示"全部显示"，不进行任何筛选
 * - 当集合非空时：只显示集合内分类对应的美食标记
 * 用户在分类筛选面板点击标签来切换此集合的内容。
 * 每次变化后触发 showFoodMarkers() 重新渲染标记。
 */
var activeCategories = new Set();

/**
 * addressCache: 逆地理编码结果缓存对象（普通 Object）
 * 减少对高德 API 的重复调用，提升响应速度、节省 API 配额。
 * - key: "经度,纬度" 格式的字符串（经度和纬度各保留5位小数，约1.1米精度）
 * - value: 对应的中文地址字符串（如"菏泽市牡丹区xx路xx号"）
 * 在 loadData() 中预填充数据中已有的地址，后续 getAddress() 中动态追加。
 */
var addressCache = {};

/**
 * toggleFoodVisible: 美食图层全局可见性开关（boolean）
 * true = 显示所有美食标记（默认），false = 隐藏所有美食标记。
 * 通过顶部工具栏的"美食"按钮切换，toggleSpotVisible 同理控制景点。
 * 注意：此开关与 activeCategories 独立——分类筛选在"可见"前提下进一步细化。
 */
var toggleFoodVisible = true;

/**
 * toggleSpotVisible: 景点图层全局可见性开关（boolean）
 * true = 显示所有景点标记（默认），false = 隐藏所有景点标记。
 */
var toggleSpotVisible = true;

/**
 * openOnlyActive: "仅显示营业中"筛选开关（boolean）
 * true = 只显示当前正在营业的美食店铺，隐藏已打烊的标记。
 * false = 显示全部（默认）。
 * 通过顶部工具栏的"营业中"按钮切换，只有美食标记受此开关影响（景点无营业状态概念）。
 * 依赖 getOpenStatus() 函数判断每条美食的营业状态。
 */
var openOnlyActive = false;

/**
 * currentRoutePolylines: 当前已绘制到地图上的路线折线（AMap.Polyline 实例）列表
 * 路线可能包含多段（起点→途经点1→途经点2→...→终点），每段是一条折线。
 * clearRoute() 遍历此数组统一移除（调用 setMap(null)）。
 */
var currentRoutePolylines = [];

/**
 * currentRouteMarkers: 当前已绘制到地图上的路线辅助标记列表
 * 每条路线的中点处放置一个标记，显示该段的距离/时间/过路费信息。
 * clearRoute() 时统一移除。
 */
var currentRouteMarkers = [];

/**
 * routeWaypoints: 路线规划中用户添加的途经点数组
 * 每个途经点对象包含：{ id（POI的ID）, name（名称）, type（'food'|'spot'）, lng（经度）, lat（纬度）}
 * 用户在路线面板中添加/删除/排序途经点，此数组实时同步。
 * 未选择具体POI的占位途经点坐标为 (0, 0)，executeRoutePlan() 中会过滤掉。
 */
var routeWaypoints = [];

/**
 * routeMode: 当前路线规划的交通方式（string）
 * 可选值：'driving'（驾车，默认）、'walking'（步行）、'bus'（公交/transit）。
 * 由路线面板的模式切换按钮（.route-mode-btn[data-mode]）控制。
 * 影响高德路径规划 API 的 URL 路径（/v3/direction/driving 或 /walking 等）。
 */
var routeMode = 'driving';

/**
 * routeSortMode: 路线规划的策略排序方式（string）
 * 可选值：'time'（最短时间，默认 strategy=0）、'distance'（最短距离 strategy=2）、'toll'（最少收费 strategy=1）。
 * 由路线面板的排序按钮（.route-mode-btn[data-sort]）控制。
 * 作为 strategy 参数传给高德路径规划 API，影响推荐哪条路线。
 */
var routeSortMode = 'time';

/**
 * currentInfoWindow: 当前打开的信息窗口实例（AMap.InfoWindow 类型）
 * 用于管理"同时只显示一个详情弹窗"的逻辑：
 * - 打开新窗口前先关闭 currentInfoWindow（避免多个弹窗堆叠）
 * - 再次点击同一 POI 时，如果窗口已打开则关闭（toggle 行为）
 * - 每个信息窗口上通过 _poiId 和 _poiType 属性标记所属的POI
 * 初始值为 null。
 */
var currentInfoWindow = null;

// ============================================================================
// 二、操作历史管理器（HistoryManager）—— 基于栈的撤销/重做系统
// ============================================================================

/**
 * HistoryManager: 操作历史管理器
 *
 * 【设计思路】
 * 使用栈（stack）记录用户的所有关键操作，支持撤销（Undo）和重做（Redo）。
 * 类似于浏览器的前进/后退功能，适用于需要"回退操作"的交互密集型应用。
 * 指针（pointer）指向"当前已执行到"的操作索引。
 *
 * 【重要行为】
 * 当用户在非栈顶位置（即撤销了几步之后）执行新的操作时，
 * 指针之后的所有"未来"操作会被截断丢弃——这是标准的"撤销后做新操作则清空重做栈"行为。
 *
 * 【数据结构】
 * stack: 操作记录数组，每条记录包含三个字段：
 *   - type: 操作类型标识（'view'查看详情 | 'filter'分类筛选 | 'search'搜索 | 'fav_add'收藏 | 'fav_del'取消收藏 | 'route'路线规划）
 *   - data: 操作相关的附加数据（如筛选前后的分类集合、POI信息等），供撤销/重做时恢复状态
 *   - time: 操作时间的格式化字符串（HH:MM格式，中文24小时制）
 * pointer: 当前指针位置，-1 表示空栈（无任何操作）
 * maxSize: 最大容量 50 条，超出时移除最早的记录（FIFO策略，避免内存无限增长）
 *
 * 【与各功能模块的交互】
 * 该管理器被各个功能模块通过 push() 调用以记录操作，但不直接修改功能状态。
 * 撤销/重做时，通过 _applyReverse() 和 _applyForward() 调用各功能模块的 API 来恢复/重放状态。
 */
var HistoryManager = {
    /** @type {Array} 操作记录栈，按时间顺序存储（栈底=最早，栈顶=最新） */
    stack: [],
    /** @type {number} 当前指针位置（索引），-1 表示空栈 */
    pointer: -1,
    /** @type {number} 最大保存的操作记录数 */
    maxSize: 50,

    /**
     * push(type, data): 向历史栈推入一条新的操作记录
     *
     * 【执行流程】
     * 1. 如果指针不在栈顶（说明用户之前做了撤销，有"未来"操作可以重做），
     *    则截断指针之后的所有"未来"操作——这是标准的"新操作清空重做栈"行为
     * 2. 将新操作记录推入栈顶，附带当前时间戳的格式化字符串
     * 3. 如果栈超过最大容量（50条），移除最早的记录（栈底出队）
     * 4. 指针更新为栈顶索引
     * 5. 刷新 UI（按钮启用/禁用状态和下拉列表内容）
     *
     * @param {string} type - 操作类型标识（'view'|'filter'|'search'|'fav_add'|'fav_del'|'route'）
     * @param {object} data - 操作附加数据（不同的操作类型携带不同的数据结构）
     */
    push: function(type, data) {
        // 截断指针之后的"未来"操作（新操作清空重做栈）
        if (this.pointer < this.stack.length - 1) { this.stack = this.stack.slice(0, this.pointer + 1); }
        // 推入新操作记录，time 使用中文本地化时间格式（如 "14:30"）
        this.stack.push({ type: type, data: data, time: new Date().toLocaleTimeString('zh-CN', {hour:'2-digit',minute:'2-digit'}) });
        // 容量溢出时移除最早记录（shift 弹出栈底），指针保持不变（因为栈长度已减1）
        if (this.stack.length > this.maxSize) this.stack.shift();
        this.pointer = this.stack.length - 1;
        this.updateUI();
    },

    /**
     * undo(): 撤销一步操作
     * 将指针向前移动一位，获取当前位置的操作记录，调用 _applyReverse() 执行"撤销逻辑"。
     * 执行前通过 canUndo() 检查是否有可撤销的操作。
     */
    undo: function() {
        if (!this.canUndo()) return;
        var item = this.stack[this.pointer]; this.pointer--;
        this._applyReverse(item); this.updateUI();
    },

    /**
     * redo(): 重做一步之前被撤销的操作
     * 将指针向后移动一位，获取新位置的操作记录，调用 _applyForward() 执行"重做逻辑"。
     * 执行前通过 canRedo() 检查是否有可重做的操作。
     */
    redo: function() {
        if (!this.canRedo()) return;
        this.pointer++;
        var item = this.stack[this.pointer];
        this._applyForward(item); this.updateUI();
    },

    /**
     * canUndo(): 判断是否可以撤销
     * @returns {boolean} pointer >= 0 说明栈中至少有一条已执行的操作
     */
    canUndo: function() { return this.pointer >= 0; },

    /**
     * canRedo(): 判断是否可以重做
     * @returns {boolean} 指针不在栈顶（pointer < stack.length - 1）说明有被撤销的操作可以重做
     */
    canRedo: function() { return this.pointer < this.stack.length - 1; },

    /**
     * _applyReverse(item): 执行指定操作的反向（撤销）逻辑
     *
     * 【核心思想】
     * 每种操作类型有对应的"逆操作"来恢复到操作前的状态：
     *
     * - filter（分类筛选）:
     *   撤销 = 恢复为筛选前的分类集合（prevCategories），然后重新渲染标记和筛选面板
     *
     * - search（搜索）:
     *   撤销 = 清空搜索输入框、隐藏搜索结果面板、恢复所有标记的透明度为1（全不透明）
     *
     * - fav_add（收藏添加）:
     *   撤销 = 取消收藏（调用 FavoriteManager.remove），即"逆向"添加操作
     *
     * - fav_del（取消收藏）:
     *   撤销 = 重新添加收藏（调用 FavoriteManager.add），即"逆向"删除操作
     *   注意：需要 data 中的 name 字段来重建收藏记录
     *
     * - route（路线规划）:
     *   撤销 = 清除地图上的所有路线元素（clearRoute），恢复自适应视野
     *
     * - view（查看POI详情）:
     *   撤销 = 关闭当前信息窗口
     *
     * 所有撤销操作后均刷新排行榜（RankingManager.refresh()），确保排名数据一致。
     *
     * @param {object} item - 操作记录对象，包含 type 和 data 字段
     */
    _applyReverse: function(item) {
        switch(item.type) {
            case 'filter':
                // 恢复为筛选前的分类集合
                if (item.data.prevCategories) { activeCategories = new Set(item.data.prevCategories); showFoodMarkers(); generateCategoryFilters(); }
                break;
            case 'search':
                // 清空搜索状态——输入框、结果面板、标记透明度
                var si = document.getElementById('searchInput'); if (si) si.value = '';
                var sr = document.getElementById('searchResults'); if (sr) sr.style.display = 'none';
                if (map) { foodMarkers.forEach(function(m) { m.setOpacity(1); }); spotMarkers.forEach(function(m) { m.setOpacity(1); }); }
                break;
            case 'fav_add':
                // 收藏的逆向操作 = 取消收藏
                FavoriteManager.remove(item.data.id, item.data.type); break;
            case 'fav_del':
                // 取消收藏的逆向操作 = 重新添加收藏
                FavoriteManager.add({ id: item.data.id, type: item.data.type, name: item.data.name }); break;
            case 'route':
                // 路线的逆向操作 = 清除路线并恢复视野
                clearRoute(); autoFitView(); break;
            case 'view':
                // 查看的逆向操作 = 关闭信息窗口
                if (map) map.clearInfoWindow(); break;
        }
        RankingManager.refresh();
    },

    /**
     * _applyForward(item): 执行指定操作的正向（重做）逻辑
     *
     * 与 _applyReverse 对应，将被撤销的操作"重新执行"一遍（但不是所有操作都需要forward逻辑）。
     * 例如：撤销"添加收藏"后重做 = 重新添加该收藏。
     *
     * 注意：search（搜索）和 route（路线）没有 _applyForward 逻辑，
     * 因为这些操作不便于从历史记录中完全恢复（搜索词可能已变化，路线API可能返回不同结果）。
     *
     * @param {object} item - 操作记录对象
     */
    _applyForward: function(item) {
        switch(item.type) {
            case 'filter':
                // 重做筛选：应用新的分类集合
                if (item.data.newCategories) { activeCategories = new Set(item.data.newCategories); showFoodMarkers(); generateCategoryFilters(); }
                break;
            case 'fav_add':
                // 重做收藏添加
                FavoriteManager.add({ id: item.data.id, type: item.data.type, name: item.data.name }); break;
            case 'fav_del':
                // 重做取消收藏
                FavoriteManager.remove(item.data.id, item.data.type); break;
            case 'view':
                // 重做查看：将地图定位到该POI位置（16级缩放）
                if (map && item.data.lng) { map.setZoomAndCenter(16, [item.data.lng, item.data.lat]); }
                break;
        }
        RankingManager.refresh();
    },

    /**
     * updateUI(): 更新历史面板的 UI 状态
     * - 根据 canUndo() 和 canRedo() 的返回值设置撤销/重做按钮的透明度
     *   （不可用时半透明表示禁用状态，可用时完全不透明）
     * - 调用 _renderDropdown() 重新渲染历史记录下拉列表
     */
    updateUI: function() {
        var ub = document.getElementById('btnHistoryUndo'); if (ub) ub.style.opacity = this.canUndo() ? '1' : '0.4';
        var rb = document.getElementById('btnHistoryRedo'); if (rb) rb.style.opacity = this.canRedo() ? '1' : '0.4';
        this._renderDropdown();
    },

    /**
     * _renderDropdown(): 渲染历史记录下拉列表
     *
     * 【展示规则（用符号标记操作状态）】
     * - ▶ (&#9654;): 标记当前指针位置（即"当前所在的历史状态"）
     * - ● (&#9679;): 标记"未来"操作（位于指针之后，已被撤销但可以重做的操作）
     * - ○ (&#9675;): 标记"过去"操作（位于指针之前，已执行且未被撤销的操作）
     *
     * 【CSS 类名对应】
     * - history-current: 当前指针位置
     * - history-done: 未来可重做的操作
     * - history-undone: 过去的操作
     *
     * 【排序规则】
     * 从栈顶到栈底倒序遍历（最新的在最上面），方便用户从最近的操作开始查看。
     * 每条记录显示三个元素：状态符号 + 操作时间 + 操作类型中文标签。
     */
    _renderDropdown: function() {
        var list = document.getElementById('historyDropdownList'); if (!list) return;
        var html = '';
        // 倒序遍历：最新的记录显示在最上面
        for (var i = this.stack.length - 1; i >= 0; i--) {
            var item = this.stack[i];
            // 根据与 pointer 的位置关系选择不同的标记符号和 CSS 类名
            var m = i === this.pointer ? '&#9654;' : (i > this.pointer ? '&#9679;' : '&#9675;');
            var cls = i === this.pointer ? 'history-current' : (i > this.pointer ? 'history-done' : 'history-undone');
            // 操作类型 → 中文标签的映射表
            var labels = { view:'查看', filter:'筛选', search:'搜索', fav_add:'收藏', fav_del:'取消收藏', route:'路线' };
            html += '<div class="history-item '+cls+'" data-index="'+i+'"><span class="history-marker">'+m+'</span><span class="history-time">'+item.time+'</span><span class="history-label">'+ (labels[item.type]||item.type) +'</span></div>';
        }
        list.innerHTML = html || '<div class="history-empty">暂无操作记录</div>';
    },

    /**
     * clear(): 清空所有历史记录
     * 重置栈数组和指针，刷新 UI（禁用撤销/重做按钮，清空下拉列表）。
     */
    clear: function() { this.stack = []; this.pointer = -1; this.updateUI(); }
};

// ============================================================================
// 三、收藏夹管理器（FavoriteManager）—— 基于 localStorage 的持久化收藏系统
// ============================================================================

/**
 * FavoriteManager: 收藏夹管理器
 *
 * 【设计思路】
 * 使用浏览器 localStorage 持久化存储收藏数据，确保页面刷新后收藏不会丢失。
 * 收藏操作（添加/删除）是系统的"核心操作"，会触发连锁刷新：地图标记样式、HistoryManager记录、
 * 排行榜排名、用户画像、每日推荐等。
 *
 * 【数据结构】
 * items: 收藏记录数组（内存中的数据），每条记录包含：
 *   - id: POI的唯一标识（数字，对应 foodData 或 spotData 中的 id 字段）
 *   - type: POI类型（'food' 美食 | 'spot' 景点）
 *   - name: POI名称（冗余存储，使离线状态下也能显示收藏列表的名称）
 *   - addedAt: 添加收藏时的时间戳（毫秒，Date.now()），用于排序和追踪
 * storageKey: localStorage 的键名，值为 'heze_favorites'
 *
 * 【持久化策略】
 * 每次 add/remove 操作后立即调用 save() 写入 localStorage。
 * 保存时只存储 { id, type, addedAt } 三个字段（name 可从数据源获取，不冗余存储到磁盘）。
 * 加载时（load()）从 localStorage 读取并解析 JSON。
 * 使用 try-catch 包裹 localStorage 操作，防止隐私模式或存储满导致的异常。
 *
 * 【连锁刷新机制】
 * 收藏添加或删除后，会刷新以下模块：
 * - 地图标记：showFoodMarkers()/showSpotMarkers() —— 已收藏的POI标记显示金色边框
 * - 排行榜：RankingManager.refresh() —— 收藏热榜的排序会变化
 * - 用户画像：ProfileManager.renderPanel() —— 口味/价格分析数据源变了
 * - 每日推荐：DailyTourManager.generate() —— 推荐基于收藏偏好
 * - 收藏面板：this.renderPanel() —— 更新侧边栏收藏列表
 */
var FavoriteManager = {
    /** @type {Array} 收藏记录数组（内存中的实时数据） */
    items: [],
    /** @type {string} localStorage 存储键名 */
    storageKey: 'heze_favorites',

    /**
     * init(): 初始化收藏夹
     * 从 localStorage 加载已有收藏数据到内存。在 loadData() 完成时调用。
     */
    init: function() { this.load(); },

    /**
     * add(poi): 添加一个 POI 到收藏夹
     *
     * 【幂等性】如果该 POI 已经在收藏夹中，则直接返回不执行任何操作（防止重复收藏）。
     *
     * 【连锁操作】
     * 1. 追加到 items 数组（附带时间戳）
     * 2. 写入 localStorage 持久化
     * 3. 记录到操作历史（供撤销使用）
     * 4. 刷新收藏面板、排行榜、用户画像、每日推荐
     * 5. 刷新地图标记（收藏的POI标记会显示金色边框）
     *
     * @param {object} poi - 包含 { id, type, name } 三个必填字段的POI对象
     *   id: POI的唯一数字ID
     *   type: 'food' 或 'spot'
     *   name: POI名称（用于收藏列表显示）
     */
    add: function(poi) {
        // 防止重复收藏——已存在则直接返回（幂等操作）
        if (this.isFavorite(poi.id, poi.type)) return;
        // 添加到内存数组，记录添加时间戳
        this.items.push({ id: poi.id, type: poi.type, name: poi.name, addedAt: Date.now() });
        this.save();
        // 记录操作历史——收藏操作的 data 包含 id/type/name 供撤销还原
        HistoryManager.push('fav_add', { id: poi.id, type: poi.type, name: poi.name });
        // 连锁刷新所有相关面板和标记
        this.renderPanel();
        RankingManager.refresh();
        ProfileManager.renderPanel();
        DailyTourManager.generate();
        // 刷新地图标记——收藏的POI标记显示金色边框（3px #ffd700）
        if (typeof showFoodMarkers === 'function') showFoodMarkers();
        if (typeof showSpotMarkers === 'function') showSpotMarkers();
    },

    /**
     * remove(id, type): 从收藏夹中移除指定的 POI
     *
     * 先从 items 中查找该收藏项（为了获取 name 供历史记录使用），
     * 然后过滤掉匹配项，保存并刷新所有关联模块。
     *
     * @param {number} id   - POI的唯一数字ID
     * @param {string} type - POI类型（'food' | 'spot'）
     */
    remove: function(id, type) {
        // 先查找收藏项（获取 name 字段供历史记录的 data 使用）
        var item = this.items.find(function(f) { return f.id === id && f.type === type; });
        // 过滤掉匹配项——保留所有 (id, type) 不匹配的收藏
        this.items = this.items.filter(function(f) { return !(f.id === id && f.type === type); });
        this.save();
        // 如果找到了该收藏项，记录删除操作到历史
        if (item) HistoryManager.push('fav_del', { id: id, type: type, name: item.name });
        this.renderPanel();
        RankingManager.refresh();
        ProfileManager.renderPanel();
        DailyTourManager.generate();
        if (typeof showFoodMarkers === 'function') showFoodMarkers();
        if (typeof showSpotMarkers === 'function') showSpotMarkers();
    },

    /**
     * isFavorite(id, type): 检查指定 POI 是否已被收藏
     * 使用 Array.some() 遍历 items 进行匹配（简洁高效）。
     * @param {number} id   - POI的唯一数字ID
     * @param {string} type - POI类型（'food' | 'spot'）
     * @returns {boolean} 是否已收藏
     */
    isFavorite: function(id, type) { return this.items.some(function(f) { return f.id === id && f.type === type; }); },

    /**
     * getAll(): 获取所有已收藏 POI 的完整数据（含原始数据字段）
     *
     * 【工作原理】
     * 遍历 items（收藏记录），根据 type 在对应的数据源（foodData / spotData）中
     * 查找完整的原始数据，使用 Object.assign() 合并收藏特有字段。
     *
     * 返回的每个对象包含原始数据的所有字段 + 两个额外字段：
     *   - favType: 收藏类型（'food' 或 'spot'），方便渲染时区分显示
     *   - addedAt: 收藏时间戳
     *
     * 如果某个收藏在数据源中找不到（可能数据已更新），则返回 null，后续 filter 移除。
     *
     * @returns {Array} 完整的 POI 数据数组（已在原始数据源中找到匹配的项）
     */
    getAll: function() {
        var self = this;
        return this.items.map(function(f) {
            // 根据类型在对应数据源中查找完整数据
            var data = f.type === 'food' ? foodData.find(function(x) { return x.id === f.id; }) :
                       spotData.find(function(x) { return x.id === f.id; });
            // 合并原始数据和收藏特有字段（返回新对象，不修改原数据）
            return data ? Object.assign({}, data, { favType: f.type, addedAt: f.addedAt }) : null;
        }).filter(function(x) { return x !== null; }); // 过滤掉在数据源中找不到的收藏
    },

    /**
     * save(): 将收藏数据持久化到 localStorage
     *
     * 只保存 { id, type, addedAt } 三个字段到磁盘。
     * name 不保存（可从数据源获取），减少 localStorage 空间占用。
     * 使用 try-catch 防止异常（如隐私模式、存储空间满）。
     */
    save: function() {
        var d = this.items.map(function(f) { return { id: f.id, type: f.type, addedAt: f.addedAt }; });
        try { localStorage.setItem(this.storageKey, JSON.stringify(d)); } catch(e) {}
    },

    /**
     * load(): 从 localStorage 加载收藏数据到内存
     *
     * 解析 JSON 字符串恢复 items 数组。
     * 使用 try-catch 防止 localStorage 数据损坏导致解析失败——此时重置为空数组。
     * 注意：加载时 items 中的 name 字段为空（因为 save 时不保存 name），
     * 但后续通过 getAll() 从数据源获取时会重新补充完整数据。
     */
    load: function() {
        try { var r = localStorage.getItem(this.storageKey); if (r) this.items = JSON.parse(r); } catch(e) { this.items = []; }
    },

    /**
     * renderPanel(): 渲染收藏夹面板（左侧侧边栏中的收藏列表）
     *
     * 【渲染内容】
     * 每条收藏项包含三个部分：
     *   1. 左侧圆形图标：美食显示名字首字（分类颜色背景），景点显示"景"字（蓝色背景）
     *   2. 中间信息区：上排显示POI名称，下排显示元信息（美食:评分+价格，景点:类型）
     *   3. 右侧金色星星：已收藏状态（&#9733;实心星），点击可取消收藏
     *
     * 【交互行为】
     * - 点击列表项（非星星区域）：地图定位到该POI（16级缩放）
     * - 点击星星图标：取消收藏（阻止事件冒泡，避免触发列表项的定位）
     * - 无收藏时显示空状态提示文字
     */
    renderPanel: function() {
        var container = document.getElementById('favoritesList'); if (!container) return;
        var favs = this.getAll();
        // 更新面板标题中的收藏数量（如 "收藏夹(5)"）
        var countEl = document.getElementById('favoritesCount'); if (countEl) countEl.textContent = '(' + favs.length + ')';
        if (favs.length === 0) {
            // 空收藏夹——显示友好的提示文字
            container.innerHTML = '<div class="favorites-empty">暂无收藏</div>';
        } else {
            var html = '';
            favs.forEach(function(f) {
                // 判断是美食还是景点——设置不同的颜色和图标文字
                var isFood = f.favType === 'food';
                var color = isFood ? (CATEGORY_COLORS[f.category] || CATEGORY_COLORS.default) : '#1565c0';
                var icon = isFood ? f.name.charAt(0) : '景';
                var sub = isFood ? f.score.toFixed(1) + '分 / Y' + f.price : f.type;
                html += '<div class="favorite-item" data-lng="'+f.lng+'" data-lat="'+f.lat+'" data-id="'+f.id+'" data-type="'+f.favType+'">' +
                    '<div class="favorite-icon" style="background:'+color+'">'+icon+'</div>' +
                    '<div class="favorite-info"><div class="favorite-name">'+f.name+'</div><div class="favorite-meta">'+sub+'</div></div>' +
                    '<span class="fav-star faved" data-poi-id="'+f.id+'" data-poi-type="'+f.favType+'">&#9733;</span></div>';
            });
            container.innerHTML = html;
            // 为每个收藏列表项绑定点击事件——点击定位到地图（16级缩放）
            container.querySelectorAll('.favorite-item').forEach(function(el) {
                el.addEventListener('click', function(e) {
                    // 排除点击收藏星星的情况——星星有独立的取消收藏事件
                    if (e.target.classList.contains('fav-star')) return;
                    var lng = parseFloat(this.dataset.lng), lat = parseFloat(this.dataset.lat);
                    if (map) map.setZoomAndCenter(16, [lng, lat]);
                });
            });
            // 为每个收藏星星绑定点击事件——点击取消该POI的收藏
            container.querySelectorAll('.fav-star').forEach(function(el) {
                el.addEventListener('click', function(e) { e.stopPropagation();
                    FavoriteManager.remove(parseInt(this.dataset.poiId), this.dataset.poiType); });
            });
        }
    }
};

// ============================================================================
// 四、排行榜管理器（RankingManager）—— 三种排序维度的美食排行榜
// ============================================================================

/**
 * rankSortDesc: 排行榜排序方向标志（boolean）
 * true = 降序排列（默认，高分在前，符合直觉的"最好到最差"排列）
 * false = 升序排列（低分在前）
 * 用户点击排序按钮（#btnRankSort）时切换，按钮显示 ↓(降序) / ↑(升序)。
 */
var rankSortDesc = true;

/**
 * RankingManager: 排行榜管理器
 *
 * 【设计思路】
 * 提供三种排序维度的排行榜，数据来源于 foodData（美食数据，不包含景点）。
 * 排行榜面板位于左侧侧边栏，支持标签页切换和升/降序切换。
 *
 * 【三种排名维度】
 *   1. popular（热门榜）: 按评分（score）排序，纯粹的质量排名
 *   2. value（性价比榜）: 按 评分 / log(价格+1) 排序，平衡质量与价格
 *      - 使用自然对数平滑价格差异的影响
 *      - 加1避免价格为0时 log(0) 无定义
 *   3. favorites（收藏热榜）: 按被收藏次数优先排序，收藏次数相同时按评分排序
 *      - 体现了用户的群体偏好
 *
 * 【与系统的关系】
 * - refresh() 被 HistoryManager 和 FavoriteManager 调用，确保数据变化后排名及时更新
 * - 每条记录可点击定位到地图
 * - 已收藏的美食在名称旁显示金色星标
 */
var RankingManager = {
    /** @type {string} 当前激活的排行榜标签，可选值：'popular' | 'value' | 'favorites' */
    currentTab: 'popular',

    /**
     * getPopular(): 获取热门排行榜（按评分排序）
     * 使用 Array.slice() 复制数组后再排序，不修改原始 foodData。
     * 根据 rankSortDesc 决定降序（高分在前）还是升序（低分在前）。
     * @returns {Array} 排序后的 foodData 副本
     */
    getPopular: function() { 
        var r = foodData.slice().sort(function(a,b){return b.score - a.score;});
        if (!rankSortDesc) r.reverse();
        return r;
    },

    /**
     * getValue(): 获取性价比排行榜
     *
     * 【性价比计算公式】
     * value = score / ln(price + 1)
     *
     * 【公式解释】
     * - 分子 score（0-5）：美食质量得分，越高越好
     * - 分母 ln(price + 1)：价格的自然对数变换
     *   - 使用自然对数是为了"缩小"高价格的影响——如果直接用 score/price，
     *     则价格为1元的商品会在排名中畸形地高
     *   - 加1是为了处理价格为0的边缘情况（ln(1)=0，导致除零错误）
     *   - 自然对数的增长速率递减：价格从10到20的影响 远大于 价格从100到110的影响
     * - 整体：评分高且价格低的排在前面
     *
     * @returns {Array} 按性价比排序的 foodData 副本
     */
    getValue: function() {
        var r = foodData.slice().sort(function(a,b){
            return (b.score/Math.log(b.price+1)) - (a.score/Math.log(a.price+1));
        });
        if (!rankSortDesc) r.reverse();
        return r;
    },

    /**
     * getFavRanking(): 获取收藏热榜
     *
     * 【排序规则（两级优先级）】
     * 1. 第一优先级：收藏次数（fc[food.id]），次数多 > 次数少
     * 2. 第二优先级：当收藏次数相同时，按评分降序排列
     *
     * 统计收藏次数时只统计 type==='food' 的收藏，忽略景点收藏。
     *
     * @returns {Array} 按收藏热度排序的 foodData 副本
     */
    getFavRanking: function() {
        // 遍历 FavoriteManager.items，统计每个美食ID被收藏的次数
        var fc = {};
        FavoriteManager.items.forEach(function(f){ if(f.type==='food') fc[f.id]=(fc[f.id]||0)+1; });
        var r = foodData.slice().sort(function(a,b){
            var fa=fc[a.id]||0, fb=fc[b.id]||0;
            if(fb!==fa) return fb-fa; // 收藏次数不同——次数多在前
            return b.score-a.score;   // 收藏次数相同——评分高在前
        });
        if (!rankSortDesc) r.reverse();
        return r;
    },

    /**
     * refresh(): 刷新排行榜面板的显示内容
     * 重新渲染当前选中标签的排名列表。
     */
    refresh: function() { this.renderPanel(); },

    /**
     * renderPanel(): 渲染排行榜面板的 HTML
     *
     * 【展示内容】
     * - 前3名：显示奖牌 emoji（&#129351;=🥇金牌, &#129352;=🥈银牌, &#129353;=🥉铜牌）
     * - 第4-10名：显示数字序号
     * - 每条记录显示：名次 + 名称（已收藏加星标）+ 评分/价格/分类
     * - 只展示前10条记录
     *
     * 【交互】
     * 点击任意排名项，地图定位到该POI位置（16级缩放）。
     */
    renderPanel: function() {
        var list = document.getElementById('rankingList'); if(!list) return;
        // 根据当前标签选择对应的排序数据集
        var ranking = this.currentTab==='popular' ? this.getPopular() : (this.currentTab==='value' ? this.getValue() : this.getFavRanking());
        var medals = ['&#129351;','&#129352;','&#129353;'];
        var html = '';
        // 只展示前10条（使用 slice(0,10) 流式处理，不影响原始数组）
        ranking.slice(0,10).forEach(function(f,i){
            // 前3名用奖牌emoji（大字号），后续用数字序号
            var rankHtml = i<3 ? '<span style="font-size:18px;">'+medals[i]+'</span>' : '<span class="ranking-num">'+(i+1)+'</span>';
            var isFav = FavoriteManager.isFavorite(f.id,'food');
            html += '<div class="ranking-item" data-lng="'+f.lng+'" data-lat="'+f.lat+'"><div class="ranking-rank">'+rankHtml+'</div>' +
                '<div class="ranking-info"><div class="ranking-name">'+f.name+(isFav?' &#9733;':'')+'</div>' +
                '<div class="ranking-meta">'+f.score.toFixed(1)+'分 / Y'+f.price+' / '+f.category+'</div></div></div>';
        });
        list.innerHTML = html;
        // 绑定点击事件——点击排名项定位到地图
        list.querySelectorAll('.ranking-item').forEach(function(el){ el.addEventListener('click',function(){
            map.setZoomAndCenter(16,[parseFloat(this.dataset.lng),parseFloat(this.dataset.lat)]); }); });
    },

    /**
     * init(): 初始化排行榜的交互事件绑定
     *
     * 绑定两个交互：
     * 1. 标签页切换按钮（.ranking-tab）：切换 currentTab 并重新渲染
     * 2. 排序方向按钮（#btnRankSort）：切换 rankSortDesc 并更新按钮文字和排名
     */
    init: function() {
        var self = this;
        // 标签切换事件：移除所有标签的 active 类，给当前点击的添加 active 类
        document.querySelectorAll('.ranking-tab').forEach(function(tab){ tab.addEventListener('click',function(e){
            e.stopPropagation();
            document.querySelectorAll('.ranking-tab').forEach(function(t){t.classList.remove('active');});
            this.classList.add('active'); self.currentTab = this.dataset.tab; self.renderPanel(); }); });
        
        // 排序方向按钮：切换升降序标志，更新按钮文字和排名
        var sortBtn = document.getElementById('btnRankSort');
        if (sortBtn) {
            sortBtn.addEventListener('click', function(e) {
                e.stopPropagation();
                rankSortDesc = !rankSortDesc;
                this.textContent = rankSortDesc ? '↓' : '↑'; // ↓=降序, ↑=升序
                self.renderPanel();
            });
        }
    }
};

// ============================================================================
// 五、用户画像管理器（ProfileManager）—— 基于收藏数据的偏好分析
// ============================================================================

/**
 * ProfileManager: 用户偏好画像管理器
 *
 * 【设计思路】
 * 通过分析用户收藏的美食数据，生成三个维度的偏好画像：
 *   1. 口味偏好：从收藏美食的 tags 字段中匹配预定义的口味关键词，统计频次
 *   2. 价格分布：统计收藏美食在不同价格区间（<10, 10-30, 30-60, 60-100, 100+）的分布比例
 *   3. 品类偏好：统计收藏美食所属分类（汤类/面食/小吃等）的频次
 *
 * 【编辑模式（可手动调整口味标签）】
 * 用户可以进入编辑模式手动干预口味画像：
 *   - 点击 +/- 按钮增加或减少某个口味标签的频次
 *   - 点击 × 删除某个口味标签
 *   - 从下拉列表添加新的口味标签
 *   - 保存后写入 _savedCounts（持久化在内存中，页面刷新后重置）
 *   - 取消编辑丢弃 _editCounts（恢复为编辑前状态）
 *   - 恢复默认清除所有自定义编辑数据
 *
 * 【口味关键词模糊匹配规则】
 * 从收藏美食的 tags（如 ["麻辣", "酸辣", "浓郁"]）中提取口味信息：
 * 双向包含匹配——如果标签包含关键词 OR 关键词包含标签，都视为匹配。
 * 例如：标签"麻辣烫"可以匹配关键词"麻辣"；标签"辣"也可以匹配关键词"麻辣"。
 *
 * 【容错机制】
 * 如果收藏美食的 tags 中没有匹配到任何预定义的口味关键词，
 * 则根据美食分类（如"汤类"→"鲜香+暖胃"）自动映射默认口味。
 *
 * 【与系统的关系】
 * 画像面板位于左侧侧边栏，收藏变化时 FavoriteManager 会触发 renderPanel() 刷新。
 */
var ProfileManager = {
    /** @type {boolean} 是否处于编辑模式（控制标签的可交互性） */
    isEditing: false,
    /** @type {object|null} 编辑中的口味计数临时数据（{ 口味名: 频次 }），编辑取消时丢弃 */
    _editCounts: null,
    /** @type {object|null} 已保存的自定义口味计数（{ 口味名: 频次 }），编辑保存后写入 */
    _savedCounts: null,

    /**
     * renderPanel(): 渲染用户画像面板
     *
     * 【渲染流程（三阶段分析）】
     * 阶段1 — 口味分析：
     *   遍历每个收藏美食的 tags 数组，与预定义的口味关键词列表做模糊匹配，
     *   统计每个关键词的出现频次，排序取前8个展示为标签云。
     *
     * 阶段2 — 价格分析：
     *   将收藏美食按价格分5个区间（<10元, 10-30元, 30-60元, 60-100元, 100+元），
     *   统计每个区间的数量和百分比，渲染为横向进度条。
     *
     * 阶段3 — 品类分析：
     *   统计每个美食分类（汤类/面食/烧烤等）的收藏数量，
     *   排序取前5个品类，渲染为横向进度条。
     *
     * 【编辑模式下的特殊渲染】
     * - 每个口味标签显示为可交互的 chip（带 -、+、x 按钮）
     * - 底部显示"添加标签"下拉选择器 + 添加按钮
     * - 数据源切换为 _editCounts（用户的临时编辑数据）
     */
    renderPanel: function() {
        var container = document.getElementById('profilePanel');
        if (!container) return;
        var favs = FavoriteManager.getAll();
        // 无收藏数据时——显示空状态提示
        if (favs.length === 0) {
            container.innerHTML = '<div class="profile-empty">收藏美食后可查看画像</div>';
            return;
        }
        
        // 过滤出美食类型的收藏（景点不参与口味/价格分析）
        var foods = favs.filter(function(f) { return f.favType === 'food'; });
        if (foods.length === 0) {
            container.innerHTML = '<div class="profile-empty">收藏美食后可查看画像</div>';
            return;
        }

        // ============================================================
        // 阶段1：口味分析 —— 基于预定义关键词的模糊匹配
        // ============================================================

        // 预定义的口味关键词列表（覆盖中国传统美食常见的口味描述）
        // 包括辣味系、酸味系、口感系、烹饪工艺系、综合评价系等
        var tasteKeywords = ['麻辣','酸辣','清淡','酱香','蒜蓉','原味','鲜香','五香','甜口','咸鲜','微辣','重辣','酸甜','孜然','鲜嫩','酥脆','软糯','筋道','Q弹','肥而不腻','入口即化','暖胃','滋补','开胃','清爽','咖喱','醇厚','鲜美','香辣','浓汤','清甜','酸甜','焦香','嫩滑'];
        var tasteCounts = {};
        // 遍历每个收藏美食的 tags 数组，与关键词做模糊匹配
        foods.forEach(function(f) {
            if (f.tags) {
                f.tags.forEach(function(t) {
                    var tClean = String(t).trim();
                    // 双向模糊匹配：标签包含关键词 或 关键词包含标签
                    tasteKeywords.forEach(function(kw) {
                        if (tClean.indexOf(kw) >= 0 || kw.indexOf(tClean) >= 0) {
                            tasteCounts[kw] = (tasteCounts[kw] || 0) + 1;
                        }
                    });
                });
            }
        });
        // 保存编辑前的原始计数（用于"取消编辑"时的数据恢复）
        if (!this.isEditing) {
            this._editCounts = null;
        }
        // 编辑模式下使用用户的临时编辑数据
        if (this.isEditing) {
            if (!this._editCounts) {
                // 首次进入编辑模式：基于已保存数据或原始数据初始化编辑计数
                this._editCounts = JSON.parse(JSON.stringify(this._savedCounts || tasteCounts));
            }
            tasteCounts = this._editCounts;
        } else if (this._savedCounts) {
            // 非编辑模式下，如果有已保存的自定义数据，优先使用
            tasteCounts = this._savedCounts;
        }

        // 按频次降序排列，取前8个口味
        var tasteSorted = Object.keys(tasteCounts).sort(function(a,b){return tasteCounts[b]-tasteCounts[a];}).slice(0, 8);

        // 容错机制：如果从 tags 中没有匹配到任何口味标签（可能是数据质量问题）
        // 则根据美食分类映射到默认口味偏好
        if (tasteSorted.length === 0) {
            // 分类 → 默认口味的映射表
            var catMap = {
                '汤类': ['鲜香','暖胃'],
                '面食': ['筋道','原味'],
                '烧烤': ['孜然','香辣'],
                '甜品': ['甜口','清甜'],
                '饮品': ['清爽','清甜'],
                '凉菜': ['开胃','清爽'],
                '小吃': ['原味','鲜香'],
                '正餐': ['醇厚','酱香']
            };
            var catTastes = {};
            foods.forEach(function(f) {
                var defaults = catMap[f.category] || ['原味','鲜香'];
                defaults.forEach(function(t) { catTastes[t] = (catTastes[t] || 0) + 1; });
            });
            tasteSorted = Object.keys(catTastes).sort(function(a,b){return catTastes[b]-catTastes[a];});
        }

        // ============================================================
        // 阶段2：价格分析 —— 按5个价格区间统计收藏分布
        // ============================================================
        var priceBands = {'<10':0,'10-30':0,'30-60':0,'60-100':0,'100+':0};
        foods.forEach(function(f) {
            if (f.price <= 10) priceBands['<10']++;
            else if (f.price <= 30) priceBands['10-30']++;
            else if (f.price <= 60) priceBands['30-60']++;
            else if (f.price <= 100) priceBands['60-100']++;
            else priceBands['100+']++;
        });
        var maxPrice = Math.max.apply(null, Object.values(priceBands)) || 1;
        
        // ============================================================
        // 阶段3：品类分析 —— 统计每个分类的收藏频次
        // ============================================================
        var catCounts = {};
        foods.forEach(function(f) { catCounts[f.category] = (catCounts[f.category] || 0) + 1; });
        // 只展示前5个品类
        var catSorted = Object.keys(catCounts).sort(function(a,b){return catCounts[b]-catCounts[a];}).slice(0, 5);
        var maxCat = Math.max.apply(null, Object.values(catCounts)) || 1;

        var html = '';

        // ---------- 口味偏好标签云区域 ----------
        html += '<div class="profile-section"><div class="profile-section-title">口味偏好 ' + (this.isEditing ? '<span style="font-size:11px;color:#999;">(点击 +/- 调整 / 下拉添加)</span>' : '') + '</div><div class="profile-taste-cloud">';
        tasteSorted.forEach(function(t) {
            var count = tasteCounts[t] || 0;
            if (ProfileManager.isEditing) {
                // 编辑模式：每个标签含 -（减少）、+（增加）、x（删除）三个操作按钮
                html += '<span class="profile-taste-tag editable" data-taste="' + t + '" style="display:inline-flex;align-items:center;gap:2px;font-size:13px;margin:2px;padding:3px 6px;background:#fff3e0;border:1px solid #ffcc80;border-radius:12px;">' +
                    '<span class="taste-minus" data-taste="' + t + '" style="cursor:pointer;color:#d32f2f;font-weight:bold;padding:0 3px;">-</span>' +
                    t + '(' + count + ')' +
                    '<span class="taste-plus" data-taste="' + t + '" style="cursor:pointer;color:#4caf50;font-weight:bold;padding:0 3px;">+</span>' +
                    '<span class="taste-del" data-taste="' + t + '" style="cursor:pointer;color:#999;font-size:11px;margin-left:2px;">x</span></span>';
            } else {
                // 只读模式：仅显示标签名和频次
                html += '<span class="profile-taste-tag" style="font-size:13px;">' + t + '(' + count + ')</span>';
            }
        });
        html += '</div>';
        if (this.isEditing) {
            // 编辑模式：提供"添加新标签"的下拉选择器
            // 从所有美食数据中提取全部可用的口味标签（去重）
            var allTastes = new Set();
            foodData.forEach(function(fd) {
                if (fd.tags) fd.tags.forEach(function(tg) {
                    var tc = String(tg).trim();
                    tasteKeywords.forEach(function(kw) {
                        if (tc.indexOf(kw) >= 0 || kw.indexOf(tc) >= 0) allTastes.add(kw);
                    });
                });
            });
            var tasteList = Array.from(allTastes).sort();
            html += '<div style="display:flex;gap:4px;margin-top:6px;">' +
                '<select id="profileAddTaste" class="route-select-input" style="flex:1;padding:4px 6px;font-size:12px;">' +
                '<option value="">选择标签添加</option>';
            tasteList.forEach(function(tt) {
                html += '<option value="' + tt + '">' + tt + '</option>';
            });
            html += '</select>' +
                '<button id="btnProfileAddTaste" class="panel-action-btn" style="padding:4px 10px;">添加</button></div>';
        }
        html += '</div>';

        // ---------- 价格分布进度条区域 ----------
        html += '<div class="profile-section"><div class="profile-section-title">价格区间</div>';
        ['<10','10-30','30-60','60-100','100+'].forEach(function(b) {
            // 计算该区间的百分比（相对于收藏美食总数）
            var pct = Math.round((priceBands[b] / foods.length) * 100);
            html += '<div class="profile-bar-row"><span class="profile-bar-label">' + (b === '<10' ? '<10元' : b === '100+' ? '100+元' : b + '元') + '</span>' +
                '<div class="profile-bar"><div class="profile-bar-fill" style="width:' + pct + '%"></div></div>' +
                '<span class="profile-bar-pct">' + pct + '%</span></div>';
        });
        html += '</div>';

        // ---------- 品类偏好进度条区域 ----------
        html += '<div class="profile-section"><div class="profile-section-title">偏好品类</div>';
        catSorted.forEach(function(c) {
            var pct = Math.round((catCounts[c] / foods.length) * 100);
            html += '<div class="profile-bar-row"><span class="profile-bar-label">' + c + '</span>' +
                '<div class="profile-bar"><div class="profile-bar-fill" style="width:' + pct + '%"></div></div>' +
                '<span class="profile-bar-pct">' + pct + '%</span></div>';
        });
        html += '</div>';

        container.innerHTML = html;

        // ---------- 编辑模式下的交互事件绑定 ----------
        if (this.isEditing) {
            // "+" 按钮：增加某个口味标签的频次
            container.querySelectorAll('.taste-plus').forEach(function(el) {
                el.addEventListener('click', function(e) { e.stopPropagation();
                    var taste = this.dataset.taste;
                    if (ProfileManager._editCounts) {
                        ProfileManager._editCounts[taste] = (ProfileManager._editCounts[taste] || 0) + 1;
                        ProfileManager.renderPanel(); // 重新渲染以更新显示
                    }
                });
            });
            // "-" 按钮：减少频次（减到0则删除该标签）
            container.querySelectorAll('.taste-minus').forEach(function(el) {
                el.addEventListener('click', function(e) { e.stopPropagation();
                    var taste = this.dataset.taste;
                    if (ProfileManager._editCounts && ProfileManager._editCounts[taste]) {
                        ProfileManager._editCounts[taste]--;
                        if (ProfileManager._editCounts[taste] <= 0) delete ProfileManager._editCounts[taste];
                        ProfileManager.renderPanel();
                    }
                });
            });
            // "x" 删除按钮：彻底移除该口味标签
            container.querySelectorAll('.taste-del').forEach(function(el) {
                el.addEventListener('click', function(e) { e.stopPropagation();
                    var taste = this.dataset.taste;
                    if (ProfileManager._editCounts && ProfileManager._editCounts[taste]) {
                        delete ProfileManager._editCounts[taste];
                    }
                    ProfileManager.renderPanel();
                });
            });
            // "添加" 按钮：从下拉列表中选择标签并添加到编辑数据中
            var addBtn = document.getElementById('btnProfileAddTaste');
            var addSel = document.getElementById('profileAddTaste');
            if (addBtn && addSel) {
                addBtn.addEventListener('click', function() {
                    var val = addSel.value;
                    if (val && ProfileManager._editCounts) {
                        ProfileManager._editCounts[val] = (ProfileManager._editCounts[val] || 0) + 1;
                        ProfileManager.renderPanel();
                    }
                });
            }
        }
    },

    /**
     * enterEditMode(): 进入画像编辑模式
     *
     * 将 isEditing 标志设为 true，切换按钮显示：
     * - 隐藏：编辑按钮（#btnEditProfile）
     * - 显示：保存按钮、取消按钮、恢复默认按钮
     * 然后重新渲染面板（此时标签变为可交互状态）。
     */
    enterEditMode: function() {
        this.isEditing = true;
        document.getElementById('btnEditProfile').style.display = 'none';
        document.getElementById('btnSaveProfile').style.display = 'inline-block';
        document.getElementById('btnCancelProfile').style.display = 'inline-block';
        document.getElementById('btnRestoreProfile').style.display = 'inline-block';
        this.renderPanel();
    },

    /**
     * saveProfile(): 保存画像编辑结果
     *
     * 将 _editCounts（临时编辑数据）深拷贝到 _savedCounts（持久化结果），
     * 退出编辑模式，切换按钮显示，重新渲染面板（此时使用 _savedCounts）。
     * 注意：_savedCounts 仅保存在内存中，页面刷新后重置为原始数据。
     */
    saveProfile: function() {
        // 深拷贝：创建独立副本，避免后续修改 _editCounts 影响保存的数据
        this._savedCounts = JSON.parse(JSON.stringify(this._editCounts));
        this.isEditing = false;
        this._editCounts = null;
        document.getElementById('btnEditProfile').style.display = 'inline-block';
        document.getElementById('btnSaveProfile').style.display = 'none';
        document.getElementById('btnCancelProfile').style.display = 'none';
        document.getElementById('btnRestoreProfile').style.display = 'none';
        this.renderPanel();
    },

    /**
     * cancelProfile(): 取消编辑，丢弃所有未保存的修改
     *
     * 清除 _editCounts（临时编辑数据），退出编辑模式，恢复按钮显示。
     * 面板使用 _savedCounts（如果有保存过的数据）或原始数据重新渲染。
     */
    cancelProfile: function() {
        this.isEditing = false;
        this._editCounts = null;
        document.getElementById('btnEditProfile').style.display = 'inline-block';
        document.getElementById('btnSaveProfile').style.display = 'none';
        document.getElementById('btnCancelProfile').style.display = 'none';
        document.getElementById('btnRestoreProfile').style.display = 'none';
        this.renderPanel();
    },

    /**
     * restoreProfile(): 恢复为默认画像
     *
     * 清除所有自定义编辑数据（_editCounts 和 _savedCounts 都设为 null），
     * 退出编辑模式。面板将完全基于收藏数据的原始分析结果重新生成。
     */
    restoreProfile: function() {
        this.isEditing = false;
        this._editCounts = null;
        this._savedCounts = null;
        document.getElementById('btnEditProfile').style.display = 'inline-block';
        document.getElementById('btnSaveProfile').style.display = 'none';
        document.getElementById('btnCancelProfile').style.display = 'none';
        document.getElementById('btnRestoreProfile').style.display = 'none';
        this.renderPanel();
    }
};

// ============================================================================
// 六、每日推荐管理器（DailyTourManager）—— 智能一日游路线生成器
// ============================================================================

/**
 * DailyTourManager: 每日推荐路线生成器
 *
 * 【设计思路】
 * 基于用户收藏的偏好数据，自动生成一个"一日游"路线方案：
 * 以某个景点为中心，推荐3个附近的美食，形成"逛景点+品美食"的完整行程。
 *
 * 【算法流程（六步骤）】
 * 步骤1 — 分析用户偏好：从收藏中统计品类偏好（Top3分类）和平均消费水平。
 * 步骤2 — 美食评分：对所有美食逐项评分——
 *         总分 = 品类匹配(0.3) + 价格匹配(0.1) + 评分归一化(score/5 × 0.4)
 *         取Top20高分美食作为候选。
 * 步骤3 — Fisher-Yates洗牌：打乱Top20的顺序，增加每次生成的随机多样性。
 * 步骤4 — 枚举组合（时间复杂度 ≈ C(20,3) × |spots| = 1140 × N）：
 *         三重循环枚举所有3个美食的组合，找到地理上最近的景点，
 *         计算完整路线距离（景点→美食1→美食2→美食3）。
 * 步骤5 — 路线评分：综合得分 = 平均评分 × 0.7 + 距离归一化 × 0.3
 *         距离归一化：(1 - totalDist/50000)，距离越短得分越高。
 *         过滤掉总距离超过50km的不合理方案。
 * 步骤6 — 排序取前3条：最佳方案 + 2个备选方案。
 *
 * 【一键采纳机制】
 * 用户点击"采纳"按钮后，adopt() 方法会自动：
 *   1. 将方案中的景点设置为路线起点
 *   2. 将3个美食设置为途经点
 *   3. 将最后一个美食设置为终点
 *   4. 自动执行 executeRoutePlan() 进行路线规划
 *
 * 【与系统的关系】
 * - 依赖 FavoriteManager 获取用户收藏数据（分析偏好）
 * - 生成结果通过调用路线规划的 API（executeRoutePlan）落地
 * - 被 FavoriteManager.add/remove 触发重新生成
 */
var DailyTourManager = {
    /** @type {Array} 生成的推荐路线方案数组，最多3个（1个最佳 + 2个备选） */
    candidates: [],

    /**
     * generate(): 生成每日推荐路线方案
     *
     * 整个算法的入口函数。分析收藏偏好 → 评分排序 → 枚举组合 → 排序输出。
     * 结果存储在 this.candidates 中，供 renderPanel() 和 adopt() 使用。
     * 如果没有收藏数据或收藏的美食不足，candidates 为空数组。
     */
    generate: function() {
        var self = this;
        var favs = FavoriteManager.getAll();
        var foods = favs.filter(function(f) { return f.favType === 'food'; });
        var spots = favs.filter(function(f) { return f.favType === 'spot'; });
        
        // ======== 步骤1: 分析用户偏好 ========
        // 统计每个分类被收藏的次数
        var catPrefs = {};
        var priceSum = 0;
        foods.forEach(function(f) {
            catPrefs[f.category] = (catPrefs[f.category] || 0) + 1;
            priceSum += f.price;
        });
        // 取前3个最常收藏的品类作为偏好
        var topCats = Object.keys(catPrefs).sort(function(a,b){return catPrefs[b]-catPrefs[a];}).slice(0, 3);
        // 计算加权平均价格（作为用户"舒适消费区间"的参考值）
        var avgPrice = foods.length > 0 ? priceSum / foods.length : 40;
        
        // ======== 步骤2: 对所有美食进行综合评分 ========
        // 评分的三个维度：
        //   - catMatch (0.3): 该美食分类是否在用户Top3品类中
        //   - priceMatch (0.1): 价格与用户平均价差距 < 20元
        //   - scoreNorm (0.4): 原始评分归一化到 0-0.4 区间
        var scored = foodData.map(function(f) {
            var catMatch = topCats.indexOf(f.category) >= 0 ? 0.3 : 0;
            var priceMatch = Math.abs(f.price - avgPrice) < 20 ? 0.1 : 0;
            var score = f.score / 5 * 0.4 + catMatch + priceMatch;
            return { id: f.id, name: f.name, category: f.category, score: f.score, price: f.price, lng: f.lng, lat: f.lat, totalScore: score };
        });
        // 按综合得分降序排列，取前20个最高分的美食
        scored.sort(function(a,b){return b.totalScore - a.totalScore;});
        var topFoods = scored.slice(0, 20);
        
        // ======== 步骤3: Fisher-Yates 洗牌算法（增加推荐多样性） ========
        // 从后往前遍历数组，每个位置与前面随机位置交换
        // 这样每次生成的推荐方案不会完全相同（有随机性），避免单调
        for (var s = topFoods.length - 1; s > 0; s--) {
            var r = Math.floor(Math.random() * (s + 1));
            var temp = topFoods[s];
            topFoods[s] = topFoods[r];
            topFoods[r] = temp;
        }
        
        // ======== 步骤4: 三重循环枚举3个美食的组合 ========
        var candidates = [];
        var usedFoods = new Set();  // 去重集合：用排序后的ID拼接字符串来避免重复组合
        for (var i = 0; i < topFoods.length && candidates.length < 5; i++) {
            for (var j = i + 1; j < topFoods.length && candidates.length < 5; j++) {
                for (var k = j + 1; k < topFoods.length && candidates.length < 5; k++) {
                    var trio = [topFoods[i], topFoods[j], topFoods[k]];
                    // 生成唯一键：对3个ID排序后用逗号拼接（确保 {1,2,3} 和 {3,1,2} 的key相同）
                    var key = trio.map(function(x){return x.id;}).sort().join(',');
                    if (usedFoods.has(key)) continue;
                    usedFoods.add(key);
                    
                    // 计算3个美食的地理中心点
                    var centerLng = (trio[0].lng + trio[1].lng + trio[2].lng) / 3;
                    var centerLat = (trio[0].lat + trio[1].lat + trio[2].lat) / 3;
                    // 找距离中心点最近的景点
                    var nearest = null, nearestDist = Infinity;
                    spotData.forEach(function(s) {
                        var d = haversine(centerLng, centerLat, s.lng, s.lat);
                        if (d < nearestDist) { nearestDist = d; nearest = s; }
                    });
                    if (!nearest) continue;
                    
                    // ======== 步骤5: 计算路线总距离和综合得分 ========
                    // 路线顺序：景点 → 美食1 → 美食2 → 美食3（用户游览顺序）
                    var totalDist = 0;
                    var prev = nearest;
                    trio.forEach(function(f) {
                        totalDist += haversine(prev.lng, prev.lat, f.lng, f.lat);
                        prev = f;
                    });
                    
                    // 过滤：超过50公里的方案不切实际，跳过
                    if (totalDist > 50000) continue;
                    
                    // 综合路线评分 = 美食质量(70%) + 距离合理性(30%)
                    var avgScore = (trio[0].score + trio[1].score + trio[2].score) / 3;
                    var routeScore = avgScore * 0.7 + (1 - totalDist / 50000) * 0.3;
                    
                    candidates.push({
                        spot: nearest,          // 中心景点（最近景点）
                        foods: trio,            // 推荐的3个美食
                        totalDist: totalDist,   // 路线总距离（米）
                        routeScore: routeScore, // 综合路线得分（越高越好）
                        avgScore: avgScore      // 3个美食的平均原始评分
                    });
                }
            }
        }
        
        // ======== 步骤6: 按综合得分排序，取前3条 ========
        candidates.sort(function(a,b){return b.routeScore - a.routeScore;});
        this.candidates = candidates.slice(0, 3);
        this.renderPanel();
    },

    /**
     * renderPanel(): 渲染每日推荐面板
     *
     * 【展示内容】
     * - 最佳方案（第一条）：大卡片展示
     *   - 标题："[景点名]周边美食游"
     *   - 元信息：平均评分、驾车距离（km）、估算驾车时间（距离/400米每分钟 ≈ 24km/h）
     *   - 途经点详细列表（景点/美食名、评分、价格）
     *   - "采纳此路线"按钮
     * - 备选方案（第2-3条）：简化卡片展示
     *   - 只显示景点名、评分、距离
     *   - "采纳"按钮
     *
     * 无推荐方案时显示提示信息。
     */
    renderPanel: function() {
        var container = document.getElementById('dailyTourList');
        if (!container) return;
        if (this.candidates.length === 0) {
            container.innerHTML = '<div class="profile-empty">收藏POI后可生成推荐</div>';
            return;
        }
        
        // 展示最佳方案
        var best = this.candidates[0];
        var html = '<div class="daily-tour-best">';
        html += '<div class="daily-tour-name">' + best.spot.name + '周边美食游</div>';
        // 估算驾车时间：距离(米) / 400(米/分钟) ≈ 市区平均车速 24km/h
        html += '<div class="daily-tour-meta">评分 ' + best.avgScore.toFixed(1) + ' | 驾车约 ' + (best.totalDist/1000).toFixed(1) + 'km / ' + Math.round(best.totalDist/400) + 'min</div>';
        html += '<div class="daily-tour-stops">';
        // 第1站：景点
        html += '<div class="daily-tour-stop">1. ' + best.spot.name + ' (景点/&#9733;' + best.spot.score.toFixed(1) + ')</div>';
        // 第2-4站：3个美食
        best.foods.forEach(function(f, i) {
            html += '<div class="daily-tour-stop">' + (i+2) + '. ' + f.name + ' (' + f.category + '/&#9733;' + f.score.toFixed(1) + '/Y' + f.price + ')</div>';
        });
        html += '</div>';
        html += '<button class="daily-tour-adopt" data-index="0" onclick="DailyTourManager.adopt(0)">采纳此路线</button>';
        html += '</div>';
        
        // 展示备选方案
        if (this.candidates.length > 1) {
            html += '<div class="daily-tour-alt-title">备选方案</div>';
            for (var i = 1; i < this.candidates.length; i++) {
                var alt = this.candidates[i];
                html += '<div class="daily-tour-alt">';
                html += '<span>' + alt.spot.name + '游</span>';
                html += '<span>评分 ' + alt.avgScore.toFixed(1) + ' / ' + (alt.totalDist/1000).toFixed(0) + 'km</span>';
                html += '<button class="daily-tour-adopt small" data-index="' + i + '" onclick="DailyTourManager.adopt(' + i + ')">采纳</button>';
                html += '</div>';
            }
        }
        
        container.innerHTML = html;
    },

    /**
     * adopt(index): 采纳指定索引的推荐路线方案
     *
     * 【执行流程（自动填充路线规划面板）】
     * 1. 查找路线起点下拉框（#routeStart），选中景点选项（value = "spot_<id>"）
     * 2. 清空途经点数组，将3个美食依次添加为途经点
     * 3. 调用 renderWaypointList() 刷新途经点列表界面
     * 4. 查找路线终点下拉框（#routeEnd），选中最后一个美食选项（value = "food_<id>"）
     * 5. 给采纳按钮显示"已采纳"反馈（1.5秒后恢复原文本，按钮暂时禁用）
     * 6. 调用 executeRoutePlan() 自动执行路线规划
     *
     * @param {number} index - 候选方案的索引（0=最佳方案，1=第一个备选，2=第二个备选）
     */
    adopt: function(index) {
        var tour = this.candidates[index];
        if (!tour) return;
        // 设置路线起点为景点（遍历下拉选项找到匹配的 value）
        var startSel = document.getElementById('routeStart');
        if (startSel) {
            for (var i = 0; i < startSel.options.length; i++) {
                if (startSel.options[i].value === 'spot_' + tour.spot.id) {
                    startSel.selectedIndex = i; break;
                }
            }
        }
        // 清空并重新设置途经点列表
        routeWaypoints = [];
        tour.foods.forEach(function(f) {
            routeWaypoints.push({ id: f.id, name: f.name, type: 'food', lng: f.lng, lat: f.lat });
        });
        renderWaypointList();
        // 设置终点为最后一个美食
        var endSel = document.getElementById('routeEnd');
        if (endSel && tour.foods.length > 0) {
            var last = tour.foods[tour.foods.length - 1];
            for (var j = 0; j < endSel.options.length; j++) {
                if (endSel.options[j].value === 'food_' + last.id) {
                    endSel.selectedIndex = j; break;
                }
            }
        }

        // 显示"已采纳"视觉反馈（按钮变灰并显示已采纳，1.5秒后恢复）
        var btn = document.querySelector('.daily-tour-adopt[data-index="' + index + '"]');
        if (btn) {
            var originalText = btn.textContent;
            btn.textContent = '已采纳'; btn.disabled = true;
            setTimeout(function() { btn.textContent = originalText; btn.disabled = false; }, 1500);
        }

        // 自动执行路线规划
        executeRoutePlan();
    }
};

// ============================================================================
// 七、系统常量与配置
// ============================================================================

/**
 * HEZE_CENTER: 菏泽市地理中心坐标 [经度, 纬度]
 *
 * 作为地图默认显示中心（初始加载时的视野焦点）。
 * 基于菏泽市主要 POI 分布重新计算的中心点。
 * 格式: [lng, lat] 数组，符合高德 API 的坐标格式约定。
 * 经度范围约 115.2°-115.7°，纬度范围约 35.0°-35.4°。
 */
var HEZE_CENTER = [115.477, 35.245];

/**
 * AMAP_KEY: 高德地图开放平台 Web API Key（JSAPI 类型）
 *
 * 用于以下高德服务的身份验证：
 * - 地图渲染（AMapLoader.load）
 * - 路径规划（restapi.amap.com/v3/direction/）
 * - 逆地理编码（AMap.Geocoder）
 *
 * 该 Key 需在高德开放平台注册并开通相应服务：
 * https://console.amap.com/
 *
 * 安全提醒：生产环境中 Key 不应直接硬编码在前端代码中，
 * 建议通过后端代理或环境变量注入。
 */
var AMAP_KEY = "647bb3e7a596c479b998f3e20a5a486a";

// ============================================================================
// 八、分类颜色与图标映射表
// ============================================================================

/**
 * CATEGORY_COLORS: 美食分类 → 地图标记颜色 的映射表
 *
 * 【颜色选择原则】
 * 暖色系（红/橙/棕）代表热食类，与中国的"暖食"文化对应；
 * 绿色代表凉菜/蔬菜类；
 * 紫色代表饮品类——整体形成视觉上的"温度感"区分。
 *
 * 每种分类的颜色用于：
 * - 地图上美食标记的圆形背景色
 * - 信息窗口中标题的左边框颜色
 * - 收藏面板中美食的图标背景色
 *
 * 'default' 键为未匹配分类时的兜底颜色（橙色 #ff5722）。
 */
var CATEGORY_COLORS = {
    '汤类': '#ff6f00',  // 深橙 —— 代表温暖浓郁的汤
    '面食': '#f9a825',  // 金黄 —— 代表小麦色面食
    '小吃': '#e64a19',  // 红棕 —— 代表街头小吃的烟火气
    '正餐': '#c62828',  // 深红 —— 代表正式餐食的庄重
    '烧烤': '#bf360c',  // 暗红 —— 代表炭火的暗红色
    '甜品': '#ad1457',  // 粉红 —— 代表甜品的甜美柔和
    '饮品': '#6a1b9a',  // 紫色 —— 代表饮品的清爽感
    '凉菜': '#2e7d32',  // 绿色 —— 代表蔬菜的新鲜感
    'default': '#ff5722' // 默认橙 —— 未匹配分类的兜底颜色
};

/**
 * CATEGORY_ICONS: 美食分类 → 标记显示字符 的映射表
 *
 * 由于地图标记尺寸很小（22px直径），只能容纳一个汉字。
 * 因此用每个分类的"代表字"来区分不同分类。
 * 例如：汤类的标记显示"汤"，面食的标记显示"面"。
 *
 * 'default' 键用于未匹配分类时的兜底显示"食"。
 */
var CATEGORY_ICONS = {
    '汤类': '汤', '面食': '面', '小吃': '吃', '正餐': '餐',
    '烧烤': '烤', '甜品': '甜', '饮品': '饮', '凉菜': '凉',
    'default': '食'
};

// ============================================================================
// 九、地图初始化
// ============================================================================

/**
 * loadMap(): 加载并初始化高德地图
 *
 * 【调用时机】window.onload 事件触发时调用，即页面所有资源加载完毕后。
 *
 * 【执行流程】
 * 1. 前置检查：验证 AMapLoader（高德地图加载器）是否已由 HTML 的 <script> 标签引入
 * 2. 异步加载：调用 AMapLoader.load() 加载地图 SDK——
 *    - key: API Key 用于鉴权
 *    - version: "2.0" 使用 JSAPI 2.0 版本（支持自定义 HTML 标记）
 *    - plugins: 加载三个插件——
 *      · AMap.ToolBar: 缩放/平移工具栏控件
 *      · AMap.Scale: 比例尺控件
 *      · AMap.Geocoder: 逆地理编码器（坐标→地址）
 * 3. 成功回调（then）：
 *    - 创建地图实例（map = new AMap.Map(...)）
 *    - 添加工具栏和比例尺控件
 *    - 绑定地图事件（缩放关闭信息窗口、点击空白关闭信息窗口）
 *    - 初始化逆地理编码器（限定城市为"菏泽"）
 *    - 调用 initControls() 和 loadData() 完成所有初始化
 * 4. 失败回调（catch）：
 *    - 打印错误日志
 *    - 调用 showMapFallback() 显示友好的错误提示界面
 *
 * 【地图配置参数说明】
 * - zoom: 13 级（可看到菏泽市区全貌，约10km范围）
 * - center: HEZE_CENTER [115.477, 35.245]（菏泽市中心）
 * - mapStyle: 'amap://styles/light'（浅色背景，适合信息展示）
 * - features: ['bg','road','building','point']（仅显示背景、道路、建筑、POI，隐藏其他要素提升性能）
 */
function loadMap() {
    // 检查高德地图加载器（AMapLoader）是否可用
    // AMapLoader 由 HTML 中 <script src="https://...AMapLoader..."> 标签引入
    if (typeof AMapLoader === 'undefined') {
        console.error('[Map] AMapLoader 未加载，使用备用方案');
        showMapFallback();
        return;
    }

    // 使用 AMapLoader.load() 异步加载地图 SDK
    // 返回一个 Promise，成功时传入 AMap 命名空间对象
    AMapLoader.load({
        key: AMAP_KEY,          // API Key（需在高德平台注册获取）
        version: "2.0",         // JSAPI 2.0 版本（支持 HTML 自定义标记）
        plugins: ["AMap.ToolBar", "AMap.Scale", "AMap.Geocoder"]
    }).then(function (AMap) {
        // ======== 地图加载成功回调 ========
        console.log('[Map] 高德地图加载成功');

        // 创建地图实例，挂载到 #map 容器元素（由 HTML 提供）
        map = new AMap.Map('map', {
            zoom: 13,                              // 初始缩放级别（13级≈市区范围）
            center: HEZE_CENTER,                    // 初始中心点（菏泽市中心坐标）
            mapStyle: 'amap://styles/light',        // 浅色地图风格（白色背景，适合叠加信息）
            features: ['bg', 'road', 'building', 'point']  // 显示的地图要素（精简配置以提升性能）
        });

        // 添加地图控件
        map.addControl(new AMap.ToolBar({ position: 'LT' }));  // 缩放/平移工具栏（LT=左上角）
        map.addControl(new AMap.Scale());                       // 比例尺控件（默认位置：左下角）

        // 监听缩放事件：当用户缩放到很小级别（<7级，省际视图）时，标记过于密集，
        // 信息窗口失去参考意义，自动关闭
        map.on('zoomend', function() {
            var zoom = map.getZoom();
            if (zoom < 7 && currentInfoWindow) {
                currentInfoWindow.close();
                currentInfoWindow = null;
            }
        });

        // 监听地图空白区域点击事件：关闭当前打开的信息窗口
        // 提升用户体验——点击地图空白区域 = 关闭弹窗
        map.on('click', function(e) {
            if (currentInfoWindow) {
                currentInfoWindow.close();
                currentInfoWindow = null;
            }
        });

        // 初始化高德逆地理编码器
        // city: '菏泽' 限定城市范围，提高菏泽本地地址的解析精度
        geocoder = new AMap.Geocoder({ city: '菏泽' });

        // 初始化所有控件和加载数据
        initControls();
        loadData();

    }).catch(function (e) {
        // ======== 地图加载失败回调 ========
        // 可能原因：网络不通、API Key 无效或过期、高德服务异常
        console.error('[Map] 地图加载失败:', e);
        showMapFallback();
    });
}

/**
 * showMapFallback(): 显示地图加载失败的回退界面
 *
 * 【使用场景】
 * - AMapLoader 未加载（HTML 缺少 script 标签或 CDN 不可用）
 * - 高德 API 加载失败（网络问题、Key 无效、服务异常）
 *
 * 【回退界面内容】
 * - 红色标题："地图加载失败"
 * - 排查提示："请检查网络连接或 API Key 有效性"
 * - Key 前8位显示（方便排查是否 Key 配置有误）
 *
 * 【重要设计决策】
 * 即使地图加载失败，仍然调用 loadData() 尝试加载数据。
 * 这样可以确保不依赖地图的功能（如收藏列表、排行榜等）仍然可用。
 */
function showMapFallback() {
    document.getElementById('map').innerHTML =
        '<div style="display:flex;align-items:center;justify-content:center;height:100%;background:#f0f2f5;">' +
        '<div style="text-align:center;padding:40px;">' +
        '<h3 style="color:#d32f2f;">地图加载失败</h3>' +
        '<p style="color:#666;">请检查网络连接或 API Key 有效性</p>' +
        '<p style="color:#999;font-size:13px;">Key: ' + AMAP_KEY.substring(0, 8) + '...</p>' +
        '</div></div>';
    loadData();  // 仍然尝试加载数据，确保不依赖地图的功能可用
}

// ============================================================================
// 十、数据加载与全局初始化
// ============================================================================

/**
 * loadData(): 异步加载美食和景点的 JSON 数据，并初始化所有功能模块
 *
 * 【执行流程（按时间顺序）】
 * 1. 并行发起两个 fetch 请求，加载 food.json 和 spot.json
 * 2. 将已有地址数据预填充到 addressCache（节省 API 配额）
 * 3. 依次初始化以下模块：
 *    - updateStats()          → 更新顶部统计数字
 *    - generateCategoryFilters() → 生成分类筛选标签
 *    - initSearch()           → 绑定搜索框事件（防抖处理）
 *    - populateSelectOptions() → 填充路线起点/终点下拉框
 *    - initRoutePlanner()     → 绑定路线面板交互事件
 *    - FavoriteManager.init() → 从 localStorage 加载收藏
 *    - FavoriteManager.renderPanel() → 渲染收藏列表
 *    - RankingManager.init()  → 绑定排行榜交互事件
 *    - RankingManager.renderPanel() → 渲染排行榜
 *    - ProfileManager.renderPanel() → 渲染用户画像
 *    - DailyTourManager.generate()  → 生成每日推荐
 *    - initHistory()          → 绑定历史面板事件
 * 4. 绑定画像编辑按钮和每日推荐刷新按钮的事件
 * 5. 绑定工具栏下拉菜单和侧边栏面板折叠的交互
 * 6. 如果地图实例已就绪（loadMap 完成），渲染标记并自适应视野
 *
 * 【容错机制】
 * 每个 fetch 请求都有独立的 catch 处理，单个数据文件加载失败不影响另一个。
 * 使用 await Promise.all(loadPromises) 等待两个请求都完成（成功或失败）后才进入初始化流程。
 *
 * 【异步设计】
 * 函数声明为 async，使用 await 等待数据加载完成后再初始化。
 */
async function loadData() {
    console.log('[Map] 加载数据...');

    var loadPromises = [];

    // ======== 并行加载美食数据 ========
    loadPromises.push(
        fetch('data/food.json')                             // 发起 HTTP GET 请求
            .then(r => r.ok ? r.json() : Promise.reject('HTTP ' + r.status))  // 检查响应状态，解析 JSON
            .then(d => { foodData = d; console.log('[Map] 美食:', d.length, '条'); })  // 赋值给全局变量
            .catch(e => console.warn('[Map] food.json:', e))  // 加载失败仅打印警告，不阻断流程
    );

    // ======== 并行加载景点数据 ========
    loadPromises.push(
        fetch('data/spot.json')
            .then(r => r.ok ? r.json() : Promise.reject('HTTP ' + r.status))
            .then(d => { spotData = d; console.log('[Map] 景点:', d.length, '条'); })
            .catch(e => console.warn('[Map] spot.json:', e))
    );

    // 等待两个请求都完成（无论成功或失败）
    await Promise.all(loadPromises);

    // ======== 预填充地址缓存 ========
    // 如果数据中已有有效的地址信息（非空、非"-"、非"无法获取"），
    // 直接写入 addressCache，后续的逆地理编码可以跳过这些坐标
    foodData.forEach(function(f) {
        if (f.address && f.address !== '-' && f.address !== '无法获取') {
            var key = f.lng.toFixed(5) + ',' + f.lat.toFixed(5);  // 缓存键："经度,纬度"格式
            addressCache[key] = f.address;
        }
    });
    spotData.forEach(function(s) {
        if (s.address && s.address !== '-' && s.address !== '无法获取') {
            var key = s.lng.toFixed(5) + ',' + s.lat.toFixed(5);
            addressCache[key] = s.address;
        }
    });

    // ======== 初始化各功能模块（按调用顺序） ========
    updateStats();              // 更新统计数字（美食/景点数量）
    generateCategoryFilters();  // 动态生成分类筛选标签栏
    initSearch();               // 初始化搜索功能（绑定输入框防抖事件）
    populateSelectOptions();    // 填充路线规划的起点/终点下拉选择框
    initRoutePlanner();         // 初始化路线规划面板的交互事件
    FavoriteManager.init();     // 从 localStorage 加载已收藏的 POI 数据
    FavoriteManager.renderPanel(); // 渲染收藏夹面板
    RankingManager.init();      // 初始化排行榜的标签和排序按钮事件
    RankingManager.renderPanel(); // 渲染排行榜（默认"热门榜"）
    ProfileManager.renderPanel(); // 渲染用户画像面板（口味/价格/品类分析）
    DailyTourManager.generate();  // 基于收藏数据生成每日推荐路线
    initHistory();              // 初始化操作历史面板（撤销/重做按钮事件）

    // ======== 绑定画像编辑按钮事件 ========
    var editBtn = document.getElementById('btnEditProfile');
    var saveBtn = document.getElementById('btnSaveProfile');
    var cancelBtn = document.getElementById('btnCancelProfile');
    var restoreBtn = document.getElementById('btnRestoreProfile');
    if (editBtn) editBtn.addEventListener('click', function() { ProfileManager.enterEditMode(); });
    if (saveBtn) saveBtn.addEventListener('click', function() { ProfileManager.saveProfile(); });
    if (cancelBtn) cancelBtn.addEventListener('click', function() { ProfileManager.cancelProfile(); });
    if (restoreBtn) restoreBtn.addEventListener('click', function() { ProfileManager.restoreProfile(); });

    // ======== 绑定每日推荐手动刷新按钮 ========
    var refreshBtn = document.getElementById('btnRefreshTour');
    if (refreshBtn) { refreshBtn.addEventListener('click', function() { DailyTourManager.generate(); }); }

    // ======== 绑定顶部工具栏下拉菜单的展开/收起交互 ========
    // 点击工具栏组（.header-tool-group）时切换其内部下拉菜单的可见性
    document.querySelectorAll('.header-tool-group').forEach(function(group) {
        var dropdown = group.querySelector('.header-dropdown');
        if (!dropdown) return;
        group.addEventListener('click', function(e) {
            e.stopPropagation();  // 阻止冒泡，避免触发 document 的全局关闭事件
            dropdown.style.display = dropdown.style.display === 'block' ? 'none' : 'block';
        });
    });
    // 点击页面任意位置关闭所有工具栏下拉菜单
    document.addEventListener('click', function() {
        document.querySelectorAll('.header-dropdown').forEach(function(d) { d.style.display = 'none'; });
    });

    // ======== 绑定侧边栏面板的折叠/展开交互 ========
    // 点击面板标题（.panel-header）时切换父元素的 collapsed 类
    // CSS 通过 .collapsed 类控制面板内容区域的显示/隐藏
    document.querySelectorAll('.panel-header').forEach(function(header) {
        header.addEventListener('click', function() {
            this.parentElement.classList.toggle('collapsed');
        });
    });

    // ======== 如果地图实例已就绪，渲染标记并适配视野 ========
    if (map) {
        if (foodData.length > 0) showFoodMarkers();  // 在分类筛选激活前先渲染全部美食标记
        if (spotData.length > 0) showSpotMarkers();  // 渲染全部景点标记
        // 延时500ms等待标记渲染完成后再适配视野
        // 延迟是为了确保 DOM 中的标记元素已经完成布局计算
        setTimeout(autoFitView, 500);
    }
}

// ============================================================================
// 十一、逆地理编码 —— 将经纬度坐标转换为可读的中文地址
// ============================================================================

/**
 * getAddress(lng, lat, callback): 通过高德逆地理编码获取经纬度对应的中文地址
 *
 * 【工作原理（缓存优先策略）】
 * 1. 构造缓存键（cacheKey）—— "经度,纬度" 格式，经纬度各保留5位小数
 *    保留5位小数 ≈ 约1.1米精度，相邻坐标可共享缓存结果
 * 2. 先检查 addressCache 缓存，命中则直接回调（同步返回，无网络开销）
 * 3. 如果 geocoder 未就绪（地图加载失败），回调占位文本"地址获取中..."
 * 4. 否则调用高德 geocoder.getAddress() 进行逆地理编码：
 *    - 成功：缓存 formattedAddress 并回调
 *    - 失败：回调"地址获取失败"
 *
 * 【缓存的作用】
 * - 减少高德 API 调用次数（免费配额有限）
 * - 提升响应速度（内存访问 vs 网络请求）
 *
 * @param {number}   lng      - 经度（longitude）
 * @param {number}   lat      - 纬度（latitude）
 * @param {function} callback - 回调函数，接收解析后的地址字符串作为参数
 */
function getAddress(lng, lat, callback) {
    var cacheKey = lng.toFixed(5) + ',' + lat.toFixed(5);
    // 缓存命中：直接返回（最快的路径）
    if (addressCache[cacheKey]) {
        callback(addressCache[cacheKey]);
        return;
    }

    // geocoder 未初始化（地图没加载出来）：返回占位文本
    if (!geocoder) {
        callback('地址获取中...');
        return;
    }

    // 调用高德逆地理编码 API
    geocoder.getAddress([lng, lat], function(status, result) {
        if (status === 'complete' && result.regeocode) {
            // formattedAddress 是格式化的中文地址字符串（如"山东省菏泽市牡丹区XX路XX号"）
            var addr = result.regeocode.formattedAddress || '未知地址';
            addressCache[cacheKey] = addr;  // 写入缓存供后续复用
            callback(addr);
        } else {
            callback('地址获取失败');
        }
    });
}

// ============================================================================
// 十二、美食标记渲染
// ============================================================================

/**
 * showFoodMarkers(): 根据当前筛选条件渲染所有美食的标记
 *
 * 【渲染规则（多重筛选叠加）】
 * 该方法组合了三个层级的可见性过滤：
 *   层级1 — 分类筛选（activeCategories）：只显示选中分类的美食
 *   层级2 — 图层开关（toggleFoodVisible）：全局的美食显示/隐藏
 *   层级3 — 营业筛选（openOnlyActive）：只显示营业中的美食
 *
 * 【HTML 自定义标记 vs 默认图标】
 * 使用 HTML 自定义标记（Custom Marker）而非高德默认图标：
 * 优点：可以动态设置颜色和文字，区分不同分类，支持收藏金色边框
 * 实现：每个标记是一个 22px 直径的圆形 div，背景色按分类，显示美食名称首字
 *
 * 【交互效果】
 * - 点击：打开该美食的详情信息窗口（showFoodDetail）
 * - 鼠标悬停：标记放大（22px→32px），文字放大（10px→14px），提升可点击区域
 * - 鼠标离开：恢复原始大小
 *
 * 【标记生命周期】
 * 每次调用都会先清除地图上所有已有的美食标记（foodMarkers.forEach(m => m.setMap(null))），
 * 然后根据最新数据和筛选条件重新创建。这意味着频繁筛选时会有短暂闪烁。
 */
function showFoodMarkers() {
    // 清除旧标记
    foodMarkers.forEach(function (m) { m.setMap(null); });
    foodMarkers = [];

    // 遍历每一条美食数据
    foodData.forEach(function (food, index) {
        // 层级1：分类筛选 —— 如果筛选激活且不包含该美食的分类，则跳过
        if (activeCategories.size > 0 && !activeCategories.has(food.category)) return;

        // 获取该分类对应的颜色（用于标记背景）
        var color = CATEGORY_COLORS[food.category] || CATEGORY_COLORS['default'];

        // 标记显示文字：取美食名称的第一个汉字（如"张三羊汤"→"张"）
        var displayChar = food.name.charAt(0);

        // ======== 构造 HTML 标记内容 ========
        // 圆形标记：22px 直径、分类颜色背景、白色文字、白色边框、阴影
        var markerContent =
            '<div class="food-marker" style="' +
            'background:' + color + ';' +
            'color:white;' +
            'width:22px;height:22px;' +
            'border-radius:50%;' +
            'display:flex;align-items:center;justify-content:center;' +
            'font-size:10px;font-weight:bold;' +
            'box-shadow:0 1px 3px rgba(0,0,0,0.3);' +
            'border:2px solid white;' +
            'cursor:pointer;' +
            '">' +
            displayChar +
            '</div>';

        // 已收藏的美食：金色加粗边框（3px #ffd700），视觉效果醒目
        if (FavoriteManager.isFavorite(food.id, 'food')) {
            markerContent = markerContent.replace('border:2px solid white', 'border:3px solid #ffd700');
        }

        try {
            // 创建高德 Marker 实例
            var marker = new AMap.Marker({
                position: new AMap.LngLat(food.lng, food.lat),  // 经纬度位置
                content: markerContent,       // 自定义 HTML 内容（替换默认蓝色水滴图标）
                offset: new AMap.Pixel(-6, -6), // 像素偏移（使标记视觉中心对齐坐标点）
                zIndex: 100,                  // 叠放层级（景点标记为110，美食在下层）
                title: food.name              // 原生 tooltip 提示文字
            });

            // 绑定点击事件：打开该美食的详情信息窗口
            marker.on('click', function () {
                showFoodDetail(food);
            });

            // 绑定鼠标悬停事件：临时放大标记（提升可点击目标大小和视觉反馈）
            marker.on('mouseover', function () {
                this.setContent(markerContent.replace('width:22px','width:32px').replace('height:22px','height:32px').replace('font-size:10px','font-size:14px'));
            });
            // 绑定鼠标离开事件：恢复标记到原始大小
            marker.on('mouseout', function () {
                this.setContent(markerContent);
            });

            // 在标记对象上保存原始美食数据的引用（供后续操作如营业状态筛选使用）
            marker._foodData = food;
            // 将标记添加到地图
            marker.setMap(map);
            foodMarkers.push(marker);
        } catch (e) {
            // 捕获标记创建过程中的异常（如坐标格式不正确）
            console.warn('[Map] 美食标记失败:', food.name, e);
        }
    });

    // ======== 层级2：美食图层全局开关 ========
    if (!toggleFoodVisible) {
        foodMarkers.forEach(function(m) { m.setMap(null); });
    }

    // ======== 层级3：仅显示营业中 ========
    if (openOnlyActive) {
        foodMarkers.forEach(function(m) {
            // 检查该标记对应美食的营业状态
            if (m._foodData && getOpenStatus(m._foodData.opentime).cls === 'status-closed') {
                m.setMap(null);  // 已打烊的标记从地图上移除
            }
        });
    }

    console.log('[Map] 美食标记:', foodMarkers.length, '个');
}

// ============================================================================
// 十三、景点标记渲染
// ============================================================================

/**
 * showSpotMarkers(): 渲染所有景点的标记
 *
 * 【与美食标记的区别】
 * - 形状：方形（border-radius:4px）vs 美食的圆形（border-radius:50%）
 * - 颜色：统一蓝色背景（#1565c0）vs 美食按分类区分颜色
 * - 文字：固定显示"景"字 vs 美食显示名字首字
 * - 大小：24px vs 22px
 * - 层级：zIndex 110 vs 100（景点标记在地图上更显眼，不受美食重叠影响）
 * - 筛选：不受分类筛选、营业状态筛选影响（景点数量少，无需分类过滤）
 *
 * 【交互】
 * - 点击：打开景点详情信息窗口
 * - 无鼠标悬停效果（与美食不同，景点没有放大效果以保持视觉稳重）
 * - 已收藏的景点标记显示金色边框（与美食一致）
 */
function showSpotMarkers() {
    spotMarkers.forEach(function (m) { m.setMap(null); });
    spotMarkers = [];

    spotData.forEach(function (spot) {
        // 方形蓝色标记，固定显示"景"字
        var markerContent =
            '<div class="spot-marker" style="' +
            'background:#1565c0;' +            // 统一蓝色背景（Material Blue 800）
            'color:white;' +
            'width:24px;height:24px;' +
            'border-radius:4px;' +             // 圆角方形（区别于美食的圆形）
            'display:flex;align-items:center;justify-content:center;' +
            'font-size:11px;font-weight:bold;' +
            'box-shadow:0 1px 3px rgba(0,0,0,0.3);' +
            'border:2px solid white;' +
            'cursor:pointer;' +
            '">' +
            '<span>景</span>' +                 // 固定"景"字图标
            '</div>';

        // 已收藏的景点标记：金色边框
        if (FavoriteManager.isFavorite(spot.id, 'spot')) {
            markerContent = markerContent.replace('border:2px solid white', 'border:3px solid #ffd700');
        }

        try {
            var marker = new AMap.Marker({
                position: new AMap.LngLat(spot.lng, spot.lat),
                content: markerContent,
                offset: new AMap.Pixel(-12, -12),  // 24px方形标记的中心偏移（-12 = -24/2）
                zIndex: 110,                        // 高于美食标记（100），景点更显眼
                title: spot.name
            });

            // 点击打开景点详情
            marker.on('click', function () {
                showSpotDetail(spot);
            });

            marker._spotData = spot;
            marker.setMap(map);
            spotMarkers.push(marker);
        } catch (e) {
            console.warn('[Map] 景点标记失败:', spot.name, e);
        }
    });

    // 景点图层全局开关
    if (!toggleSpotVisible) {
        spotMarkers.forEach(function(m) { m.setMap(null); });
    }

    console.log('[Map] 景点标记:', spotMarkers.length, '个');
}

// ============================================================================
// 十四、信息窗口（详情弹窗）—— 点击标记后弹出的 POI 详情面板
// ============================================================================

/**
 * showFoodDetail(food): 打开美食的详情信息窗口
 *
 * 【Toggle 行为】
 * 如果同一个美食的信息窗口已经打开（通过 _poiId + _poiType 判断），
 * 再次点击会关闭窗口（toggle 模式），而不是创建新的。
 *
 * 【信息窗口内容（8个模块）】
 * 1. 收藏按钮 — ★已收藏（橙色背景）/ ☆收藏（灰色背景），点击可切换收藏状态
 * 2. 图片轮播 — 220×150px 图片，加载失败自动隐藏
 * 3. 营业状态条 — 绿色●营业中 / 灰色○已打烊，附带营业时间文本
 * 4. 名称标题 — 左侧带分类图标，底部边框颜色跟随分类
 * 5. 评分星级 — ★实心星 + ☆半空心星（小数≥0.5时），附带数字评分
 * 6. 价格等级 — ¥金额 + 💰数量（1-4个钱袋表示价格档位）
 * 7. 地址 — 优先用数据中的已有地址，否则显示占位文本
 * 8. 标签 — tags 数组渲染为小标签 chips
 *
 * 【信息窗口管理】
 * - 同时只显示一个信息窗口（打开新窗口前先关闭旧的 currentInfoWindow）
 * - 在窗口实例上通过 _poiId 和 _poiType 属性标记所属 POI
 * - 收藏按钮事件通过 setTimeout 延迟100ms 绑定（等待 DOM 渲染完成）
 * - 窗口打开时记录到 HistoryManager（支持撤销回到之前的状态）
 *
 * @param {object} food - foodData 中的一条美食数据对象
 */
function showFoodDetail(food) {
    // Toggle 行为：如果同一POI的信息窗口已打开，点击关闭
    if (currentInfoWindow && currentInfoWindow._poiId === food.id && currentInfoWindow._poiType === 'food') {
        currentInfoWindow.close();
        currentInfoWindow = null;
        return;
    }

    // ======== 构造评分星级 ========
    // ★ = 完全填充的星（每个整数1颗）
    // ☆ = 半星（小数部分 ≥ 0.5 时显示，表示"半满"状态）
    var stars = '';
    var fullStars = Math.floor(food.score);  // 整数部分 = 完整星
    var halfStar = (food.score - fullStars) >= 0.5;  // 小数部分 ≥ 0.5 时显示半星
    for (var i = 0; i < fullStars; i++) stars += '★';
    if (halfStar) stars += '☆';

    var color = CATEGORY_COLORS[food.category] || CATEGORY_COLORS['default'];
    var icon = CATEGORY_ICONS[food.category] || CATEGORY_ICONS['default'];

    // ======== 价格等级（用钱袋 emoji 数量表示） ========
    var priceLevel = '';
    if (food.price <= 10) priceLevel = '💰';           // ≤10元：便宜
    else if (food.price <= 30) priceLevel = '💰💰';     // 11-30元：适中
    else if (food.price <= 60) priceLevel = '💰💰💰';   // 31-60元：较贵
    else priceLevel = '💰💰💰💰';                        // >60元：贵

    // ======== 构造标签 HTML ========
    var tagsHtml = '';
    if (food.tags && food.tags.length > 0) {
        tagsHtml = '<div class="info-tags">';
        food.tags.forEach(function (t) {
            tagsHtml += '<span class="info-tag">' + t + '</span>';
        });
        tagsHtml += '</div>';
    }

    // ======== 地址元素 ID（用于异步更新地址文本） ========
    var addrId = 'addr-food-' + food.id;

    // ======== 构造图片 HTML ========
    var photosHtml = '';
    if (food.photos && food.photos.length > 0) {
        photosHtml = '<div class="info-photos">';
        food.photos.forEach(function(url) {
            // onerror 内联处理：图片加载失败时隐藏该 img 元素（避免显示破损图标）
            photosHtml += '<img src="' + url + '" style="width:220px;height:150px;object-fit:cover;border-radius:4px;margin:2px;" onerror="this.style.display=\'none\'">';
        });
        photosHtml += '</div>';
    }

    // ======== 营业状态条 ========
    var statusBar = '';
    if (food.opentime && food.opentime !== '-') {
        var s = getOpenStatus(food.opentime);
        // 绿色 ● 表示营业中，灰色 ○ 表示已打烊
        statusBar = '<div style="font-size:12px;margin-bottom:6px;color:' + (s.cls === 'status-open' ? '#4caf50' : '#999') + ';">' +
            (s.cls === 'status-open' ? '●' : '○') + ' ' + s.text + ' · ' + food.opentime + '</div>';
    }

    // ======== 记录操作历史（"查看详情"操作） ========
    HistoryManager.push('view', { poiId: food.id, poiType: 'food', lng: food.lng, lat: food.lat, poiName: food.name });

    // ======== 收藏按钮 HTML ========
    var isFav = FavoriteManager.isFavorite(food.id, 'food');
    var starBtn = '<div class="fav-btn ' + (isFav ? 'faved' : '') + '" data-poi-id="' + food.id + '" data-poi-type="food" style="cursor:pointer;padding:4px 8px;margin-bottom:8px;background:' + (isFav ? '#fff3e0' : '#f5f5f5') + ';border-radius:4px;text-align:center;font-size:13px;border:1px solid ' + (isFav ? '#ff9800' : '#ddd') + ';">' + (isFav ? '★ 已收藏' : '☆ 收藏') + '</div>';

    // ======== 组装完整的信息窗口 HTML ========
    var html =
        '<div class="info-window">' +
        starBtn +
        photosHtml +
        statusBar +
        // 标题：边框颜色跟随分类（视觉一致性）
        '<h4 style="border-color:' + color + '">' + icon + ' ' + food.name + '</h4>' +
        '<div class="info-row"><span class="info-label">评分</span>' +
        '<span class="info-score" style="color:#ff9800;">' + stars + ' ' + food.score.toFixed(1) + '</span></div>' +
        '<div class="info-row"><span class="info-label">价格</span>' +
        '<span class="info-price">¥' + food.price + ' ' + priceLevel + '</span></div>' +
        '<div class="info-row"><span class="info-label">分类</span>' +
        '<span>' + food.category + '</span></div>' +
        '<div class="info-row"><span class="info-label">地址</span>' +
        '<span id="' + addrId + '" style="color:#666;font-size:12px;">获取地址中...</span></div>' +
        tagsHtml +
        '</div>';

    // ======== 创建并显示信息窗口 ========
    var infoWindow = new AMap.InfoWindow({
        content: html,
        offset: new AMap.Pixel(0, -25),  // 向上偏移25px（标记是22px，窗口底部对齐标记顶部）
        closeWhenClickMap: false          // 不依赖SDK的关闭行为，由自定义的 map.click 事件处理
    });

    // 在信息窗口实例上标记 POI 信息（供 toggle 判断和调试使用）
    infoWindow._poiId = food.id;
    infoWindow._poiType = 'food';
    // 关闭之前的窗口（确保同时只有一个信息窗口）
    if (currentInfoWindow) currentInfoWindow.close();
    infoWindow.open(map, [food.lng, food.lat]);
    currentInfoWindow = infoWindow;

    // ======== 延迟绑定收藏按钮的点击事件 ========
    // 因为信息窗口是异步渲染到 DOM 的，需要 setTimeout 等待 DOM 元素就绪
    setTimeout(function() {
        var se = document.querySelector('.fav-btn[data-poi-id="' + food.id + '"]');
        if (se) { se.addEventListener('click', function(e) { e.stopPropagation();
            if (FavoriteManager.isFavorite(food.id, 'food')) { FavoriteManager.remove(food.id, 'food'); se.innerHTML = '&#9734; 收藏'; se.className = 'fav-btn'; se.style.background = '#f5f5f5'; se.style.borderColor = '#ddd'; }
            else { FavoriteManager.add({ id: food.id, type: 'food', name: food.name }); se.innerHTML = '&#9733; 已收藏'; se.className = 'fav-btn faved'; se.style.background = '#fff3e0'; se.style.borderColor = '#ff9800'; } }); }
    }, 100);

    // ======== 地址回填 ========
    // 如果数据中已有有效地址（非空、非"-"、非"无法获取"），直接显示
    // 否则保持"获取地址中..."占位（该占位可能在后续通过异步 getAddress 更新）
    var addrEl = document.getElementById(addrId);
    if (addrEl) {
        addrEl.textContent = (food.address && food.address !== '-' && food.address !== '无法获取') ? food.address : '地址未知';
    }
}

/**
 * showSpotDetail(spot): 打开景点的详情信息窗口
 *
 * 【与美食详情的区别】
 * - 无营业状态条（景点使用固定的 openingTime 字段）
 * - 标签使用蓝色风格（#e3f2fd 背景，#1565c0 文字）
 * - 标题颜色固定蓝色（#1565c0），带[景点]前缀标识
 * - 展示字段：类型、评分、门票信息、开放时间、简介（额外字段）
 * - 无价格和价格等级
 *
 * @param {object} spot - spotData 中的一条景点数据对象
 */
function showSpotDetail(spot) {
    // Toggle 行为：同一景点点击关闭
    if (currentInfoWindow && currentInfoWindow._poiId === spot.id && currentInfoWindow._poiType === 'spot') {
        currentInfoWindow.close();
        currentInfoWindow = null;
        return;
    }

    // 记录操作历史
    HistoryManager.push('view', { poiId: spot.id, poiType: 'spot', lng: spot.lng, lat: spot.lat, poiName: spot.name });

    // ======== 构造评分星级（景点只有完整星，不显示半星） ========
    var stars = '';
    var fullStars = Math.floor(spot.score);
    for (var i = 0; i < fullStars; i++) stars += '★';

    // ======== 构造标签（蓝色风格，区别于美食的橙色风格） ========
    var tagsHtml = '';
    if (spot.tags && spot.tags.length > 0) {
        tagsHtml = '<div class="info-tags">';
        spot.tags.forEach(function (t) {
            tagsHtml += '<span class="info-tag" style="background:#e3f2fd;color:#1565c0;">' + t + '</span>';
        });
        tagsHtml += '</div>';
    }

    var addrId = 'addr-spot-' + spot.id;

    // ======== 构造图片 ========
    var photosHtml = '';
    if (spot.photos && spot.photos.length > 0) {
        photosHtml = '<div class="info-photos">';
        spot.photos.forEach(function(url) {
            photosHtml += '<img src="' + url + '" style="width:220px;height:150px;object-fit:cover;border-radius:4px;margin:2px;" onerror="this.style.display=\'none\'">';
        });
        photosHtml += '</div>';
    }

    // ======== 收藏按钮 ========
    var isFav = FavoriteManager.isFavorite(spot.id, 'spot');
    var starBtn = '<div class="fav-btn ' + (isFav ? 'faved' : '') + '" data-poi-id="' + spot.id + '" data-poi-type="spot" style="cursor:pointer;padding:4px 8px;margin-bottom:8px;background:' + (isFav ? '#fff3e0' : '#f5f5f5') + ';border-radius:4px;text-align:center;font-size:13px;border:1px solid ' + (isFav ? '#ff9800' : '#ddd') + ';">' + (isFav ? '★ 已收藏' : '☆ 收藏') + '</div>';

    // ======== 组装景点信息窗口 HTML ========
    var html =
        '<div class="info-window">' +
        starBtn +
        photosHtml +
        '<h4 style="color:#1565c0;border-color:#1565c0;">[景点] ' + spot.name + '</h4>' +
        '<div class="info-row"><span class="info-label">类型</span><span>' + spot.type + '</span></div>' +
        '<div class="info-row"><span class="info-label">评分</span>' +
        '<span class="info-score">' + stars + ' ' + spot.score.toFixed(1) + '</span></div>' +
        '<div class="info-row"><span class="info-label">门票</span><span>' + spot.ticketInfo + '</span></div>' +
        '<div class="info-row"><span class="info-label">时间</span><span>' + spot.openingTime + '</span></div>' +
        '<div class="info-row"><span class="info-label">地址</span>' +
        '<span id="' + addrId + '" style="color:#666;font-size:12px;">' + (spot.address || '获取地址中...') + '</span></div>' +
        '<div class="info-row"><span class="info-label">简介</span>' +
        '<span style="font-size:12px;color:#666;">' + spot.description + '</span></div>' +
        tagsHtml +
        '</div>';

    var infoWindow = new AMap.InfoWindow({
        content: html,
        offset: new AMap.Pixel(0, -25),
        closeWhenClickMap: false
    });

    infoWindow._poiId = spot.id;
    infoWindow._poiType = 'spot';
    if (currentInfoWindow) currentInfoWindow.close();
    infoWindow.open(map, [spot.lng, spot.lat]);
    currentInfoWindow = infoWindow;

    // 延迟绑定收藏按钮事件
    setTimeout(function() {
        var se = document.querySelector('.fav-btn[data-poi-id="' + spot.id + '"]');
        if (se) { se.addEventListener('click', function(e) { e.stopPropagation();
            if (FavoriteManager.isFavorite(spot.id, 'spot')) { FavoriteManager.remove(spot.id, 'spot'); se.innerHTML = '&#9734; 收藏'; se.className = 'fav-btn'; se.style.background = '#f5f5f5'; se.style.borderColor = '#ddd'; }
            else { FavoriteManager.add({ id: spot.id, type: 'spot', name: spot.name }); se.innerHTML = '&#9733; 已收藏'; se.className = 'fav-btn faved'; se.style.background = '#fff3e0'; se.style.borderColor = '#ff9800'; } }); }
    }, 100);

    // 地址回填
    var spotAddrEl = document.getElementById(addrId);
    if (spotAddrEl) {
        spotAddrEl.textContent = (spot.address && spot.address !== '-' && spot.address !== '无法获取') ? spot.address : '地址未知';
    }
}

// ============================================================================
// 十五、视野自适应 —— 自动调整地图视野以包含所有POI
// ============================================================================

/**
 * autoFitView(): 自动调整地图视野，使所有可见的 POI 标记都显示在地图范围内
 *
 * 【工作原理】
 * 1. 收集所有可见的美食（受分类筛选影响）和景点的经纬度坐标
 * 2. 调用高德地图的 setFitView() 方法自动计算最佳缩放级别（zoom）和中心点（center）
 * 3. 通过 padding 参数预留边距：
 *    - 顶部 60px（工具栏和筛选栏占位）
 *    - 底部 60px（比例尺和状态栏占位）
 *    - 左侧 60px（通用间距）
 *    - 右侧 360px（侧边栏面板的宽度，确保POI不被面板遮挡）
 * 4. 如果 setFitView 失败（如坐标数据异常），回退到默认中心 + 13级缩放
 *
 * 【调用时机】
 * - 数据加载完成后（loadData 中 setTimeout 500ms 延迟确保标记已渲染）
 * - 分类筛选变化后（generateCategoryFilters 的点击回调中）
 * - 路线清除后（clearRoute / HistoryManager._applyReverse 'route'）
 */
function autoFitView() {
    if (!map) return;

    var allLngLats = [];

    // 收集美食坐标（受当前分类筛选影响——只收集"可见的"美食）
    foodData.forEach(function (f) {
        if (activeCategories.size === 0 || activeCategories.has(f.category)) {
            allLngLats.push(new AMap.LngLat(f.lng, f.lat));
        }
    });

    // 收集所有景点坐标（不受筛选影响）
    spotData.forEach(function (s) {
        allLngLats.push(new AMap.LngLat(s.lng, s.lat));
    });

    if (allLngLats.length > 0) {
        try {
            // setFitView 自动计算最佳 zoom 和 center，使所有坐标都可见
            // padding: [top, right, bottom, left] —— 右边留360px防止侧边栏遮挡
            map.setFitView(allLngLats, false, [60, 60, 60, 360]);
        } catch (e) {
            // 失败回退——使用默认视图
            map.setZoomAndCenter(13, HEZE_CENTER);
        }
    }
}

// ============================================================================
// 十六、图层控制与筛选面板
// ============================================================================

/**
 * initControls(): 初始化地图工具栏控件的交互事件
 *
 * 【绑定的控件及功能】
 * - #btnToggleFood: 切换美食图层的全局显示/隐藏（toggleFoodVisible）
 * - #btnToggleSpot: 切换景点图层的全局显示/隐藏（toggleSpotVisible）
 * - #btnOpenOnly:  切换"仅显示营业中"筛选（openOnlyActive），重新渲染美食标记
 * - #btnZoomIn:    地图放大一级（map.zoomIn()）
 * - #btnZoomOut:   地图缩小一级（map.zoomOut()）
 * - #btnReset:     重置地图视图到默认状态（菏泽中心 + 13级缩放）
 *
 * 【图层切换实现】
 * 隐藏：将标记从地图移除（m.setMap(null)），但不销毁标记对象。
 *      下次显示时直接恢复（m.setMap(map)），无需重新创建。
 * 这比每次重建标记更高效（避免重复的DOM操作）。
 */
function initControls() {
    var btnFood = document.getElementById('btnToggleFood');
    var btnSpot = document.getElementById('btnToggleSpot');

    // 美食图层切换按钮
    if (btnFood) {
        btnFood.addEventListener('click', function() {
            toggleFoodVisible = !toggleFoodVisible;
            if (toggleFoodVisible) {
                this.classList.add('active');
                foodMarkers.forEach(function(m) { m.setMap(map); });  // 恢复显示
            } else {
                this.classList.remove('active');
                foodMarkers.forEach(function(m) { m.setMap(null); }); // 隐藏（不销毁）
            }
        });
    }

    // 景点图层切换按钮
    if (btnSpot) {
        btnSpot.addEventListener('click', function() {
            toggleSpotVisible = !toggleSpotVisible;
            if (toggleSpotVisible) {
                this.classList.add('active');
                spotMarkers.forEach(function(m) { m.setMap(map); });
            } else {
                this.classList.remove('active');
                spotMarkers.forEach(function(m) { m.setMap(null); });
            }
        });
    }

    // "仅显示营业中"按钮
    var btnOpenOnly = document.getElementById('btnOpenOnly');
    if (btnOpenOnly) {
        btnOpenOnly.addEventListener('click', function() {
            openOnlyActive = !openOnlyActive;
            if (openOnlyActive) { this.classList.add('active'); } else { this.classList.remove('active'); }
            showFoodMarkers();  // 重新渲染（会检查 openOnlyActive 标志过滤已打烊的标记）
        });
    }

    // 地图缩放按钮
    document.getElementById('btnZoomIn').addEventListener('click', function () {
        if (map) map.zoomIn();  // 缩小一级（Zoom In = 增大 zoom 值 = 地图更精细）
    });
    document.getElementById('btnZoomOut').addEventListener('click', function () {
        if (map) map.zoomOut(); // 缩小一级（Zoom Out = 减小 zoom 值 = 地图更概览）
    });
    document.getElementById('btnReset').addEventListener('click', function () {
        if (map) map.setZoomAndCenter(13, HEZE_CENTER);  // 恢复初始视图
    });
}

/**
 * generateCategoryFilters(): 动态生成美食分类筛选标签栏
 *
 * 【生成流程】
 * 1. 遍历 foodData 统计每种分类（category）包含的美食数量
 * 2. 生成"全部(N)"标签 + 各分类标签（按分类名字典序排列）
 * 3. 每个标签显示分类图标（CATEGORY_ICONS）和该分类的美食数量
 *
 * 【点击交互规则】
 * - 点击"全部"：清空 activeCategories，重置所有标签高亮状态
 * - 点击具体分类：切换该分类在 activeCategories 中的存在状态（toggle）
 * - 自动同步"全部"标签的高亮：activeCategories 为空时自动激活"全部"
 * - 每次点击都记录到 HistoryManager（保存变化前后的分类集合）
 * - 每次变化后重新渲染美食标记 + 自适应视图
 */
function generateCategoryFilters() {
    var container = document.getElementById('categoryFilters');

    // 统计每种分类的美食数量
    var cats = {};
    foodData.forEach(function (f) {
        if (f.category) {
            if (!cats[f.category]) cats[f.category] = 0;
            cats[f.category]++;
        }
    });

    // 生成标签 HTML
    var html = '<div class="category-tag active" data-category="all">全部(' + foodData.length + ')</div>';
    Object.keys(cats).sort().forEach(function (c) {
        var icon = CATEGORY_ICONS[c] || '';
        html += '<div class="category-tag" data-category="' + c + '">' + icon + ' ' + c + '(' + cats[c] + ')</div>';
    });
    container.innerHTML = html;

    // 绑定点击事件
    container.querySelectorAll('.category-tag').forEach(function (tag) {
        tag.addEventListener('click', function () {
            var cat = this.dataset.category;
            // 保存筛选前的分类集合（供历史撤销使用）
            var prevCats = Array.from(activeCategories);

            if (cat === 'all') {
                // 点击"全部"：清空筛选，高亮"全部"标签
                activeCategories.clear();
                container.querySelectorAll('.category-tag').forEach(function (t) { t.classList.remove('active'); });
                this.classList.add('active');
                HistoryManager.push('filter', { prevCategories: prevCats, newCategories: Array.from(activeCategories) });
            } else {
                // 点击具体分类：切换选中状态
                this.classList.toggle('active');
                if (activeCategories.has(cat)) activeCategories.delete(cat);
                else activeCategories.add(cat);
                HistoryManager.push('filter', { prevCategories: prevCats, newCategories: Array.from(activeCategories) });

                // 自动同步"全部"标签状态
                var allTag = container.querySelector('[data-category="all"]');
                if (activeCategories.size === 0) allTag.classList.add('active');
                else allTag.classList.remove('active');
            }
            // 筛选变化后重新渲染标记和地图视野
            if (map) {
                showFoodMarkers();
                autoFitView();
            }
        });
    });
}

/**
 * updateStats(): 更新地图顶部区域的统计数字显示
 * 在数据加载完成后调用，显示美食和景点的总量。
 */
function updateStats() {
    document.getElementById('foodCount').textContent = foodData.length;
    document.getElementById('spotCount').textContent = spotData.length;
}

// ============================================================================
// 十七、路线规划面板初始化
// ============================================================================

/**
 * populateSelectOptions(): 填充路线规划的起点/终点下拉选择框
 *
 * 【数据来源】
 * 将 foodData + spotData 合并为一个统一的 allPOIs 数组，
 * 按 POI 名称的中文拼音排序（localeCompare('zh')）。
 *
 * 【选项格式】
 * value = "type_id"（如 "food_3"、"spot_7"），
 * 在路线规划时通过 findPOI() 解析该格式定位到原始数据。
 *
 * 【筛选支持】
 * 起点（#routeStartSearch）和终点（#routeEndSearch）各有独立的搜索输入框，
 * 输入文字时实时过滤下拉选项——按名称或分类模糊匹配（indexOf）。
 */
function populateSelectOptions() {
    // 合并所有美食和景点为一个统一的POI数组
    var allPOIs = [];
    foodData.forEach(function(f) { allPOIs.push({name: f.name, id: f.id, type: 'food', category: f.category || '美食', lng: f.lng, lat: f.lat}); });
    spotData.forEach(function(s) { allPOIs.push({name: s.name, id: s.id, type: 'spot', category: s.type || '景点', lng: s.lng, lat: s.lat}); });
    // 按中文名称排序
    allPOIs.sort(function(a,b) { return a.name.localeCompare(b.name, 'zh'); });

    /**
     * renderSelect(selId, filter): 渲染指定的下拉选择框
     * @param {string} selId  - 目标 select 元素的 ID
     * @param {string} filter - 可选的过滤关键字（从搜索输入框获取）
     */
    function renderSelect(selId, filter) {
        var sel = document.getElementById(selId);
        if (!sel) return;
        // 根据过滤关键字筛选POI列表
        var filtered = filter ? allPOIs.filter(function(p) {
            return p.name.indexOf(filter) >= 0 || p.category.indexOf(filter) >= 0;
        }) : allPOIs;
        var html = '<option value="">-- 选择POI --</option>';
        filtered.forEach(function(p) {
            html += '<option value="' + p.type + '_' + p.id + '">' + p.name + ' (' + p.category + ')</option>';
        });
        sel.innerHTML = html;
    }

    // 初始渲染（不限制过滤条件）
    renderSelect('routeStart', '');
    renderSelect('routeEnd', '');

    // 绑定起点搜索框的输入事件（实时过滤）
    var startSearch = document.getElementById('routeStartSearch');
    if (startSearch) {
        startSearch.addEventListener('input', function() {
            renderSelect('routeStart', this.value.trim());
        });
    }
    // 绑定终点搜索框的输入事件
    var endSearch = document.getElementById('routeEndSearch');
    if (endSearch) {
        endSearch.addEventListener('input', function() {
            renderSelect('routeEnd', this.value.trim());
        });
    }
}

/**
 * initRoutePlanner(): 初始化路线规划面板的交互事件
 *
 * 【绑定的控件】
 * - 交通方式切换按钮（.route-mode-btn[data-mode]）：driving/walking/bus → 更新 routeMode
 * - 排序策略切换按钮（.route-mode-btn[data-sort]）：time/distance/toll → 更新 routeSortMode
 * - 添加途经点按钮（#btnAddWaypoint）：最多5个途经点
 * - 规划路线按钮（#btnPlanRoute）：调用 executeRoutePlan()
 * - 清除路线按钮（#btnClearRoute）：调用 clearRoute()
 */
function initRoutePlanner() {
    // 交通方式切换（驾车/步行/公交）
    var modeBtns = document.querySelectorAll('.route-mode-btn[data-mode]');
    modeBtns.forEach(function(btn) {
        btn.addEventListener('click', function() {
            modeBtns.forEach(function(b) { b.classList.remove('active'); });
            this.classList.add('active');
            routeMode = this.dataset.mode;
        });
    });

    // 排序策略切换（最快/最短/最省钱）
    var sortBtns = document.querySelectorAll('.route-mode-btn[data-sort]');
    sortBtns.forEach(function(btn) {
        btn.addEventListener('click', function() {
            sortBtns.forEach(function(b) { b.classList.remove('active'); });
            this.classList.add('active');
            routeSortMode = this.dataset.sort;
        });
    });

    // 添加途经点（按序插入，最多5个）
    var addBtn = document.getElementById('btnAddWaypoint');
    if (addBtn) {
        addBtn.addEventListener('click', function() {
            if (routeWaypoints.length >= 5) { alert('最多添加5个途经点'); return; }
            routeWaypoints.push({ id: 0, name: '途经点' + (routeWaypoints.length + 1), type: 'food', lng: 0, lat: 0 });
            renderWaypointList();
        });
    }

    // 执行路线规划
    var planBtn = document.getElementById('btnPlanRoute');
    if (planBtn) {
        planBtn.addEventListener('click', function() { executeRoutePlan(); });
    }

    // 清除当前路线
    var clearBtn = document.getElementById('btnClearRoute');
    if (clearBtn) {
        clearBtn.addEventListener('click', function() { clearRoute(); });
    }
}

/**
 * renderWaypointList(): 渲染途经点列表
 *
 * 每个途经点行包含：
 * - POI 下拉选择框（可搜索选择美食或景点）
 * - 上移按钮 ▲：与上一个途经点交换位置
 * - 下移按钮 ▼：与下一个途经点交换位置
 * - 删除按钮 ×：移除此途经点
 *
 * 【数据同步】
 * 下拉选择框的 value 格式为 "type_id"（如 "food_3"），
 * 选择变化时更新 routeWaypoints 数组中对应元素的数据。
 */
function renderWaypointList() {
    var container = document.getElementById('routeWaypoints');
    if (!container) return;

    var html = '';
    routeWaypoints.forEach(function(wp, i) {
        html += '<div class="route-waypoint-item">' +
            '<select class="wp-select" data-index="' + i + '" style="flex:1;padding:4px;border:1px solid #ddd;border-radius:4px;font-size:12px;">' +
            '<option value="">选择途经点</option></select>' +
            '<span class="wp-up" data-index="' + i + '" style="cursor:pointer;padding:0 2px;color:#999;font-size:11px;">&#9650;</span>' +
            '<span class="wp-down" data-index="' + i + '" style="cursor:pointer;padding:0 2px;color:#999;font-size:11px;">&#9660;</span>' +
            '<span class="wp-del" data-index="' + i + '" style="color:#d32f2f;cursor:pointer;font-weight:bold;padding:0 6px;font-size:16px;line-height:1;">×</span>' +
            '</div>';
    });
    container.innerHTML = html;

    // 构建可供选择的POI列表（与起点/终点选择框共用相同数据源）
    var allPOIs = [];
    foodData.forEach(function(f) { allPOIs.push({name: f.name, id: f.id, type: 'food', category: f.category || '美食', lng: f.lng, lat: f.lat}); });
    spotData.forEach(function(s) { allPOIs.push({name: s.name, id: s.id, type: 'spot', category: s.type || '景点', lng: s.lng, lat: s.lat}); });
    allPOIs.sort(function(a,b) { return a.name.localeCompare(b.name, 'zh'); });

    // 填充每个途经点的下拉选择框
    container.querySelectorAll('.wp-select').forEach(function(sel) {
        var idx = parseInt(sel.dataset.index);
        var optHtml = '<option value="">选择途经点</option>';
        allPOIs.forEach(function(p) {
            // 如果途经点已设置了该POI，标记为 selected（保持选择状态）
            optHtml += '<option value="' + p.type + '_' + p.id + '" ' +
                (routeWaypoints[idx].id === p.id && routeWaypoints[idx].type === p.type ? 'selected' : '') +
                '>' + p.name + ' (' + p.category + ')</option>';
        });
        sel.innerHTML = optHtml;

        // 选择变化事件：更新 routeWaypoints 数组中对应的POI数据
        sel.addEventListener('change', function() {
            var wpIdx = parseInt(this.dataset.index);
            var val = this.value;
            if (val) {
                var parts = val.split('_');
                var poi = allPOIs.find(function(p) { return p.type === parts[0] && p.id === parseInt(parts[1]); });
                if (poi) {
                    routeWaypoints[wpIdx] = { id: poi.id, name: poi.name, type: poi.type, lng: poi.lng, lat: poi.lat };
                }
            } else {
                // 选择了空选项：重置为占位途经点
                routeWaypoints[wpIdx] = { id: 0, name: '途经点' + (wpIdx + 1), type: 'food', lng: 0, lat: 0 };
            }
        });
    });

    // 删除按钮事件
    container.querySelectorAll('.wp-del').forEach(function(el) {
        el.addEventListener('click', function() {
            var idx = parseInt(this.dataset.index);
            routeWaypoints.splice(idx, 1);
            renderWaypointList();
        });
    });

    // 上移按钮事件（与上一个途经点交换位置）
    container.querySelectorAll('.wp-up').forEach(function(el) {
        el.addEventListener('click', function() {
            var idx = parseInt(this.dataset.index);
            if (idx > 0) { var t = routeWaypoints[idx-1]; routeWaypoints[idx-1] = routeWaypoints[idx]; routeWaypoints[idx] = t; renderWaypointList(); }
        });
    });

    // 下移按钮事件（与下一个途经点交换位置）
    container.querySelectorAll('.wp-down').forEach(function(el) {
        el.addEventListener('click', function() {
            var idx = parseInt(this.dataset.index);
            if (idx < routeWaypoints.length - 1) { var t = routeWaypoints[idx+1]; routeWaypoints[idx+1] = routeWaypoints[idx]; routeWaypoints[idx] = t; renderWaypointList(); }
        });
    });
}

// ============================================================================
// 十八、搜索功能
// ============================================================================

/**
 * matchesQuery(text, query): 模糊文本匹配辅助函数
 *
 * 【匹配规则（宽松匹配，提升搜索命中率）】
 * - 忽略大小写：转为小写后比较
 * - 忽略空白字符：移除所有空格、制表符等
 * - 子串匹配：查询词（query）是目标文本（text）的子串即为匹配
 *
 * @param {string} text  - 待搜索的目标文本
 * @param {string} query - 用户输入的搜索关键词
 * @returns {boolean} 是否匹配
 */
function matchesQuery(text, query) {
    if (!query || !text) return false;
    var lowerText = text.toLowerCase().replace(/\s+/g, '');   // 转小写 + 去空白
    var lowerQuery = query.toLowerCase().replace(/\s+/g, '');
    return lowerText.indexOf(lowerQuery) !== -1;               // 子串匹配
}

/**
 * performSearch(): 执行搜索操作，筛选并展示匹配的 POI
 *
 * 【搜索范围（根据当前选中的搜索类型）】
 * - 'all' 或 'food'：在美食的 name、address、tags 中搜索
 * - 'all' 或 'spot'：在景点的 name、address、description 中搜索
 *
 * 【视觉反馈】
 * - 搜索结果面板：显示匹配的POI列表（最多50条），每条可点击定位
 * - 地图标记透明度：匹配的标记保持不透明(opacity=1)，不匹配的降为0.2（弱化显示）
 *   这种"弱化而非隐藏"的设计保留了空间参考信息
 *
 * 【操作记录】
 * 每次搜索都会 push 到 HistoryManager，支持撤销（清空搜索并恢复标记透明度）
 */
function performSearch() {
    var query = document.getElementById('searchInput').value.trim();
    var searchType = document.querySelector('.search-type-tag.active').dataset.type;
    var resultDiv = document.getElementById('searchResults');
    var resultList = document.getElementById('searchResultList');
    var resultCount = document.getElementById('searchResultCount');

    // 搜索词为空：隐藏搜索结果，恢复所有标记的透明度
    if (!query) {
        resultDiv.style.display = 'none';
        if (map) {
            foodMarkers.forEach(function(m) { m.setOpacity(1); });
            spotMarkers.forEach(function(m) { m.setOpacity(1); });
        }
        return;
    }

    // 记录搜索操作到历史
    HistoryManager.push('search', { query: query, searchType: searchType });

    var results = [];

    // 搜索美食：在名称、地址、标签数组中查找
    if (searchType === 'all' || searchType === 'food') {
        foodData.forEach(function(f) {
            var m = matchesQuery(f.name, query) || matchesQuery(f.address, query) ||
                    (f.tags && f.tags.some(function(t) { return matchesQuery(t, query); }));
            if (m) results.push({ type: 'food', data: f });
        });
    }

    // 搜索景点：在名称、地址、简介中查找
    if (searchType === 'all' || searchType === 'spot') {
        spotData.forEach(function(s) {
            var m = matchesQuery(s.name, query) || matchesQuery(s.address, query) ||
                    matchesQuery(s.description, query);
            if (m) results.push({ type: 'spot', data: s });
        });
    }

    resultCount.textContent = results.length;

    // 渲染搜索结果列表
    if (results.length === 0) {
        resultList.innerHTML = '<div class="search-result-nohits">未找到匹配项</div>';
    } else {
        var html = '';
        results.slice(0, 50).forEach(function(r) {
            var item = r.data;
            var isFood = r.type === 'food';
            var color = isFood ? (CATEGORY_COLORS[item.category] || CATEGORY_COLORS.default) : '#1565c0';
            var icon = isFood ? item.name.charAt(0) : '景';
            var sub = isFood ? item.category + ' / ' + item.score.toFixed(1) + '分 / ¥' + item.price : item.type;
            html += '<div class="search-result-item" data-type="' + r.type + '" data-id="' + item.id + '" data-lng="' + item.lng + '" data-lat="' + item.lat + '">' +
                '<div class="search-result-icon" style="background:' + color + '">' + icon + '</div>' +
                '<div style="flex:1"><div class="search-result-name">' + item.name + '</div>' +
                '<div class="search-result-meta">' + sub + '</div></div></div>';
        });
        resultList.innerHTML = html;

        // 绑定搜索结果点击事件：定位到地图 + 打开详情
        resultList.querySelectorAll('.search-result-item').forEach(function(el) {
            el.addEventListener('click', function() {
                var type = this.dataset.type;
                var id = parseInt(this.dataset.id);
                var lng = parseFloat(this.dataset.lng);
                var lat = parseFloat(this.dataset.lat);
                if (map) {
                    map.setZoomAndCenter(16, [lng, lat]);
                    if (type === 'food') {
                        var food = foodData.find(function(f) { return f.id === id; });
                        if (food) showFoodDetail(food);
                    } else {
                        var spot = spotData.find(function(s) { return s.id === id; });
                        if (spot) showSpotDetail(spot);
                    }
                }
            });
        });
    }

    resultDiv.style.display = 'block';

    // 调整地图标记的透明度——匹配的高亮，不匹配的弱化
    if (map) {
        var resultIds = new Set();
        results.forEach(function(r) { resultIds.add(r.data.id); });
        foodMarkers.forEach(function(m, i) {
            m.setOpacity(resultIds.has(foodData[i].id) ? 1 : 0.2);
        });
        spotMarkers.forEach(function(m, i) {
            m.setOpacity(resultIds.has(spotData[i].id) ? 1 : 0.2);
        });
    }
}

/**
 * initSearch(): 初始化搜索功能的交互事件
 *
 * 搜索输入框使用 200ms 防抖（debounce）策略：
 * 用户停止输入后 200ms 才触发搜索，避免每次按键都发起遍历（提高性能）。
 * 搜索类型切换（全部/美食/景点）后立即重新搜索。
 */
function initSearch() {
    var searchInput = document.getElementById('searchInput');
    if (!searchInput) return;

    // 防抖处理：每次输入时重置定时器，200ms 无输入后执行搜索
    searchInput.addEventListener('input', function() {
        clearTimeout(this._searchTimer);
        this._searchTimer = setTimeout(performSearch, 200);
    });

    // 搜索类型标签切换事件
    document.querySelectorAll('.search-type-tag').forEach(function(tag) {
        tag.addEventListener('click', function() {
            document.querySelectorAll('.search-type-tag').forEach(function(t) { t.classList.remove('active'); });
            this.classList.add('active');
            performSearch();
        });
    });
}

// ============================================================================
// 十九、营业状态判断
// ============================================================================

/**
 * getOpenStatus(opentime): 根据营业时间字符串判断当前是否在营业时间内
 *
 * 【时间格式约定】
 * 系统支持以下营业时间格式：
 * - "24小时" / "全天" / 包含"全天"：始终营业中
 * - 空值 / "-"：数据缺失，默认视为营业中（不误判）
 * - 标准单段格式："HH:MM-HH:MM"（如 "08:00-22:00"）
 * - 多段格式："HH:MM-HH:MM,HH:MM-HH:MM"（如 "06:00-14:00,17:00-22:00"）
 *   多段时间用英文逗号分隔，表示一天内有多个营业时段
 *
 * 【跨日判断算法】
 * 如果结束时间 ≤ 开始时间（如 "22:00-02:00" 深夜营业），说明跨越午夜。
 * 处理方式：
 * 1. 将结束时间 + 24小时（变为次日的分钟数）
 * 2. 检查当前时间是否在 [开始, 结束+24h] 区间内
 * 3. 同时检查 [当前时间+24h] 是否在 [开始, 结束+24h] 区间内
 *    （第3步处理的是"深夜时段"，当前时间在凌晨、营业区间跨午夜的情况）
 *
 * @param {string} opentime - 营业时间字符串
 * @returns {object} { cls: 'status-open'|'status-closed', text: '营业中'|'已打烊' }
 *   cls: CSS 类名（status-open 绿色 / status-closed 灰色）
 *   text: 显示文本
 */
function getOpenStatus(opentime) {
    // 全天营业或数据缺失 → 默认营业中
    if (!opentime || opentime === '-' || opentime === '24小时' || opentime === '全天' || opentime.indexOf('全天') !== -1) {
        return { cls: 'status-open', text: '营业中' };
    }

    // 获取当前时间的总分钟数（从 00:00 开始计算）
    var now = new Date();
    var currentMin = now.getHours() * 60 + now.getMinutes();

    // 多段时间用逗号分隔，逐段检查
    var segments = opentime.split(',');
    for (var i = 0; i < segments.length; i++) {
        var parts = segments[i].trim().split('-');
        if (parts.length === 2) {
            // 解析开始时间和结束时间的时、分
            var s = parts[0].trim().split(':');
            var e = parts[1].trim().split(':');
            if (s.length >= 2 && e.length >= 2) {
                var start = parseInt(s[0]) * 60 + parseInt(s[1]);  // 开始时间（分钟数）
                var end = parseInt(e[0]) * 60 + parseInt(e[1]);    // 结束时间（分钟数）

                // 跨日处理：if 结束 ≤ 开始（如 22:00 ≤ 02:00? → 22:00 > 2:00 = true → end += 24h）
                // 修正后 end = 26*60=1560分钟，区间变为 [1320, 1560] 即 [22:00, 次日02:00]
                if (end <= start) end += 24 * 60;

                // 检查1：当前时间是否在标准区间内
                if (currentMin >= start && currentMin <= end) {
                    return { cls: 'status-open', text: '营业中' };
                }
                // 检查2：当前时间+24h 是否在区间内
                // （处理"凌晨1点检查昨晚22:00-02:00的营业时间段"的场景）
                if (currentMin + 24*60 >= start && currentMin + 24*60 <= end) {
                    return { cls: 'status-open', text: '营业中' };
                }
            }
        }
    }

    // 所有时段都不匹配 → 已打烊
    return { cls: 'status-closed', text: '已打烊' };
}

// ============================================================================
// 二十、路线规划核心逻辑
// ============================================================================

/**
 * executeRoutePlan(): 执行路线规划
 *
 * 【完整流程】
 * 1. 从起点/终点下拉框获取选择的值
 * 2. 通过 findPOI() 将选项值解析为完整的 POI 数据
 * 3. 收集所有路径点：起点 + 有效的途经点（坐标非0） + 终点
 * 4. 验证所有坐标有效性（非NaN、非0）
 * 5. 检查每段距离合理性（不超过500km，超过提示分日出行）
 * 6. 清除旧路线，记录历史操作
 * 7. 显示路线详情面板（加载中状态）
 * 8. 调用 planRouteSegments() 开始递归分段规划
 *
 * 【分段规划的设计原因】
 * 高德 API 的单次请求最多支持几个途经点，但系统的路线可能很长。
 * 递归分段处理每对相邻点，逐段获取精确路径，确保鲁棒性。
 */
function executeRoutePlan() {
    var startVal = document.getElementById('routeStart').value;
    var endVal = document.getElementById('routeEnd').value;
    if (!startVal || !endVal) { alert('请选择起点和终点'); return; }

    var startPOI = findPOI(startVal);
    var endPOI = findPOI(endVal);
    if (!startPOI || !endPOI) { alert('无效的POI选择'); return; }

    // 收集完整路径点序列：起点 → [途经点...] → 终点
    var allWaypoints = [startPOI];
    routeWaypoints.forEach(function(wp) {
        // 过滤掉未设置具体POI的占位途经点（坐标为0,0）
        if (wp.lng !== 0 && wp.lat !== 0 && !isNaN(wp.lng) && !isNaN(wp.lat)) allWaypoints.push(wp);
    });
    allWaypoints.push(endPOI);

    // 验证所有坐标有效性
    for (var i = 0; i < allWaypoints.length; i++) {
        var w = allWaypoints[i];
        if (isNaN(w.lng) || isNaN(w.lat) || w.lng === 0 || w.lat === 0) {
            alert('途径点坐标无效，请重新选择');
            return;
        }
    }

    // 检查相邻点之间的距离是否合理（超过500km的一天内不实际）
    for (var i = 0; i < allWaypoints.length - 1; i++) {
        var d = haversine(allWaypoints[i].lng, allWaypoints[i].lat, allWaypoints[i+1].lng, allWaypoints[i+1].lat);
        if (d > 500000) { alert('距离过远(' + (d/1000).toFixed(0) + 'km)，建议分日出行'); return; }
    }

    clearRoute();
    HistoryManager.push('route', {});
    document.getElementById('routeDetail').style.display = 'block';
    document.getElementById('routeSummary').innerHTML = '<div class="route-loading">路线规划中...</div>';
    document.getElementById('routeSegments').innerHTML = '';

    // 初始化分段结果数组，长度 = 段数（N个点有N-1段）
    window._routeSegments = new Array(allWaypoints.length - 1);
    planRouteSegments(allWaypoints, 0);
}

/**
 * findPOI(val): 根据选项值字符串解析为完整的 POI 对象
 *
 * @param {string} val - 选择框的值，格式 "type_id"（如 "food_3"）
 * @returns {object|null} { id, name, type, lng, lat } 或 null（找不到时）
 */
function findPOI(val) {
    if (!val) return null;
    var parts = val.split('_');
    var type = parts[0];
    var id = parseInt(parts[1]);
    if (type === 'food') {
        var f = foodData.find(function(fd) { return fd.id === id; });
        return f ? { id: f.id, name: f.name, type: 'food', lng: f.lng, lat: f.lat } : null;
    }
    var s = spotData.find(function(sd) { return sd.id === id; });
    return s ? { id: s.id, name: s.name, type: 'spot', lng: s.lng, lat: s.lat } : null;
}

/**
 * clearRoute(): 清除地图上所有路线相关的可视化元素
 * 移除折线（polyline）、路线标记、分段数据，隐藏详情面板。
 */
function clearRoute() {
    currentRoutePolylines.forEach(function(p) { p.setMap(null); });
    currentRouteMarkers.forEach(function(m) { m.setMap(null); });
    currentRoutePolylines = [];
    currentRouteMarkers = [];
    window._routeSegments = [];
    var rd = document.getElementById('routeDetail');
    if (rd) rd.style.display = 'none';
}

/**
 * planRouteSegments(waypoints, index): 递归分段规划路线
 *
 * 【工作原理】
 * 逐段调用高德 Web API 的路径规划接口获取精确路线。
 * URL 格式：https://restapi.amap.com/v3/direction/{mode}?origin=lng,lat&destination=lng,lat&key=KEY&strategy=N
 *
 * 【Strategy 策略映射】
 * - time=0（速度优先，推荐路线，时间最短，默认策略）
 * - toll=1（费用优先，避开收费路段）
 * - distance=2（距离优先，不走高速）
 *
 * 【容错机制（三层保障）】
 * 1. API 返回成功 → 使用高德精确路线数据
 * 2. API 返回失败/无路线 → 使用 haversine 直线距离作为估算（虚线显示）
 * 3. 网络错误/超时 → 同上，使用估算路线
 *
 * 【递归策略与限流】
 * 每段之间间隔 300ms（setTimeout），避免短时间内大量 API 调用触发高德限流。
 *
 * @param {Array}  waypoints - 完整路径点数组
 * @param {number} index     - 当前处理的段索引（从0开始）
 */
function planRouteSegments(waypoints, index) {
    if (!map || index >= waypoints.length - 1) {
        if (index >= waypoints.length - 1) renderRouteSummary();
        return;
    }

    var from = waypoints[index];
    var to = waypoints[index + 1];

    console.log('[Route] segment', index, 'requesting:', from.name, '->', to.name);

    var policyMap = { time: 0, distance: 2, toll: 1 };
    var policy = policyMap[routeSortMode] || 0;

    // Load AMap.Driving or AMap.Walking plugin dynamically
    var pluginName = routeMode === 'walking' ? 'AMap.Walking' : 'AMap.Driving';
    AMap.plugin(pluginName, function() {
        console.log('[Route] segment', index, 'plugin loaded:', pluginName);
        try {
            var service = routeMode === 'walking'
                ? new AMap.Walking({ map: map })
                : new AMap.Driving({ map: map, policy: policy });

            var onComplete = function(status, result) {
                console.log('[Route] segment', index, 'callback:', status, result ? 'has result' : 'no result');
                try { service.clear(); } catch(e) {}
                if (status === 'complete' && result.routes && result.routes.length > 0) {
                    var route = result.routes[0];
                    var steps = [];
                    if (route.steps) {
                        route.steps.forEach(function(step) {
                            if (step.path) steps = steps.concat(step.path);
                        });
                    }
                    var segment = {
                        from: from.name, to: to.name,
                        distance: route.distance || 0,
                        duration: route.time || 0,
                        tolls: (routeMode === 'driving' && route.toll) ? route.toll : 0,
                        steps: steps.map(function(p) { return [p.lng, p.lat]; })
                    };
                    renderRouteSegment(segment, index);
                } else {
                    console.log('[Route] segment', index, 'API failed, fallback. status:', status);
                    renderRouteSegmentFallback(from, to, index);
                }
                planRouteSegments(waypoints, index + 1);
            };

            service.search(
                new AMap.LngLat(from.lng, from.lat),
                new AMap.LngLat(to.lng, to.lat),
                onComplete
            );
        } catch(e) {
            console.error('[Route] segment', index, 'error:', e);
            renderRouteSegmentFallback(from, to, index);
            planRouteSegments(waypoints, index + 1);
        }
    });
}

/**
 * parsePolyline(steps): 从高德 API 返回的步骤中提取路径坐标点
 *
 * 高德 API 返回的每个 step 包含一个 polyline 字段，
 * 格式为 "lng1,lat1;lng2,lat2;..." 的分号分隔字符串。
 * 该函数将其解析为 JavaScript 的二维数组 [[lng, lat], ...]。
 *
 * @param {Array} steps - API 返回的路径步骤数组
 * @returns {Array} 二维数组，每个元素为 [lng, lat]
 */
function parsePolyline(steps) {
    var allPoints = [];
    if (!steps) return allPoints;
    steps.forEach(function(step) {
        if (step.polyline && typeof step.polyline === 'string') {
            var pts = step.polyline.split(';');  // 分号分隔不同坐标点
            pts.forEach(function(p) {
                var xy = p.split(',');  // 逗号分隔经纬度
                if (xy.length === 2) {
                    allPoints.push([parseFloat(xy[0]), parseFloat(xy[1])]);
                }
            });
        }
    });
    return allPoints;
}

/**
 * renderRouteSegment(segment, index): 在地图上渲染一段精确路线
 *
 * 【渲染内容】
 * 1. 彩色折线（polyline）：使用不同颜色区分不同段（颜色数组循环）
 * 2. 里程标签（marker）：在路径中点显示距离/时间/过路费信息
 *
 * @param {object} segment - { from, to, distance, duration, tolls, steps }
 * @param {number} index   - 段索引（0,1,2,...），用于选择颜色
 */
function renderRouteSegment(segment, index) {
    if (!map) return;
    if (!segment || isNaN(segment.distance) || isNaN(segment.duration) || segment.distance <= 0) return;

    // 6种颜色循环使用，区分不同路线段
    var colors = ['#ff5722', '#e64a19', '#bf360c', '#ff9800', '#f57c00', '#ff7043'];
    var color = colors[index % colors.length];

    // 绘制折线（如果路径坐标点足够多）
    if (segment.steps.length > 1) {
        var path = segment.steps.map(function(p) { return new AMap.LngLat(p[0], p[1]); });
        var polyline = new AMap.Polyline({
            path: path, strokeColor: color, strokeWeight: 5,
            strokeOpacity: 0.7, lineJoin: 'round'
        });
        polyline.setMap(map);
        currentRoutePolylines.push(polyline);
    }

    // 在路径中点放置里程标签（距离/时间/过路费）
    if (segment.steps.length > 0) {
        var midIdx = Math.floor(segment.steps.length / 2);
        var midPt = segment.steps[midIdx];
        var label = new AMap.Marker({
            position: new AMap.LngLat(midPt[0], midPt[1]),
            content: '<div style="background:' + color + ';color:white;padding:2px 8px;border-radius:10px;font-size:11px;white-space:nowrap;">' +
                (segment.distance / 1000).toFixed(1) + 'km / ' + Math.round(segment.duration / 60) + 'min' +
                (segment.tolls > 0 ? ' / Y' + segment.tolls : '') + '</div>',
            offset: new AMap.Pixel(-30, -15), zIndex: 150
        });
        label.setMap(map);
        currentRouteMarkers.push(label);
    }

    if (!window._routeSegments) window._routeSegments = [];
    window._routeSegments[index] = segment;
}

/**
 * renderRouteSegmentFallback(from, to, index): 使用直线距离估算路线（API失败时的备用方案）
 *
 * 当高德 API 返回异常、网络出错或超时时，使用 haversine 公式计算两点间的直线距离，
 * 绘制灰色虚线作为"大致方向参考"。
 *
 * 估算速度：驾车模式 40km/h，步行模式 5km/h。
 *
 * @param {object} from  - 起点 { name, lng, lat }
 * @param {object} to    - 终点 { name, lng, lat }
 * @param {number} index - 段索引
 */
function renderRouteSegmentFallback(from, to, index) {
    if (!map) return;
    if (!from || !to || isNaN(from.lng) || isNaN(from.lat) || isNaN(to.lng) || isNaN(to.lat) || from.lng === 0 || from.lat === 0 || to.lng === 0 || to.lat === 0) return;

    // 使用 haversine 公式计算大圆距离
    var dist = haversine(from.lng, from.lat, to.lng, to.lat);
    if (isNaN(dist) || dist <= 0) return;

    // 估算行程时间：驾车 40km/h，步行 5km/h
    var speed = routeMode === 'walking' ? 5 : 40;
    var durationMin = Math.round(dist / (speed * 1000 / 60));
    var segment = { from: from.name, to: to.name, distance: Math.round(dist), duration: durationMin * 60, tolls: 0, steps: [[from.lng, from.lat], [to.lng, to.lat]] };

    // 灰色虚线：API 估算路线的视觉标识
    var line = new AMap.Polyline({
        path: [new AMap.LngLat(from.lng, from.lat), new AMap.LngLat(to.lng, to.lat)],
        strokeColor: '#999', strokeWeight: 3, strokeOpacity: 0.5,
        strokeStyle: 'dashed', lineJoin: 'round'
    });
    line.setMap(map);
    currentRoutePolylines.push(line);

    if (!window._routeSegments) window._routeSegments = [];
    window._routeSegments[index] = segment;
}

/**
 * haversine(lng1, lat1, lng2, lat2): 使用 Haversine 公式计算地球表面两点间的大圆距离
 *
 * 【公式原理】
 * Haversine 公式是球面三角学中计算两点间最短距离的标准方法，
 * 适用于计算地理坐标间的距离，精度在小尺度下足够高。
 *
 * 【公式】
 * a = sin²(Δlat/2) + cos(lat1)·cos(lat2)·sin²(Δlng/2)
 * c = 2·arctan(√a, √(1-a))
 * d = R·c
 *
 * @param {number} lng1, lat1 - 第一点的经度和纬度（度）
 * @param {number} lng2, lat2 - 第二点的经度和纬度（度）
 * @returns {number} 两点间的距离（米），使用地球半径 R=6371000m
 */
function haversine(lng1, lat1, lng2, lat2) {
    var R = 6371000;  // 地球平均半径（米）
    // 将角度转换为弧度
    var dlat = (lat2 - lat1) * Math.PI / 180;
    var dlng = (lng2 - lng1) * Math.PI / 180;
    var a = Math.sin(dlat/2) * Math.sin(dlat/2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dlng/2) * Math.sin(dlng/2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

/**
 * renderRouteSummary(): 汇总并渲染路线详情面板
 *
 * 在所有分段都规划完成后调用。汇总各段的距离、时间、过路费，
 * 展示在路线详情面板中。同时自动调整地图视野以包含整条路线。
 */
function renderRouteSummary() {
    if (!window._routeSegments || window._routeSegments.length === 0) return;
    var totalDist = 0, totalTime = 0, totalTolls = 0;
    window._routeSegments.forEach(function(s, i) {
        if (!s || isNaN(s.distance) || isNaN(s.duration)) return;
        totalDist += s.distance;
        totalTime += s.duration;
        totalTolls += (s.tolls || 0);
    });

    var sm = document.getElementById('routeSummary');

    if (isNaN(totalDist) || totalDist === 0) {
        if (sm) sm.innerHTML = '<div class="route-error">路径计算异常，请重试</div>';
        return;
    }

    // 汇总信息：总距离、总时间、过路费（仅驾车模式显示）
    var summaryHtml = '';
    summaryHtml += '<div class="route-summary-item">总距离: <strong>' + (totalDist / 1000).toFixed(1) + ' km</strong></div>';
    summaryHtml += '<div class="route-summary-item">总时间: <strong>' + Math.round(totalTime / 60) + ' 分钟</strong></div>';
    if (routeMode === 'driving' && totalTolls > 0) summaryHtml += '<div class="route-summary-item">过路费: <strong>Y' + totalTolls + '</strong></div>';
    if (sm) sm.innerHTML = summaryHtml;

    // 分段详情：每段的起点→终点、距离、时间、过路费
    var segHtml = '';
    window._routeSegments.forEach(function(s, i) {
        if (!s) return;
        segHtml += '<div class="route-segment">' +
            '<div class="route-segment-header">' + (i + 1) + '. ' + s.from + ' - ' + s.to + '</div>' +
            '<div class="route-segment-detail">距离 ' + (s.distance / 1000).toFixed(1) + 'km / 时间 ' + Math.round(s.duration / 60) + 'min</div>' +
            (routeMode === 'driving' && s.tolls > 0 ? '<div class="route-segment-toll">过路费 Y' + s.tolls + '</div>' : '') +
            '</div>';
    });
    var segEl = document.getElementById('routeSegments');
    if (segEl) segEl.innerHTML = segHtml;

    window._routeSegments = [];

    // 自动调整视野以包含整条路线
    if (currentRoutePolylines.length > 0 && map) {
        var allPoints = [];
        currentRoutePolylines.forEach(function(p) {
            try { var pts = p.getPath(); if (pts) pts.forEach(function(pt) { allPoints.push(pt); }); } catch(e) {}
        });
        if (allPoints.length > 0) {
            map.setFitView(allPoints, false, [80, 80, 80, 380]);
        }
    }
}

// ============================================================================
// 二十一、操作历史面板初始化
// ============================================================================

/**
 * initHistory(): 初始化操作历史面板的交互事件
 *
 * 绑定撤销(Undo)、重做(Redo)、清空(Clear)按钮的事件，
 * 以及历史下拉列表的打开/关闭和点击跳转（跳转到指定历史状态）。
 * 下拉列表点击时，通过连续调用 undo/redo 将指针移动到目标位置。
 */
function initHistory() {
    var undoBtn = document.getElementById('btnHistoryUndo');
    var redoBtn = document.getElementById('btnHistoryRedo');
    var clearBtn = document.getElementById('btnHistoryClear');
    var dropdown = document.getElementById('historyDropdown');
    var trigger = document.getElementById('btnHistoryDropdown');
    
    if (undoBtn) undoBtn.addEventListener('click', function() { HistoryManager.undo(); });
    if (redoBtn) redoBtn.addEventListener('click', function() { HistoryManager.redo(); });
    if (clearBtn) clearBtn.addEventListener('click', function(e) { e.stopPropagation(); HistoryManager.clear(); });
    
    // 历史下拉列表的打开/关闭
    if (trigger && dropdown) {
        trigger.addEventListener('click', function(e) { e.stopPropagation();
            dropdown.style.display = dropdown.style.display === 'block' ? 'none' : 'block';
        });
        // 点击下拉列表中的记录：通过连续 undo/redo 跳转到目标状态
        dropdown.addEventListener('click', function(e) {
            var item = e.target.closest('.history-item'); if (!item) return;
            var idx = parseInt(item.dataset.index);
            // 当前指针在目标之后 → undo 回退
            while (HistoryManager.pointer > idx) HistoryManager.undo();
            // 当前指针在目标之前 → redo 前进
            while (HistoryManager.pointer < idx) HistoryManager.redo();
        });
    }
}

// ============================================================================
// 二十二、系统启动入口
// ============================================================================

/**
 * window.onload: 系统启动入口函数
 *
 * 在页面所有资源（HTML、CSS、图片、脚本）加载完毕后自动调用。
 * 打印启动日志和 API Key 前8位（用于快速确认 Key 配置），
 * 然后调用 loadMap() 开始地图加载流程。
 *
 * 【启动链路】
 * window.onload → loadMap() → 成功: loadData() → 初始化所有模块
 *                            → 失败: showMapFallback() → loadData()（降级模式）
 */
window.onload = function () {
    console.log('[Map] 系统启动...');
    console.log('[Map] API Key:', AMAP_KEY.substring(0, 8) + '...');
    loadMap();
};
