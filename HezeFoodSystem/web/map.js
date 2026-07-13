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
var routePolyline = null;
var routeMarkers = [];
var foodData = [];
var spotData = [];
var routeData = null;
var activeCategories = new Set();
var addressCache = {};       // 逆地理编码缓存

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
    '汤类': '🍜', '面食': '🍝', '小吃': '🍢', '正餐': '🍽️',
    '烧烤': '🔥', '甜品': '🍰', '饮品': '☕', '凉菜': '🥗',
    'default': '🍴'
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

    loadPromises.push(
        fetch('data/route.json')
            .then(r => r.ok ? r.json() : Promise.reject('HTTP ' + r.status))
            .then(d => { routeData = d; console.log('[Map] 路线:', d.name); })
            .catch(e => console.warn('[Map] route.json:', e))
    );

    await Promise.all(loadPromises);

    updateStats();
    generateCategoryFilters();

    if (map) {
        if (foodData.length > 0) showFoodMarkers();
        if (spotData.length > 0) showSpotMarkers();
        if (routeData) showRoute();

        // 自动适配视野
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
        var icon = CATEGORY_ICONS[food.category] || CATEGORY_ICONS['default'];
        var displayId = index + 1;

        var markerContent =
            '<div class="food-marker" style="' +
            'background:' + color + ';' +
            'color:white;' +
            'width:32px;height:32px;' +
            'border-radius:50% 50% 50% 0;' +
            'transform:rotate(-45deg);' +
            'display:flex;align-items:center;justify-content:center;' +
            'font-size:14px;' +
            'box-shadow:0 2px 6px rgba(0,0,0,0.3);' +
            'border:2px solid white;' +
            'cursor:pointer;' +
            'transition:transform 0.2s;' +
            '">' +
            '<span style="transform:rotate(45deg);">' + icon + '</span>' +
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

            // 鼠标悬停变大
            marker.on('mouseover', function () {
                this.setContent(markerContent.replace('32px', '40px').replace('14px', '17px'));
                this.setOffset(new AMap.Pixel(-10, -10));
            });
            marker.on('mouseout', function () {
                this.setContent(markerContent);
                this.setOffset(new AMap.Pixel(-8, -8));
            });

            marker.setMap(map);
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
            '<span style="transform:rotate(-45deg);">🏛</span>' +
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
    var html =
        '<div class="info-window">' +
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
        '<h4 style="color:#1565c0;border-color:#1565c0;">🏛 ' + spot.name + '</h4>' +
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

// ==================== 路线可视化 ====================

function showRoute() {
    if (!routeData || !routeData.found || !map) return;

    if (routePolyline) routePolyline.setMap(null);
    routeMarkers.forEach(function (m) { m.setMap(null); });
    routeMarkers = [];

    try {
        var path = routeData.path.map(function (p) {
            return new AMap.LngLat(p[0], p[1]);
        });

        // 路线折线
        routePolyline = new AMap.Polyline({
            path: path,
            strokeColor: '#ff5722',
            strokeWeight: 6,
            strokeOpacity: 0.7,
            lineJoin: 'round',
            showDir: true,
            dirColor: '#fff'
        });
        routePolyline.setMap(map);

        // 路径点标记
        routeData.waypoints.forEach(function (point, i) {
            var isFood = point.type === 'food';
            var isStart = i === 0;
            var isEnd = i === routeData.waypoints.length - 1;
            var label = isStart ? '起' : (isEnd ? '终' : String(i));

            var bgColor = isFood ? '#ff5722' : '#1565c0';
            var shape = isFood ? '50%' : '4px';
            var size = isStart || isEnd ? '36px' : '28px';

            var content =
                '<div style="' +
                'background:' + bgColor + ';' +
                'color:white;' +
                'width:' + size + ';height:' + size + ';' +
                'border-radius:' + shape + ';' +
                'display:flex;align-items:center;justify-content:center;' +
                'font-size:' + (isStart || isEnd ? '14px' : '12px') + ';' +
                'font-weight:bold;' +
                'border:3px solid white;' +
                'box-shadow:0 2px 8px rgba(0,0,0,0.3);' +
                '">' + label + '</div>';

            var marker = new AMap.Marker({
                position: new AMap.LngLat(point.lng, point.lat),
                content: content,
                offset: new AMap.Pixel(-parseInt(size) / 2, -parseInt(size) / 2),
                zIndex: 200,
                title: point.name
            });

            // 点击路径点显示详情
            marker.on('click', function () {
                var wp = point;
                var wpHtml =
                    '<div class="info-window">' +
                    '<h4 style="color:' + bgColor + ';border-color:' + bgColor + ';">' +
                    (isFood ? '🍴 ' : '🏛 ') + wp.name + '</h4>' +
                    '<p style="color:#666;">' + (isStart ? '起点' : (isEnd ? '终点' : '途经点 ' + i)) + '</p>' +
                    '</div>';
                new AMap.InfoWindow({
                    content: wpHtml,
                    offset: new AMap.Pixel(0, -20)
                }).open(map, [wp.lng, wp.lat]);
            });

            marker.setMap(map);
            routeMarkers.push(marker);
        });

        // 路线标签
        var midPath = path[Math.floor(path.length / 2)];
        var routeLabel = new AMap.Marker({
            position: midPath,
            content: '<div style="background:white;color:#ff5722;padding:4px 12px;border-radius:12px;font-size:12px;font-weight:bold;box-shadow:0 2px 6px rgba(0,0,0,0.2);border:1px solid #ff5722;">' +
                (routeData.totalDistance / 1000).toFixed(1) + 'km / ' + routeData.totalTime.toFixed(0) + 'min</div>',
            offset: new AMap.Pixel(-40, -20),
            zIndex: 150
        });
        routeLabel.setMap(map);
        routeMarkers.push(routeLabel);

        console.log('[Map] 路线显示完成');
    } catch (e) {
        console.error('[Map] 路线显示失败:', e);
    }

    updateRouteInfo();
}

function updateRouteInfo() {
    var div = document.getElementById('routeInfo');
    if (!routeData || !routeData.found) {
        div.innerHTML = '<p style="color:#999;text-align:center;">暂无路线数据</p>';
        return;
    }

    var html = '<div class="route-header">' +
        '<h4>📋 ' + routeData.name + '</h4>' +
        '<p>总距离: <b>' + (routeData.totalDistance / 1000).toFixed(1) + ' 公里</b></p>' +
        '<p>预计时间: <b>' + routeData.totalTime.toFixed(0) + ' 分钟</b></p>' +
        '</div>';

    routeData.waypoints.forEach(function (point, i) {
        var isFood = point.type === 'food';
        var isStart = i === 0;
        var isEnd = i === routeData.waypoints.length - 1;

        html += '<div class="waypoint">' +
            '<div class="waypoint-icon ' + (isFood ? 'food' : 'spot') + '">' +
            (isStart ? '起' : (isEnd ? '终' : i)) + '</div>' +
            '<span class="waypoint-name">' + point.name + '</span></div>';

        if (i < routeData.waypoints.length - 1) {
            html += '<div style="text-align:center;color:#ccc;font-size:18px;line-height:0.5;">↓</div>';
        }
    });

    div.innerHTML = html;
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
        foodMarkers.forEach(function (m) { m.setVisible(this.checked); }.bind(this));
    });
    document.getElementById('toggleSpot').addEventListener('change', function () {
        spotMarkers.forEach(function (m) { m.setVisible(this.checked); }.bind(this));
    });
    document.getElementById('toggleRoute').addEventListener('change', function () {
        if (routePolyline) routePolyline.setVisible(this.checked);
        routeMarkers.forEach(function (m) { m.setVisible(this.checked); }.bind(this));
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
    if (routeData && routeData.found) {
        document.getElementById('routeDistance').textContent = (routeData.totalDistance / 1000).toFixed(1);
        document.getElementById('routeTime').textContent = routeData.totalTime.toFixed(0);
    } else {
        document.getElementById('routeDistance').textContent = '0';
        document.getElementById('routeTime').textContent = '0';
    }
}

// ==================== 启动 ====================

window.onload = function () {
    console.log('[Map] 系统启动...');
    console.log('[Map] API Key:', AMAP_KEY.substring(0, 8) + '...');
    loadMap();
};
