/**
 * 菏泽美食推荐与城市漫游规划系统 - 地图可视化模块（高德地图 API v2.0 完整集成）
 *
 * 功能：
 *   - 美食/景点标记展示（分类颜色区分）
 *   - 高德逆地理编码获取真实地址
 *   - 信息窗口（评分星级、价格、地址、标签）
 *   - 路线可视化
 *   - 图层控制、分类筛选
 */

var map = null;
var geocoder = null;
var foodMarkers = [];
var spotMarkers = [];
var foodData = [];
var spotData = [];
var activeCategories = new Set();
var addressCache = {};       // 逆地理编码缓存
var toggleFoodVisible = true;
var toggleSpotVisible = true;
var openOnlyActive = false;
var currentRoutePolylines = [];
var currentRouteMarkers = [];
var routeWaypoints = [];
var routeMode = 'driving';
var routeSortMode = 'time';

var HistoryManager = {
    stack: [], pointer: -1, maxSize: 50,
    push: function(type, data) {
        if (this.pointer < this.stack.length - 1) { this.stack = this.stack.slice(0, this.pointer + 1); }
        this.stack.push({ type: type, data: data, time: new Date().toLocaleTimeString('zh-CN', {hour:'2-digit',minute:'2-digit'}) });
        if (this.stack.length > this.maxSize) this.stack.shift();
        this.pointer = this.stack.length - 1;
        this.updateUI();
    },
    undo: function() {
        if (!this.canUndo()) return;
        var item = this.stack[this.pointer]; this.pointer--;
        this._applyReverse(item); this.updateUI();
    },
    redo: function() {
        if (!this.canRedo()) return;
        this.pointer++;
        var item = this.stack[this.pointer];
        this._applyForward(item); this.updateUI();
    },
    canUndo: function() { return this.pointer >= 0; },
    canRedo: function() { return this.pointer < this.stack.length - 1; },
    _applyReverse: function(item) {
        switch(item.type) {
            case 'filter':
                if (item.data.prevCategories) { activeCategories = new Set(item.data.prevCategories); showFoodMarkers(); generateCategoryFilters(); }
                break;
            case 'search':
                var si = document.getElementById('searchInput'); if (si) si.value = '';
                var sr = document.getElementById('searchResults'); if (sr) sr.style.display = 'none';
                if (map) { foodMarkers.forEach(function(m) { m.setOpacity(1); }); spotMarkers.forEach(function(m) { m.setOpacity(1); }); }
                break;
            case 'fav_add':
                FavoriteManager.remove(item.data.id, item.data.type); break;
            case 'fav_del':
                FavoriteManager.add({ id: item.data.id, type: item.data.type, name: item.data.name }); break;
            case 'route':
                clearRoute(); autoFitView(); break;
            case 'view':
                if (map) map.clearInfoWindow(); break;
        }
        RankingManager.refresh();
    },
    _applyForward: function(item) {
        switch(item.type) {
            case 'filter':
                if (item.data.newCategories) { activeCategories = new Set(item.data.newCategories); showFoodMarkers(); generateCategoryFilters(); }
                break;
            case 'fav_add':
                FavoriteManager.add({ id: item.data.id, type: item.data.type, name: item.data.name }); break;
            case 'fav_del':
                FavoriteManager.remove(item.data.id, item.data.type); break;
            case 'view':
                if (map && item.data.lng) { map.setZoomAndCenter(16, [item.data.lng, item.data.lat]); }
                break;
        }
        RankingManager.refresh();
    },
    updateUI: function() {
        var ub = document.getElementById('btnHistoryUndo'); if (ub) ub.style.opacity = this.canUndo() ? '1' : '0.4';
        var rb = document.getElementById('btnHistoryRedo'); if (rb) rb.style.opacity = this.canRedo() ? '1' : '0.4';
        this._renderDropdown();
    },
    _renderDropdown: function() {
        var list = document.getElementById('historyDropdownList'); if (!list) return;
        var html = '';
        for (var i = this.stack.length - 1; i >= 0; i--) {
            var item = this.stack[i];
            var m = i === this.pointer ? '&#9654;' : (i > this.pointer ? '&#9679;' : '&#9675;');
            var cls = i === this.pointer ? 'history-current' : (i > this.pointer ? 'history-done' : 'history-undone');
            var labels = { view:'查看', filter:'筛选', search:'搜索', fav_add:'收藏', fav_del:'取消收藏', route:'路线' };
            html += '<div class="history-item '+cls+'" data-index="'+i+'"><span class="history-marker">'+m+'</span><span class="history-time">'+item.time+'</span><span class="history-label">'+ (labels[item.type]||item.type) +'</span></div>';
        }
        list.innerHTML = html || '<div class="history-empty">暂无操作记录</div>';
    },
    clear: function() { this.stack = []; this.pointer = -1; this.updateUI(); }
};

var FavoriteManager = {
    items: [], storageKey: 'heze_favorites',
    init: function() { this.load(); },
    add: function(poi) {
        if (this.isFavorite(poi.id, poi.type)) return;
        this.items.push({ id: poi.id, type: poi.type, name: poi.name, addedAt: Date.now() });
        this.save();
        HistoryManager.push('fav_add', { id: poi.id, type: poi.type, name: poi.name });
        this.renderPanel();
        RankingManager.refresh();
        ProfileManager.renderPanel();
        DailyTourManager.generate();
        if (typeof showFoodMarkers === 'function') showFoodMarkers();
        if (typeof showSpotMarkers === 'function') showSpotMarkers();
    },
    remove: function(id, type) {
        var item = this.items.find(function(f) { return f.id === id && f.type === type; });
        this.items = this.items.filter(function(f) { return !(f.id === id && f.type === type); });
        this.save();
        if (item) HistoryManager.push('fav_del', { id: id, type: type, name: item.name });
        this.renderPanel();
        RankingManager.refresh();
        ProfileManager.renderPanel();
        DailyTourManager.generate();
        if (typeof showFoodMarkers === 'function') showFoodMarkers();
        if (typeof showSpotMarkers === 'function') showSpotMarkers();
    },
    isFavorite: function(id, type) { return this.items.some(function(f) { return f.id === id && f.type === type; }); },
    getAll: function() {
        var self = this;
        return this.items.map(function(f) {
            var data = f.type === 'food' ? foodData.find(function(x) { return x.id === f.id; }) :
                       spotData.find(function(x) { return x.id === f.id; });
            return data ? Object.assign({}, data, { favType: f.type, addedAt: f.addedAt }) : null;
        }).filter(function(x) { return x !== null; });
    },
    save: function() {
        var d = this.items.map(function(f) { return { id: f.id, type: f.type, addedAt: f.addedAt }; });
        try { localStorage.setItem(this.storageKey, JSON.stringify(d)); } catch(e) {}
    },
    load: function() {
        try { var r = localStorage.getItem(this.storageKey); if (r) this.items = JSON.parse(r); } catch(e) { this.items = []; }
    },
    renderPanel: function() {
        var container = document.getElementById('favoritesList'); if (!container) return;
        var favs = this.getAll();
        var countEl = document.getElementById('favoritesCount'); if (countEl) countEl.textContent = '(' + favs.length + ')';
        if (favs.length === 0) {
            container.innerHTML = '<div class="favorites-empty">暂无收藏</div>';
        } else {
            var html = '';
            favs.forEach(function(f) {
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
            container.querySelectorAll('.favorite-item').forEach(function(el) {
                el.addEventListener('click', function(e) {
                    if (e.target.classList.contains('fav-star')) return;
                    var lng = parseFloat(this.dataset.lng), lat = parseFloat(this.dataset.lat);
                    if (map) map.setZoomAndCenter(16, [lng, lat]);
                });
            });
            container.querySelectorAll('.fav-star').forEach(function(el) {
                el.addEventListener('click', function(e) { e.stopPropagation();
                    FavoriteManager.remove(parseInt(this.dataset.poiId), this.dataset.poiType); });
            });
        }
    }
};

var RankingManager = {
    currentTab: 'popular',
    getPopular: function() { return foodData.slice().sort(function(a,b){return b.score - a.score;}); },
    getValue: function() {
        return foodData.slice().sort(function(a,b){
            return (b.score/Math.log(b.price+1)) - (a.score/Math.log(a.price+1));
        });
    },
    getFavRanking: function() {
        var fc = {};
        FavoriteManager.items.forEach(function(f){ if(f.type==='food') fc[f.id]=(fc[f.id]||0)+1; });
        return foodData.slice().sort(function(a,b){
            var fa=fc[a.id]||0, fb=fc[b.id]||0;
            if(fb!==fa) return fb-fa; return b.score-a.score;
        });
    },
    refresh: function() { this.renderPanel(); },
    renderPanel: function() {
        var list = document.getElementById('rankingList'); if(!list) return;
        var ranking = this.currentTab==='popular' ? this.getPopular() : (this.currentTab==='value' ? this.getValue() : this.getFavRanking());
        var medals = ['&#129351;','&#129352;','&#129353;'];
        var html = '';
        ranking.slice(0,10).forEach(function(f,i){
            var rankHtml = i<3 ? '<span style="font-size:18px;">'+medals[i]+'</span>' : '<span class="ranking-num">'+(i+1)+'</span>';
            var isFav = FavoriteManager.isFavorite(f.id,'food');
            html += '<div class="ranking-item" data-lng="'+f.lng+'" data-lat="'+f.lat+'"><div class="ranking-rank">'+rankHtml+'</div>' +
                '<div class="ranking-info"><div class="ranking-name">'+f.name+(isFav?' &#9733;':'')+'</div>' +
                '<div class="ranking-meta">'+f.score.toFixed(1)+'分 / Y'+f.price+' / '+f.category+'</div></div></div>';
        });
        list.innerHTML = html;
        list.querySelectorAll('.ranking-item').forEach(function(el){ el.addEventListener('click',function(){
            map.setZoomAndCenter(16,[parseFloat(this.dataset.lng),parseFloat(this.dataset.lat)]); }); });
    },
    init: function() {
        var self = this;
        document.querySelectorAll('.ranking-tab').forEach(function(tab){ tab.addEventListener('click',function(){
            document.querySelectorAll('.ranking-tab').forEach(function(t){t.classList.remove('active');});
            this.classList.add('active'); self.currentTab = this.dataset.tab; self.renderPanel(); }); });
    }
};

var ProfileManager = {
    renderPanel: function() {
        var container = document.getElementById('profilePanel');
        if (!container) return;
        var favs = FavoriteManager.getAll();
        if (favs.length === 0) {
            container.innerHTML = '<div class="profile-empty">收藏美食后可查看画像</div>';
            return;
        }
        
        var foods = favs.filter(function(f) { return f.favType === 'food'; });
        if (foods.length === 0) {
            container.innerHTML = '<div class="profile-empty">收藏美食后可查看画像</div>';
            return;
        }

        // 1. Taste analysis
        var tasteCounts = {};
        foods.forEach(function(f) {
            if (f.tags) {
                f.tags.forEach(function(t) {
                    if (t.match(/^[^\d]/) && t.length <= 4 && !t.match(/^(早|午|晚|宵|下午|全天|堂食|外卖|快餐|聚餐|一人食|宴请|团餐|亲子|情侣|朋友|商务|独自|家庭|WiFi|停车|包厢|露天|空调|儿童|老字号|非遗|中华|地方|网红|必吃|百年|口碑|现点|秘制|限量|季节|配酒|人均|鲁菜|川菜|粤菜|湘菜|清真|东北|豫菜|苏菜|特色)$/)) {
                        tasteCounts[t] = (tasteCounts[t] || 0) + 1;
                    }
                });
            }
        });
        var tasteSorted = Object.keys(tasteCounts).sort(function(a,b){return tasteCounts[b]-tasteCounts[a];}).slice(0, 8);
        
        // 2. Price analysis
        var priceBands = {'<10':0,'10-30':0,'30-60':0,'60-100':0,'100+':0};
        foods.forEach(function(f) {
            if (f.price <= 10) priceBands['<10']++;
            else if (f.price <= 30) priceBands['10-30']++;
            else if (f.price <= 60) priceBands['30-60']++;
            else if (f.price <= 100) priceBands['60-100']++;
            else priceBands['100+']++;
        });
        var maxPrice = Math.max.apply(null, Object.values(priceBands)) || 1;
        
        // 3. Category analysis
        var catCounts = {};
        foods.forEach(function(f) { catCounts[f.category] = (catCounts[f.category] || 0) + 1; });
        var catSorted = Object.keys(catCounts).sort(function(a,b){return catCounts[b]-catCounts[a];}).slice(0, 5);
        var maxCat = Math.max.apply(null, Object.values(catCounts)) || 1;

        var html = '';

        // Taste tags
        html += '<div class="profile-section"><div class="profile-section-title">口味偏好</div><div class="profile-taste-cloud">';
        tasteSorted.forEach(function(t) {
            var size = Math.max(12, 12 + (tasteCounts[t] / tasteCounts[tasteSorted[0]]) * 10);
            html += '<span class="profile-taste-tag" style="font-size:' + size + 'px;">' + t + '(' + tasteCounts[t] + ')</span>';
        });
        html += '</div></div>';

        // Price bars
        html += '<div class="profile-section"><div class="profile-section-title">价格区间</div>';
        ['<10','10-30','30-60','60-100','100+'].forEach(function(b) {
            var pct = Math.round((priceBands[b] / foods.length) * 100);
            html += '<div class="profile-bar-row"><span class="profile-bar-label">' + (b === '<10' ? '<10元' : b === '100+' ? '100+元' : b + '元') + '</span>' +
                '<div class="profile-bar"><div class="profile-bar-fill" style="width:' + pct + '%"></div></div>' +
                '<span class="profile-bar-pct">' + pct + '%</span></div>';
        });
        html += '</div>';

        // Category bars
        html += '<div class="profile-section"><div class="profile-section-title">偏好品类</div>';
        catSorted.forEach(function(c) {
            var pct = Math.round((catCounts[c] / foods.length) * 100);
            html += '<div class="profile-bar-row"><span class="profile-bar-label">' + c + '</span>' +
                '<div class="profile-bar"><div class="profile-bar-fill" style="width:' + pct + '%"></div></div>' +
                '<span class="profile-bar-pct">' + pct + '%</span></div>';
        });
        html += '</div>';

        container.innerHTML = html;
    }
};

var DailyTourManager = {
    candidates: [],

    generate: function() {
        var self = this;
        var favs = FavoriteManager.getAll();
        var foods = favs.filter(function(f) { return f.favType === 'food'; });
        var spots = favs.filter(function(f) { return f.favType === 'spot'; });
        
        // Determine preferred categories and price from favorites
        var catPrefs = {};
        var priceSum = 0;
        foods.forEach(function(f) {
            catPrefs[f.category] = (catPrefs[f.category] || 0) + 1;
            priceSum += f.price;
        });
        var topCats = Object.keys(catPrefs).sort(function(a,b){return catPrefs[b]-catPrefs[a];}).slice(0, 3);
        var avgPrice = foods.length > 0 ? priceSum / foods.length : 40;
        
        // Score all foods: match category preference + score
        var scored = foodData.map(function(f) {
            var catMatch = topCats.indexOf(f.category) >= 0 ? 0.3 : 0;
            var priceMatch = Math.abs(f.price - avgPrice) < 20 ? 0.1 : 0;
            var score = f.score / 5 * 0.4 + catMatch + priceMatch;
            return { id: f.id, name: f.name, category: f.category, score: f.score, price: f.price, lng: f.lng, lat: f.lat, totalScore: score };
        });
        scored.sort(function(a,b){return b.totalScore - a.totalScore;});
        
        var topFoods = scored.slice(0, 20);
        
        // Find nearest high-score spot for each combination of 3 foods
        var candidates = [];
        var usedFoods = new Set();
        for (var i = 0; i < topFoods.length && candidates.length < 5; i++) {
            for (var j = i + 1; j < topFoods.length && candidates.length < 5; j++) {
                for (var k = j + 1; k < topFoods.length && candidates.length < 5; k++) {
                    var trio = [topFoods[i], topFoods[j], topFoods[k]];
                    var key = trio.map(function(x){return x.id;}).sort().join(',');
                    if (usedFoods.has(key)) continue;
                    usedFoods.add(key);
                    
                    // Find nearest spot
                    var centerLng = (trio[0].lng + trio[1].lng + trio[2].lng) / 3;
                    var centerLat = (trio[0].lat + trio[1].lat + trio[2].lat) / 3;
                    var nearest = null, nearestDist = Infinity;
                    spotData.forEach(function(s) {
                        var d = haversine(centerLng, centerLat, s.lng, s.lat);
                        if (d < nearestDist) { nearestDist = d; nearest = s; }
                    });
                    if (!nearest) continue;
                    
                    var totalDist = 0;
                    var prev = nearest;
                    trio.forEach(function(f) {
                        totalDist += haversine(prev.lng, prev.lat, f.lng, f.lat);
                        prev = f;
                    });
                    
                    if (totalDist > 50000) continue; // Skip >50km
                    
                    var avgScore = (trio[0].score + trio[1].score + trio[2].score) / 3;
                    var routeScore = avgScore * 0.7 + (1 - totalDist / 50000) * 0.3;
                    
                    candidates.push({
                        spot: nearest,
                        foods: trio,
                        totalDist: totalDist,
                        routeScore: routeScore,
                        avgScore: avgScore
                    });
                }
            }
        }
        
        candidates.sort(function(a,b){return b.routeScore - a.routeScore;});
        this.candidates = candidates.slice(0, 3);
        this.renderPanel();
    },

    renderPanel: function() {
        var container = document.getElementById('dailyTourList');
        if (!container) return;
        if (this.candidates.length === 0) {
            container.innerHTML = '<div class="profile-empty">收藏POI后可生成推荐</div>';
            return;
        }
        
        var best = this.candidates[0];
        var html = '<div class="daily-tour-best">';
        html += '<div class="daily-tour-name">' + best.spot.name + '周边美食游</div>';
        html += '<div class="daily-tour-meta">评分 ' + best.avgScore.toFixed(1) + ' | 驾车约 ' + (best.totalDist/1000).toFixed(1) + 'km / ' + Math.round(best.totalDist/400) + 'min</div>';
        html += '<div class="daily-tour-stops">';
        html += '<div class="daily-tour-stop">1. ' + best.spot.name + ' (景点/&#9733;' + best.spot.score.toFixed(1) + ')</div>';
        best.foods.forEach(function(f, i) {
            html += '<div class="daily-tour-stop">' + (i+2) + '. ' + f.name + ' (' + f.category + '/&#9733;' + f.score.toFixed(1) + '/Y' + f.price + ')</div>';
        });
        html += '</div>';
        html += '<button class="daily-tour-adopt" onclick="DailyTourManager.adopt(0)">采纳此路线</button>';
        html += '</div>';
        
        if (this.candidates.length > 1) {
            html += '<div class="daily-tour-alt-title">备选方案</div>';
            for (var i = 1; i < this.candidates.length; i++) {
                var alt = this.candidates[i];
                html += '<div class="daily-tour-alt">';
                html += '<span>' + alt.spot.name + '游</span>';
                html += '<span>评分 ' + alt.avgScore.toFixed(1) + ' / ' + (alt.totalDist/1000).toFixed(0) + 'km</span>';
                html += '<button class="daily-tour-adopt small" onclick="DailyTourManager.adopt(' + i + ')">采纳</button>';
                html += '</div>';
            }
        }
        
        container.innerHTML = html;
    },

    adopt: function(index) {
        var tour = this.candidates[index];
        if (!tour) return;
        // Set route start to the spot
        var startSel = document.getElementById('routeStart');
        if (startSel) {
            for (var i = 0; i < startSel.options.length; i++) {
                if (startSel.options[i].value === 'spot_' + tour.spot.id) {
                    startSel.selectedIndex = i; break;
                }
            }
        }
        // Clear existing waypoints and set food waypoints
        routeWaypoints = [];
        tour.foods.forEach(function(f) {
            routeWaypoints.push({ id: f.id, name: f.name, type: 'food', lng: f.lng, lat: f.lat });
        });
        renderWaypointList();
        // Set end to last food
        var endSel = document.getElementById('routeEnd');
        if (endSel && tour.foods.length > 0) {
            var last = tour.foods[tour.foods.length - 1];
            for (var j = 0; j < endSel.options.length; j++) {
                if (endSel.options[j].value === 'food_' + last.id) {
                    endSel.selectedIndex = j; break;
                }
            }
        }
    }
};

var HEZE_CENTER = [115.477, 35.245];  // 菏泽市中心（基于真实数据重新计算）
var AMAP_KEY = "647bb3e7a596c479b998f3e20a5a486a";

// ==================== 分类颜色映射 ====================
var CATEGORY_COLORS = {
    '汤类': '#ff6f00',  // 深橙
    '面食': '#f9a825',  // 金黄
    '小吃': '#e64a19',  // 红棕
    '正餐': '#c62828',  // 深红
    '烧烤': '#bf360c',  // 暗红
    '甜品': '#ad1457',  // 粉红
    '饮品': '#6a1b9a',  // 紫色
    '凉菜': '#2e7d32',  // 绿色
    'default': '#ff5722' // 默认橙
};

var CATEGORY_ICONS = {
    '汤类': '汤', '面食': '面', '小吃': '吃', '正餐': '餐',
    '烧烤': '烤', '甜品': '甜', '饮品': '饮', '凉菜': '凉',
    'default': '食'
};

// ==================== 地图初始化 ====================

function loadMap() {
    if (typeof AMapLoader === 'undefined') {
        console.error('[Map] AMapLoader 未加载，使用备用方案');
        showMapFallback();
        return;
    }

    AMapLoader.load({
        key: AMAP_KEY,
        version: "2.0",
        plugins: ["AMap.ToolBar", "AMap.Scale", "AMap.Geocoder"]
    }).then(function (AMap) {
        console.log('[Map] 高德地图加载成功');

        map = new AMap.Map('map', {
            zoom: 13,
            center: HEZE_CENTER,
            mapStyle: 'amap://styles/light',
            features: ['bg', 'road', 'building', 'point']
        });

        map.addControl(new AMap.ToolBar({ position: 'LT' }));
        map.addControl(new AMap.Scale());

        // 初始化逆地理编码
        geocoder = new AMap.Geocoder({ city: '菏泽' });

        initControls();
        loadData();

    }).catch(function (e) {
        console.error('[Map] 地图加载失败:', e);
        showMapFallback();
    });
}

function showMapFallback() {
    document.getElementById('map').innerHTML =
        '<div style="display:flex;align-items:center;justify-content:center;height:100%;background:#f0f2f5;">' +
        '<div style="text-align:center;padding:40px;">' +
        '<h3 style="color:#d32f2f;">地图加载失败</h3>' +
        '<p style="color:#666;">请检查网络连接或 API Key 有效性</p>' +
        '<p style="color:#999;font-size:13px;">Key: ' + AMAP_KEY.substring(0, 8) + '...</p>' +
        '</div></div>';
    loadData();  // 仍尝试加载数据
}

// ==================== 数据加载 ====================

async function loadData() {
    console.log('[Map] 加载数据...');

    var loadPromises = [];

    loadPromises.push(
        fetch('data/food.json')
            .then(r => r.ok ? r.json() : Promise.reject('HTTP ' + r.status))
            .then(d => { foodData = d; console.log('[Map] 美食:', d.length, '条'); })
            .catch(e => console.warn('[Map] food.json:', e))
    );

    loadPromises.push(
        fetch('data/spot.json')
            .then(r => r.ok ? r.json() : Promise.reject('HTTP ' + r.status))
            .then(d => { spotData = d; console.log('[Map] 景点:', d.length, '条'); })
            .catch(e => console.warn('[Map] spot.json:', e))
    );

    await Promise.all(loadPromises);

    updateStats();
    generateCategoryFilters();
        initSearch();
        populateSelectOptions();
        initRoutePlanner();
        FavoriteManager.init();
        FavoriteManager.renderPanel();
        RankingManager.init();
        RankingManager.renderPanel();
        ProfileManager.renderPanel();
        DailyTourManager.generate();
        initHistory();
        var refreshBtn = document.getElementById('btnRefreshTour');
        if (refreshBtn) { refreshBtn.addEventListener('click', function() { ProfileManager.renderPanel(); DailyTourManager.generate(); }); }
        document.querySelectorAll('.header-tool-group').forEach(function(group) {
            var dropdown = group.querySelector('.header-dropdown');
            if (!dropdown) return;
            group.addEventListener('click', function(e) {
                e.stopPropagation();
                dropdown.style.display = dropdown.style.display === 'block' ? 'none' : 'block';
            });
        });
        document.addEventListener('click', function() {
            document.querySelectorAll('.header-dropdown').forEach(function(d) { d.style.display = 'none'; });
        });

    if (map) {
        if (foodData.length > 0) showFoodMarkers();
        if (spotData.length > 0) showSpotMarkers();
        setTimeout(autoFitView, 500);
    }
}

// ==================== 逆地理编码（坐标 → 真实地址） ====================

function getAddress(lng, lat, callback) {
    var cacheKey = lng.toFixed(5) + ',' + lat.toFixed(5);
    if (addressCache[cacheKey]) {
        callback(addressCache[cacheKey]);
        return;
    }

    if (!geocoder) {
        callback('地址获取中...');
        return;
    }

    geocoder.getAddress([lng, lat], function(status, result) {
        if (status === 'complete' && result.regeocode) {
            var addr = result.regeocode.formattedAddress || '未知地址';
            addressCache[cacheKey] = addr;
            callback(addr);
        } else {
            callback('地址获取失败');
        }
    });
}

// ==================== 美食标记 ====================

function showFoodMarkers() {
    foodMarkers.forEach(function (m) { m.setMap(null); });
    foodMarkers = [];

    foodData.forEach(function (food, index) {
        if (activeCategories.size > 0 && !activeCategories.has(food.category)) return;

        var color = CATEGORY_COLORS[food.category] || CATEGORY_COLORS['default'];

        var displayChar = food.name.charAt(0);
        var markerContent =
            '<div class="food-marker" style="' +
            'background:' + color + ';' +
            'color:white;' +
            'width:32px;height:32px;' +
            'border-radius:50%;' +
            'display:flex;align-items:center;justify-content:center;' +
            'font-size:14px;font-weight:bold;' +
            'box-shadow:0 2px 6px rgba(0,0,0,0.3);' +
            'border:2px solid white;' +
            'cursor:pointer;' +
            'transition:transform 0.2s;' +
            '">' +
            displayChar +
            '</div>';

        if (FavoriteManager.isFavorite(food.id, 'food')) {
            markerContent = markerContent.replace('border:2px solid white', 'border:3px solid #ffd700');
        }

        try {
            var marker = new AMap.Marker({
                position: new AMap.LngLat(food.lng, food.lat),
                content: markerContent,
                offset: new AMap.Pixel(-8, -8),
                zIndex: 100,
                title: food.name
            });

            marker.on('click', function () {
                showFoodDetail(food);
            });

            marker.on('mouseover', function () {
                this.setContent(markerContent.replace('width:32px','width:40px').replace('height:32px','height:40px').replace('font-size:14px','font-size:17px'));
            });
            marker.on('mouseout', function () {
                this.setContent(markerContent);
            });

            marker._foodData = food;
            marker.setMap(map);
            foodMarkers.push(marker);
        } catch (e) {
            console.warn('[Map] 美食标记失败:', food.name, e);
        }
    });

    if (!toggleFoodVisible) {
        foodMarkers.forEach(function(m) { m.setMap(null); });
    }
    if (openOnlyActive) {
        foodMarkers.forEach(function(m) {
            if (m._foodData && getOpenStatus(m._foodData.opentime).cls === 'status-closed') {
                m.setMap(null);
            }
        });
    }

    console.log('[Map] 美食标记:', foodMarkers.length, '个');
}

// ==================== 景点标记 ====================

function showSpotMarkers() {
    spotMarkers.forEach(function (m) { m.setMap(null); });
    spotMarkers = [];

    spotData.forEach(function (spot) {
        var markerContent =
            '<div class="spot-marker" style="' +
            'background:#1565c0;' +
            'color:white;' +
            'width:36px;height:36px;' +
            'border-radius:4px;' +
            'transform:rotate(45deg);' +
            'display:flex;align-items:center;justify-content:center;' +
            'font-size:16px;' +
            'box-shadow:0 2px 8px rgba(0,0,0,0.3);' +
            'border:2px solid white;' +
            'cursor:pointer;' +
            '">' +
            '<span style="transform:rotate(-45deg);">景</span>' +
            '</div>';

        if (FavoriteManager.isFavorite(spot.id, 'spot')) {
            markerContent = markerContent.replace('border:2px solid white', 'border:3px solid #ffd700');
        }

        try {
            var marker = new AMap.Marker({
                position: new AMap.LngLat(spot.lng, spot.lat),
                content: markerContent,
                offset: new AMap.Pixel(-18, -18),
                zIndex: 110,
                title: spot.name
            });

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

    if (!toggleSpotVisible) {
        spotMarkers.forEach(function(m) { m.setMap(null); });
    }

    console.log('[Map] 景点标记:', spotMarkers.length, '个');
}

// ==================== 信息窗口 ====================

function showFoodDetail(food) {
    // 评分星级
    var stars = '';
    var fullStars = Math.floor(food.score);
    var halfStar = (food.score - fullStars) >= 0.5;
    for (var i = 0; i < fullStars; i++) stars += '★';
    if (halfStar) stars += '☆';

    var color = CATEGORY_COLORS[food.category] || CATEGORY_COLORS['default'];
    var icon = CATEGORY_ICONS[food.category] || CATEGORY_ICONS['default'];

    // 价格等级
    var priceLevel = '';
    if (food.price <= 10) priceLevel = '💰';
    else if (food.price <= 30) priceLevel = '💰💰';
    else if (food.price <= 60) priceLevel = '💰💰💰';
    else priceLevel = '💰💰💰💰';

    // 标签
    var tagsHtml = '';
    if (food.tags && food.tags.length > 0) {
        tagsHtml = '<div class="info-tags">';
        food.tags.forEach(function (t) {
            tagsHtml += '<span class="info-tag">' + t + '</span>';
        });
        tagsHtml += '</div>';
    }

    var addrId = 'addr-food-' + food.id;
    var photosHtml = '';
    if (food.photos && food.photos.length > 0) {
        photosHtml = '<div class="info-photos">';
        food.photos.forEach(function(url) {
            photosHtml += '<img src="' + url + '" style="width:220px;height:150px;object-fit:cover;border-radius:4px;margin:2px;" onerror="this.style.display=\'none\'">';
        });
        photosHtml += '</div>';
    }

    var statusBar = '';
    if (food.opentime && food.opentime !== '-') {
        var s = getOpenStatus(food.opentime);
        statusBar = '<div style="font-size:12px;margin-bottom:6px;color:' + (s.cls === 'status-open' ? '#4caf50' : '#999') + ';">' +
            (s.cls === 'status-open' ? '●' : '○') + ' ' + s.text + ' · ' + food.opentime + '</div>';
    }
    HistoryManager.push('view', { poiId: food.id, poiType: 'food', lng: food.lng, lat: food.lat, poiName: food.name });
    var isFav = FavoriteManager.isFavorite(food.id, 'food');
    var starBtn = '<div class="fav-btn ' + (isFav ? 'faved' : '') + '" data-poi-id="' + food.id + '" data-poi-type="food" style="cursor:pointer;padding:4px 8px;margin-bottom:8px;background:' + (isFav ? '#fff3e0' : '#f5f5f5') + ';border-radius:4px;text-align:center;font-size:13px;border:1px solid ' + (isFav ? '#ff9800' : '#ddd') + ';">' + (isFav ? '★ 已收藏' : '☆ 收藏') + '</div>';
    var html =
        '<div class="info-window">' +
        starBtn +
        photosHtml +
        statusBar +
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

    var infoWindow = new AMap.InfoWindow({
        content: html,
        offset: new AMap.Pixel(0, -25),
        closeWhenClickMap: true
    });

    infoWindow.open(map, [food.lng, food.lat]);

    setTimeout(function() {
        var se = document.querySelector('.fav-btn[data-poi-id="' + food.id + '"]');
        if (se) { se.addEventListener('click', function(e) { e.stopPropagation();
            if (FavoriteManager.isFavorite(food.id, 'food')) { FavoriteManager.remove(food.id, 'food'); se.innerHTML = '&#9734; 收藏'; se.className = 'fav-btn'; se.style.background = '#f5f5f5'; se.style.borderColor = '#ddd'; }
            else { FavoriteManager.add({ id: food.id, type: 'food', name: food.name }); se.innerHTML = '&#9733; 已收藏'; se.className = 'fav-btn faved'; se.style.background = '#fff3e0'; se.style.borderColor = '#ff9800'; } }); }
    }, 100);

    var addrEl = document.getElementById(addrId);
    if (addrEl) {
        if (food.address && food.address !== '-' && food.address !== '无法获取') {
            addrEl.textContent = food.address;
        } else {
            getAddress(food.lng, food.lat, function (addr) {
                var el2 = document.getElementById(addrId);
                if (el2) el2.textContent = addr;
            });
        }
    }
}

function showSpotDetail(spot) {
    HistoryManager.push('view', { poiId: spot.id, poiType: 'spot', lng: spot.lng, lat: spot.lat, poiName: spot.name });
    var stars = '';
    var fullStars = Math.floor(spot.score);
    for (var i = 0; i < fullStars; i++) stars += '★';

    var tagsHtml = '';
    if (spot.tags && spot.tags.length > 0) {
        tagsHtml = '<div class="info-tags">';
        spot.tags.forEach(function (t) {
            tagsHtml += '<span class="info-tag" style="background:#e3f2fd;color:#1565c0;">' + t + '</span>';
        });
        tagsHtml += '</div>';
    }

    var addrId = 'addr-spot-' + spot.id;
    var photosHtml = '';
    if (spot.photos && spot.photos.length > 0) {
        photosHtml = '<div class="info-photos">';
        spot.photos.forEach(function(url) {
            photosHtml += '<img src="' + url + '" style="width:220px;height:150px;object-fit:cover;border-radius:4px;margin:2px;" onerror="this.style.display=\'none\'">';
        });
        photosHtml += '</div>';
    }

    var isFav = FavoriteManager.isFavorite(spot.id, 'spot');
    var starBtn = '<div class="fav-btn ' + (isFav ? 'faved' : '') + '" data-poi-id="' + spot.id + '" data-poi-type="spot" style="cursor:pointer;padding:4px 8px;margin-bottom:8px;background:' + (isFav ? '#fff3e0' : '#f5f5f5') + ';border-radius:4px;text-align:center;font-size:13px;border:1px solid ' + (isFav ? '#ff9800' : '#ddd') + ';">' + (isFav ? '★ 已收藏' : '☆ 收藏') + '</div>';
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
        closeWhenClickMap: true
    });

    infoWindow.open(map, [spot.lng, spot.lat]);

    setTimeout(function() {
        var se = document.querySelector('.fav-btn[data-poi-id="' + spot.id + '"]');
        if (se) { se.addEventListener('click', function(e) { e.stopPropagation();
            if (FavoriteManager.isFavorite(spot.id, 'spot')) { FavoriteManager.remove(spot.id, 'spot'); se.innerHTML = '&#9734; 收藏'; se.className = 'fav-btn'; se.style.background = '#f5f5f5'; se.style.borderColor = '#ddd'; }
            else { FavoriteManager.add({ id: spot.id, type: 'spot', name: spot.name }); se.innerHTML = '&#9733; 已收藏'; se.className = 'fav-btn faved'; se.style.background = '#fff3e0'; se.style.borderColor = '#ff9800'; } }); }
    }, 100);

    // 如果已有地址缓存则直接显示，否则异步获取真实地址
    if (!spot.address || spot.address === '' || spot.address === '-' || spot.address === '无法获取') {
        getAddress(spot.lng, spot.lat, function (addr) {
            var el = document.getElementById(addrId);
            if (el) el.textContent = addr;
        });
    }
}

// ==================== 视野自适应 ====================

function autoFitView() {
    if (!map) return;

    var allLngLats = [];

    foodData.forEach(function (f) {
        if (activeCategories.size === 0 || activeCategories.has(f.category)) {
            allLngLats.push(new AMap.LngLat(f.lng, f.lat));
        }
    });

    spotData.forEach(function (s) {
        allLngLats.push(new AMap.LngLat(s.lng, s.lat));
    });

    if (allLngLats.length > 0) {
        try {
            map.setFitView(allLngLats, false, [60, 60, 60, 360]);
        } catch (e) {
            map.setZoomAndCenter(13, HEZE_CENTER);
        }
    }
}

// ==================== 图层控制与UI ====================

function initControls() {
    var btnFood = document.getElementById('btnToggleFood');
    var btnSpot = document.getElementById('btnToggleSpot');

    if (btnFood) {
        btnFood.addEventListener('click', function() {
            toggleFoodVisible = !toggleFoodVisible;
            if (toggleFoodVisible) {
                this.classList.add('active');
                foodMarkers.forEach(function(m) { m.setMap(map); });
            } else {
                this.classList.remove('active');
                foodMarkers.forEach(function(m) { m.setMap(null); });
            }
        });
    }
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

    var btnOpenOnly = document.getElementById('btnOpenOnly');
    if (btnOpenOnly) {
        btnOpenOnly.addEventListener('click', function() {
            openOnlyActive = !openOnlyActive;
            if (openOnlyActive) { this.classList.add('active'); } else { this.classList.remove('active'); }
            showFoodMarkers();
        });
    }

    document.getElementById('btnZoomIn').addEventListener('click', function () {
        if (map) map.zoomIn();
    });
    document.getElementById('btnZoomOut').addEventListener('click', function () {
        if (map) map.zoomOut();
    });
    document.getElementById('btnReset').addEventListener('click', function () {
        if (map) map.setZoomAndCenter(13, HEZE_CENTER);
    });
}

function generateCategoryFilters() {
    var container = document.getElementById('categoryFilters');
    var cats = {};
    foodData.forEach(function (f) {
        if (f.category) {
            if (!cats[f.category]) cats[f.category] = 0;
            cats[f.category]++;
        }
    });

    var html = '<div class="category-tag active" data-category="all">全部(' + foodData.length + ')</div>';
    Object.keys(cats).sort().forEach(function (c) {
        var icon = CATEGORY_ICONS[c] || '';
        html += '<div class="category-tag" data-category="' + c + '">' + icon + ' ' + c + '(' + cats[c] + ')</div>';
    });
    container.innerHTML = html;

    container.querySelectorAll('.category-tag').forEach(function (tag) {
        tag.addEventListener('click', function () {
            var cat = this.dataset.category;
            var prevCats = Array.from(activeCategories);
            if (cat === 'all') {
                activeCategories.clear();
                container.querySelectorAll('.category-tag').forEach(function (t) { t.classList.remove('active'); });
                this.classList.add('active');
                HistoryManager.push('filter', { prevCategories: prevCats, newCategories: Array.from(activeCategories) });
            } else {
                this.classList.toggle('active');
                if (activeCategories.has(cat)) activeCategories.delete(cat);
                else activeCategories.add(cat);
                HistoryManager.push('filter', { prevCategories: prevCats, newCategories: Array.from(activeCategories) });

                var allTag = container.querySelector('[data-category="all"]');
                if (activeCategories.size === 0) allTag.classList.add('active');
                else allTag.classList.remove('active');
            }
            if (map) {
                showFoodMarkers();
                autoFitView();
            }
        });
    });
}

function updateStats() {
    document.getElementById('foodCount').textContent = foodData.length;
    document.getElementById('spotCount').textContent = spotData.length;
}

function populateSelectOptions() {
    var allPOIs = [];
    foodData.forEach(function(f) { allPOIs.push({name: f.name, id: f.id, type: 'food', category: f.category || '美食', lng: f.lng, lat: f.lat}); });
    spotData.forEach(function(s) { allPOIs.push({name: s.name, id: s.id, type: 'spot', category: s.type || '景点', lng: s.lng, lat: s.lat}); });
    allPOIs.sort(function(a,b) { return a.name.localeCompare(b.name, 'zh'); });

    var optionHtml = '<option value="">-- 选择POI --</option>';
    allPOIs.forEach(function(p) {
        optionHtml += '<option value="' + p.type + '_' + p.id + '">' + p.name + ' (' + p.category + ')</option>';
    });

    var startSel = document.getElementById('routeStart');
    var endSel = document.getElementById('routeEnd');
    if (startSel) startSel.innerHTML = optionHtml;
    if (endSel) endSel.innerHTML = optionHtml;
}

function initRoutePlanner() {
    var modeBtns = document.querySelectorAll('.route-mode-btn[data-mode]');
    modeBtns.forEach(function(btn) {
        btn.addEventListener('click', function() {
            modeBtns.forEach(function(b) { b.classList.remove('active'); });
            this.classList.add('active');
            routeMode = this.dataset.mode;
        });
    });

    var sortBtns = document.querySelectorAll('.route-mode-btn[data-sort]');
    sortBtns.forEach(function(btn) {
        btn.addEventListener('click', function() {
            sortBtns.forEach(function(b) { b.classList.remove('active'); });
            this.classList.add('active');
            routeSortMode = this.dataset.sort;
        });
    });

    var addBtn = document.getElementById('btnAddWaypoint');
    if (addBtn) {
        addBtn.addEventListener('click', function() {
            if (routeWaypoints.length >= 5) { alert('最多添加5个途经点'); return; }
            routeWaypoints.push({ id: 0, name: '途经点' + (routeWaypoints.length + 1), type: 'food', lng: 0, lat: 0 });
            renderWaypointList();
        });
    }

    var planBtn = document.getElementById('btnPlanRoute');
    if (planBtn) {
        planBtn.addEventListener('click', function() { executeRoutePlan(); });
    }

    var clearBtn = document.getElementById('btnClearRoute');
    if (clearBtn) {
        clearBtn.addEventListener('click', function() { clearRoute(); });
    }
}

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

    var allPOIs = [];
    foodData.forEach(function(f) { allPOIs.push({name: f.name, id: f.id, type: 'food', category: f.category || '美食', lng: f.lng, lat: f.lat}); });
    spotData.forEach(function(s) { allPOIs.push({name: s.name, id: s.id, type: 'spot', category: s.type || '景点', lng: s.lng, lat: s.lat}); });
    allPOIs.sort(function(a,b) { return a.name.localeCompare(b.name, 'zh'); });

    container.querySelectorAll('.wp-select').forEach(function(sel) {
        var idx = parseInt(sel.dataset.index);
        var optHtml = '<option value="">选择途经点</option>';
        allPOIs.forEach(function(p) {
            optHtml += '<option value="' + p.type + '_' + p.id + '" ' +
                (routeWaypoints[idx].id === p.id && routeWaypoints[idx].type === p.type ? 'selected' : '') +
                '>' + p.name + ' (' + p.category + ')</option>';
        });
        sel.innerHTML = optHtml;
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
                routeWaypoints[wpIdx] = { id: 0, name: '途经点' + (wpIdx + 1), type: 'food', lng: 0, lat: 0 };
            }
        });
    });

    container.querySelectorAll('.wp-del').forEach(function(el) {
        el.addEventListener('click', function() {
            var idx = parseInt(this.dataset.index);
            routeWaypoints.splice(idx, 1);
            renderWaypointList();
        });
    });
    container.querySelectorAll('.wp-up').forEach(function(el) {
        el.addEventListener('click', function() {
            var idx = parseInt(this.dataset.index);
            if (idx > 0) { var t = routeWaypoints[idx-1]; routeWaypoints[idx-1] = routeWaypoints[idx]; routeWaypoints[idx] = t; renderWaypointList(); }
        });
    });
    container.querySelectorAll('.wp-down').forEach(function(el) {
        el.addEventListener('click', function() {
            var idx = parseInt(this.dataset.index);
            if (idx < routeWaypoints.length - 1) { var t = routeWaypoints[idx+1]; routeWaypoints[idx+1] = routeWaypoints[idx]; routeWaypoints[idx] = t; renderWaypointList(); }
        });
    });
}

// ==================== 搜索 ====================

function matchesQuery(text, query) {
    if (!query || !text) return false;
    var lowerText = text.toLowerCase().replace(/\s+/g, '');
    var lowerQuery = query.toLowerCase().replace(/\s+/g, '');
    return lowerText.indexOf(lowerQuery) !== -1;
}

function performSearch() {
    var query = document.getElementById('searchInput').value.trim();
    var searchType = document.querySelector('.search-type-tag.active').dataset.type;
    var resultDiv = document.getElementById('searchResults');
    var resultList = document.getElementById('searchResultList');
    var resultCount = document.getElementById('searchResultCount');

    if (!query) {
        resultDiv.style.display = 'none';
        if (map) {
            foodMarkers.forEach(function(m) { m.setOpacity(1); });
            spotMarkers.forEach(function(m) { m.setOpacity(1); });
        }
        return;
    }

    HistoryManager.push('search', { query: query, searchType: searchType });

    var results = [];

    if (searchType === 'all' || searchType === 'food') {
        foodData.forEach(function(f) {
            var m = matchesQuery(f.name, query) || matchesQuery(f.address, query) ||
                    (f.tags && f.tags.some(function(t) { return matchesQuery(t, query); }));
            if (m) results.push({ type: 'food', data: f });
        });
    }
    if (searchType === 'all' || searchType === 'spot') {
        spotData.forEach(function(s) {
            var m = matchesQuery(s.name, query) || matchesQuery(s.address, query) ||
                    matchesQuery(s.description, query);
            if (m) results.push({ type: 'spot', data: s });
        });
    }

    resultCount.textContent = results.length;

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

function initSearch() {
    var searchInput = document.getElementById('searchInput');
    if (!searchInput) return;
    searchInput.addEventListener('input', function() {
        clearTimeout(this._searchTimer);
        this._searchTimer = setTimeout(performSearch, 200);
    });
    document.querySelectorAll('.search-type-tag').forEach(function(tag) {
        tag.addEventListener('click', function() {
            document.querySelectorAll('.search-type-tag').forEach(function(t) { t.classList.remove('active'); });
            this.classList.add('active');
            performSearch();
        });
    });
}

// ==================== 营业状态 ====================

function getOpenStatus(opentime) {
    if (!opentime || opentime === '-' || opentime === '24小时' || opentime === '全天' || opentime.indexOf('全天') !== -1) {
        return { cls: 'status-open', text: '营业中' };
    }
    var now = new Date();
    var currentMin = now.getHours() * 60 + now.getMinutes();
    var segments = opentime.split(',');
    for (var i = 0; i < segments.length; i++) {
        var parts = segments[i].trim().split('-');
        if (parts.length === 2) {
            var s = parts[0].trim().split(':');
            var e = parts[1].trim().split(':');
            if (s.length >= 2 && e.length >= 2) {
                var start = parseInt(s[0]) * 60 + parseInt(s[1]);
                var end = parseInt(e[0]) * 60 + parseInt(e[1]);
                if (end <= start) end += 24 * 60;
                if (currentMin >= start && currentMin <= end) {
                    return { cls: 'status-open', text: '营业中' };
                }
                if (currentMin + 24*60 >= start && currentMin + 24*60 <= end) {
                    return { cls: 'status-open', text: '营业中' };
                }
            }
        }
    }
    return { cls: 'status-closed', text: '已打烊' };
}

// ==================== 路线规划 ====================

function executeRoutePlan() {
    var startVal = document.getElementById('routeStart').value;
    var endVal = document.getElementById('routeEnd').value;
    if (!startVal || !endVal) { alert('请选择起点和终点'); return; }

    var startPOI = findPOI(startVal);
    var endPOI = findPOI(endVal);
    if (!startPOI || !endPOI) { alert('无效的POI选择'); return; }

    var allWaypoints = [startPOI];
    routeWaypoints.forEach(function(wp) {
        if (wp.lng !== 0 && wp.lat !== 0 && !isNaN(wp.lng) && !isNaN(wp.lat)) allWaypoints.push(wp);
    });
    allWaypoints.push(endPOI);

    for (var i = 0; i < allWaypoints.length; i++) {
        var w = allWaypoints[i];
        if (isNaN(w.lng) || isNaN(w.lat) || w.lng === 0 || w.lat === 0) {
            alert('途径点坐标无效，请重新选择');
            return;
        }
    }

    for (var i = 0; i < allWaypoints.length - 1; i++) {
        var d = haversine(allWaypoints[i].lng, allWaypoints[i].lat, allWaypoints[i+1].lng, allWaypoints[i+1].lat);
        if (d > 500000) { alert('距离过远(' + (d/1000).toFixed(0) + 'km)，建议分日出行'); return; }
    }

    clearRoute();
    HistoryManager.push('route', {});
    document.getElementById('routeDetail').style.display = 'block';
    document.getElementById('routeSummary').innerHTML = '<div class="route-loading">路线规划中...</div>';
    document.getElementById('routeSegments').innerHTML = '';

    window._routeSegments = new Array(allWaypoints.length - 1);
    planRouteSegments(allWaypoints, 0);
}

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

function clearRoute() {
    currentRoutePolylines.forEach(function(p) { p.setMap(null); });
    currentRouteMarkers.forEach(function(m) { m.setMap(null); });
    currentRoutePolylines = [];
    currentRouteMarkers = [];
    window._routeSegments = [];
    var rd = document.getElementById('routeDetail');
    if (rd) rd.style.display = 'none';
}

function planRouteSegments(waypoints, index) {
    if (!map || index >= waypoints.length - 1) {
        if (index >= waypoints.length - 1) renderRouteSummary();
        return;
    }

    var from = waypoints[index];
    var to = waypoints[index + 1];

    var strategyMap = { time: 0, distance: 2, toll: 1 };
    var strategy = strategyMap[routeSortMode] || 0;
    var url = 'https://restapi.amap.com/v3/direction/' + routeMode +
              '?origin=' + from.lng + ',' + from.lat +
              '&destination=' + to.lng + ',' + to.lat +
              '&key=' + AMAP_KEY + '&strategy=' + strategy + '&output=json';

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = function() {
        if (xhr.status === 200) {
            try {
                var data = JSON.parse(xhr.responseText);
                if (data.status === '1' && data.route && data.route.paths && data.route.paths.length > 0) {
                    var path = data.route.paths[0];
                    var segment = {
                        from: from.name, to: to.name,
                        distance: path.distance, duration: path.duration,
                        tolls: path.tolls || 0,
                        steps: parsePolyline(path.steps || [])
                    };
                    renderRouteSegment(segment, index);
                } else {
                    renderRouteSegmentFallback(from, to, index);
                }
            } catch(e) {
                renderRouteSegmentFallback(from, to, index);
            }
        } else {
            renderRouteSegmentFallback(from, to, index);
        }
        setTimeout(function() { planRouteSegments(waypoints, index + 1); }, 300);
    };
    xhr.onerror = function() {
        renderRouteSegmentFallback(from, to, index);
        setTimeout(function() { planRouteSegments(waypoints, index + 1); }, 300);
    };
    xhr.timeout = 10000;
    xhr.ontimeout = function() {
        renderRouteSegmentFallback(from, to, index);
        setTimeout(function() { planRouteSegments(waypoints, index + 1); }, 300);
    };
    xhr.send();
}

function parsePolyline(steps) {
    var allPoints = [];
    if (!steps) return allPoints;
    steps.forEach(function(step) {
        if (step.polyline && typeof step.polyline === 'string') {
            var pts = step.polyline.split(';');
            pts.forEach(function(p) {
                var xy = p.split(',');
                if (xy.length === 2) {
                    allPoints.push([parseFloat(xy[0]), parseFloat(xy[1])]);
                }
            });
        }
    });
    return allPoints;
}

function renderRouteSegment(segment, index) {
    if (!map) return;
    if (!segment || isNaN(segment.distance) || isNaN(segment.duration) || segment.distance <= 0) return;
    var colors = ['#ff5722', '#e64a19', '#bf360c', '#ff9800', '#f57c00', '#ff7043'];
    var color = colors[index % colors.length];

    if (segment.steps.length > 1) {
        var path = segment.steps.map(function(p) { return new AMap.LngLat(p[0], p[1]); });
        var polyline = new AMap.Polyline({
            path: path, strokeColor: color, strokeWeight: 5,
            strokeOpacity: 0.7, lineJoin: 'round'
        });
        polyline.setMap(map);
        currentRoutePolylines.push(polyline);
    }

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

function renderRouteSegmentFallback(from, to, index) {
    if (!map) return;
    if (!from || !to || isNaN(from.lng) || isNaN(from.lat) || isNaN(to.lng) || isNaN(to.lat) || from.lng === 0 || from.lat === 0 || to.lng === 0 || to.lat === 0) return;
    var dist = haversine(from.lng, from.lat, to.lng, to.lat);
    if (isNaN(dist) || dist <= 0) return;
    var speed = routeMode === 'walking' ? 5 : 40;
    var durationMin = Math.round(dist / (speed * 1000 / 60));
    var segment = { from: from.name, to: to.name, distance: Math.round(dist), duration: durationMin * 60, tolls: 0, steps: [[from.lng, from.lat], [to.lng, to.lat]] };

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

function haversine(lng1, lat1, lng2, lat2) {
    var R = 6371000;
    var dlat = (lat2 - lat1) * Math.PI / 180;
    var dlng = (lng2 - lng1) * Math.PI / 180;
    var a = Math.sin(dlat/2) * Math.sin(dlat/2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dlng/2) * Math.sin(dlng/2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function renderRouteSummary() {
    if (!window._routeSegments || window._routeSegments.length === 0) return;
    var totalDist = 0, totalTime = 0, totalTolls = 0;
    window._routeSegments.forEach(function(s, i) {
        if (!s || isNaN(s.distance) || isNaN(s.duration)) return;
        totalDist += s.distance; totalTime += s.duration; totalTolls += (s.tolls || 0);
    });

    var sm = document.getElementById('routeSummary');

    if (totalDist > 10000000 || isNaN(totalDist)) {
        if (sm) sm.innerHTML = '<div class="route-error">路径计算异常，请重试</div>';
        return;
    }

    var summaryHtml = '';
    summaryHtml += '<div class="route-summary-item">总距离: <strong>' + (totalDist / 1000).toFixed(1) + ' km</strong></div>';
    summaryHtml += '<div class="route-summary-item">总时间: <strong>' + Math.round(totalTime / 60) + ' 分钟</strong></div>';
    if (routeMode === 'driving' && totalTolls > 0) summaryHtml += '<div class="route-summary-item">过路费: <strong>Y' + totalTolls + '</strong></div>';
    if (sm) sm.innerHTML = summaryHtml;

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

// ==================== 历史管理器 UI ====================

function initHistory() {
    var undoBtn = document.getElementById('btnHistoryUndo');
    var redoBtn = document.getElementById('btnHistoryRedo');
    var clearBtn = document.getElementById('btnHistoryClear');
    var dropdown = document.getElementById('historyDropdown');
    var trigger = document.getElementById('btnHistoryDropdown');
    
    if (undoBtn) undoBtn.addEventListener('click', function() { HistoryManager.undo(); });
    if (redoBtn) redoBtn.addEventListener('click', function() { HistoryManager.redo(); });
    if (clearBtn) clearBtn.addEventListener('click', function(e) { e.stopPropagation(); HistoryManager.clear(); });
    
    if (trigger && dropdown) {
        trigger.addEventListener('click', function(e) { e.stopPropagation();
            dropdown.style.display = dropdown.style.display === 'block' ? 'none' : 'block';
        });
        dropdown.addEventListener('click', function(e) {
            var item = e.target.closest('.history-item'); if (!item) return;
            var idx = parseInt(item.dataset.index);
            while (HistoryManager.pointer > idx) HistoryManager.undo();
            while (HistoryManager.pointer < idx) HistoryManager.redo();
        });
    }
}

// ==================== 启动 ====================

window.onload = function () {
    console.log('[Map] 系统启动...');
    console.log('[Map] API Key:', AMAP_KEY.substring(0, 8) + '...');
    loadMap();
};
