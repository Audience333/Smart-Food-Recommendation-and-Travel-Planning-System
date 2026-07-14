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
var currentRoutePolylines = [];
var currentRouteMarkers = [];
var routeWaypoints = [];
var routeMode = 'driving';

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

        try {
            var marker = new AMap.Marker({
                position: new AMap.LngLat(food.lng, food.lat),
                content: markerContent,
                offset: new AMap.Pixel(-8, -8),
                zIndex: 100,
                title: food.name
            });

            // 点击显示详情
            marker.on('click', function () {
                showFoodDetail(food);
            });

            marker.on('mouseover', function () {
                this.setContent(markerContent.replace('width:32px','width:40px').replace('height:32px','height:40px').replace('font-size:14px','font-size:17px'));
            });
            marker.on('mouseout', function () {
                this.setContent(markerContent);
            });

            marker.setMap(map);
            marker._foodData = food;
            if (!toggleFoodVisible) { marker.setVisible(false); }
            else {
                var status = getOpenStatus(food.opentime);
                if (status.cls === 'status-closed') marker.setVisible(false);
            }
            foodMarkers.push(marker);
        } catch (e) {
            console.warn('[Map] 美食标记失败:', food.name, e);
        }
    });

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

            marker.setMap(map);
            if (!toggleSpotVisible) marker.setVisible(false);
            spotMarkers.push(marker);
        } catch (e) {
            console.warn('[Map] 景点标记失败:', spot.name, e);
        }
    });

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
    var statusBar = '';
    if (food.opentime && food.opentime !== '-') {
        var s = getOpenStatus(food.opentime);
        statusBar = '<div style="font-size:12px;margin-bottom:6px;color:' + (s.cls === 'status-open' ? '#4caf50' : '#999') + ';">' +
            (s.cls === 'status-open' ? '●' : '○') + ' ' + s.text + ' · ' + food.opentime + '</div>';
    }
    var html =
        '<div class="info-window">' +
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

    // 异步获取真实地址
    getAddress(food.lng, food.lat, function (addr) {
        var el = document.getElementById(addrId);
        if (el) el.textContent = addr;
    });
}

function showSpotDetail(spot) {
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
    var html =
        '<div class="info-window">' +
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

    // 如果已有地址缓存则直接显示，否则异步获取真实地址
    if (!spot.address || spot.address === '') {
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
    document.getElementById('toggleFood').addEventListener('change', function () {
        toggleFoodVisible = this.checked;
        foodMarkers.forEach(function (m) {
            if (!m._foodData) { m.setVisible(toggleFoodVisible); return; }
            if (!toggleFoodVisible) { m.setVisible(false); return; }
            var status = getOpenStatus(m._foodData.opentime);
            m.setVisible(status.cls !== 'status-closed');
        });
    });
    document.getElementById('toggleSpot').addEventListener('change', function () {
        toggleSpotVisible = this.checked;
        spotMarkers.forEach(function (m) { m.setVisible(toggleSpotVisible); });
    });

    document.getElementById('btnZoomIn').addEventListener('click', function () {
        if (map) map.zoomIn();
    });
    document.getElementById('btnZoomOut').addEventListener('click', function () {
        if (map) map.zoomOut();
    });
    document.getElementById('btnReset').addEventListener('click', function () {
        if (map) map.setZoomAndCenter(13, HEZE_CENTER);
    });

    var openOnlyEl = document.getElementById('toggleOpenOnly');
    if (openOnlyEl) {
        openOnlyEl.addEventListener('change', function() {
            var showOnlyOpen = this.checked;
            foodMarkers.forEach(function(m) {
                if (!m._foodData) return;
                if (showOnlyOpen && getOpenStatus(m._foodData.opentime).cls === 'status-closed') {
                    m.setVisible(false);
                } else {
                    m.setVisible(toggleFoodVisible);
                }
            });
        });
    }
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
            if (cat === 'all') {
                activeCategories.clear();
                container.querySelectorAll('.category-tag').forEach(function (t) { t.classList.remove('active'); });
                this.classList.add('active');
            } else {
                this.classList.toggle('active');
                if (activeCategories.has(cat)) activeCategories.delete(cat);
                else activeCategories.add(cat);

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
    var modeBtns = document.querySelectorAll('.route-mode-btn');
    modeBtns.forEach(function(btn) {
        btn.addEventListener('click', function() {
            modeBtns.forEach(function(b) { b.classList.remove('active'); });
            this.classList.add('active');
            routeMode = this.dataset.mode;
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
        if (wp.lng !== 0 && wp.lat !== 0) allWaypoints.push(wp);
    });
    allWaypoints.push(endPOI);

    for (var i = 0; i < allWaypoints.length - 1; i++) {
        var d = haversine(allWaypoints[i].lng, allWaypoints[i].lat, allWaypoints[i+1].lng, allWaypoints[i+1].lat);
        if (d > 500000) { alert('距离过远(' + (d/1000).toFixed(0) + 'km)，建议分日出行'); return; }
    }

    clearRoute();
    document.getElementById('routeDetail').style.display = 'block';
    document.getElementById('routeSummary').innerHTML = '<div class="route-loading">路线规划中...</div>';
    document.getElementById('routeSegments').innerHTML = '';

    window._routeSegments = [];
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
    document.getElementById('routeDetail').style.display = 'none';
}

function planRouteSegments(waypoints, index) {
    if (!map || index >= waypoints.length - 1) {
        if (index >= waypoints.length - 1) renderRouteSummary();
        return;
    }

    var from = waypoints[index];
    var to = waypoints[index + 1];

    var url = 'https://restapi.amap.com/v3/direction/' + routeMode +
              '?origin=' + from.lng + ',' + from.lat +
              '&destination=' + to.lng + ',' + to.lat +
              '&key=' + AMAP_KEY + '&strategy=0&output=json';

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
    window._routeSegments.push(segment);
}

function renderRouteSegmentFallback(from, to, index) {
    if (!map) return;
    var dist = haversine(from.lng, from.lat, to.lng, to.lat);
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
    window._routeSegments.push(segment);
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
    window._routeSegments.forEach(function(s) { totalDist += s.distance; totalTime += s.duration; totalTolls += s.tolls; });

    var sm = document.getElementById('routeSummary');
    var summaryHtml = '';
    summaryHtml += '<div class="route-summary-item">总距离: <strong>' + (totalDist / 1000).toFixed(1) + ' km</strong></div>';
    summaryHtml += '<div class="route-summary-item">总时间: <strong>' + Math.round(totalTime / 60) + ' 分钟</strong></div>';
    if (routeMode === 'driving' && totalTolls > 0) summaryHtml += '<div class="route-summary-item">过路费: <strong>Y' + totalTolls + '</strong></div>';
    if (sm) sm.innerHTML = summaryHtml;

    var segHtml = '';
    window._routeSegments.forEach(function(s, i) {
        segHtml += '<div class="route-segment">' +
            '<div class="route-segment-header">' + (i + 1) + '. ' + s.from + ' - ' + s.to + '</div>' +
            '<div class="route-segment-detail">距离 ' + (s.distance / 1000).toFixed(1) + 'km / 时间 ' + Math.round(s.duration / 60) + '分钟</div>' +
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

// ==================== 启动 ====================

window.onload = function () {
    console.log('[Map] 系统启动...');
    console.log('[Map] API Key:', AMAP_KEY.substring(0, 8) + '...');
    loadMap();
};
