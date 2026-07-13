/**
 * 菏泽美食推荐与城市漫游规划系统 - 地图可视化模块
 */

var map = null;
var foodMarkers = [];
var spotMarkers = [];
var routePolyline = null;
var routeMarkers = [];
var foodData = [];
var spotData = [];
var routeData = null;
var activeCategories = new Set();

var HEZE_CENTER = [115.4806, 35.2337];

function loadMap() {
    if (typeof AMapLoader === 'undefined') {
        console.error('[Map] AMapLoader 未加载');
        loadData();
        return;
    }

    AMapLoader.load({
        key: "647bb3e7a596c479b998f3e20a5a486a",
        version: "2.0",
        plugins: ["AMap.ToolBar", "AMap.Scale"]
    }).then(function (AMap) {
        console.log('[Map] AMap 加载成功');

        map = new AMap.Map('map', {
            zoom: 12,
            center: HEZE_CENTER,
            mapStyle: 'amap://styles/normal'
        });

        map.addControl(new AMap.ToolBar({ position: 'LT' }));
        map.addControl(new AMap.Scale());

        initControls();
        loadData();

    }).catch(function (e) {
        console.error('[Map] AMap 加载失败:', e);
        document.getElementById('map').innerHTML =
            '<div style="padding:40px;text-align:center;color:#666;">' +
            '<h3>地图加载失败</h3>' +
            '<p>可能原因：API Key 无效或网络问题</p>' +
            '</div>';
        loadData();
    });
}

async function loadData() {
    console.log('[Map] 开始加载数据...');

    try {
        var foodRes = await fetch('data/food.json');
        if (foodRes.ok) {
            foodData = await foodRes.json();
            console.log('[Map] 美食数据:', foodData.length, '条');
        }
    } catch (e) {
        console.error('[Map] food.json 加载失败:', e);
    }

    try {
        var spotRes = await fetch('data/spot.json');
        if (spotRes.ok) {
            spotData = await spotRes.json();
            console.log('[Map] 景点数据:', spotData.length, '条');
        }
    } catch (e) {
        console.error('[Map] spot.json 加载失败:', e);
    }

    try {
        var routeRes = await fetch('data/route.json');
        if (routeRes.ok) {
            routeData = await routeRes.json();
            console.log('[Map] 路线数据加载成功');
        }
    } catch (e) {
        console.error('[Map] route.json 加载失败:', e);
    }

    updateStats();
    generateCategoryFilters();

    if (map) {
        if (foodData.length > 0) showFoodMarkers();
        if (spotData.length > 0) showSpotMarkers();
        if (routeData) showRoute();
    }
}

function showFoodMarkers() {
    foodMarkers.forEach(function (m) { m.setMap(null); });
    foodMarkers = [];

    var count = 0;
    foodData.forEach(function (food) {
        if (activeCategories.size > 0 && !activeCategories.has(food.category)) return;

        try {
            var marker = new AMap.Marker({
                position: new AMap.LngLat(food.lng, food.lat),
                title: food.name,
                offset: new AMap.Pixel(-10, -10)
            });

            marker.on('click', function () {
                showFoodInfo(food, this);
            });

            marker.setMap(map);
            foodMarkers.push(marker);
            count++;
        } catch (e) {
            console.warn('[Map] 标记失败:', food.name);
        }
    });

    console.log('[Map] 美食标记:', count, '个');
}

function showSpotMarkers() {
    spotMarkers.forEach(function (m) { m.setMap(null); });
    spotMarkers = [];

    var count = 0;
    spotData.forEach(function (spot) {
        try {
            var marker = new AMap.Marker({
                position: new AMap.LngLat(spot.lng, spot.lat),
                title: spot.name,
                offset: new AMap.Pixel(-10, -10)
            });

            marker.on('click', function () {
                showSpotInfo(spot, this);
            });

            marker.setMap(map);
            spotMarkers.push(marker);
            count++;
        } catch (e) {
            console.warn('[Map] 标记失败:', spot.name);
        }
    });

    console.log('[Map] 景点标记:', count, '个');
}

function showFoodInfo(food, marker) {
    var tags = '';
    if (food.tags && food.tags.length > 0) {
        tags = '<div style="margin-top:8px;">';
        food.tags.forEach(function (t) {
            tags += '<span style="display:inline-block;padding:2px 8px;margin:2px;background:#fff3e0;color:#e65100;border-radius:10px;font-size:12px;">' + t + '</span>';
        });
        tags += '</div>';
    }

    var html = '<div style="padding:10px;min-width:200px;">' +
        '<h4 style="color:#d32f2f;margin:0 0 8px 0;">' + food.name + '</h4>' +
        '<p style="margin:4px 0;">评分：<span style="color:#ff9800;">' + food.score + '</span></p>' +
        '<p style="margin:4px 0;">价格：<span style="color:#4caf50;">' + food.price + ' 元</span></p>' +
        '<p style="margin:4px 0;">分类：' + food.category + '</p>' +
        tags + '</div>';

    new AMap.InfoWindow({ content: html, offset: new AMap.Pixel(0, -20) }).open(map, marker.getPosition());
}

function showSpotInfo(spot, marker) {
    var html = '<div style="padding:10px;min-width:200px;">' +
        '<h4 style="color:#1565c0;margin:0 0 8px 0;">' + spot.name + '</h4>' +
        '<p style="margin:4px 0;">类型：' + spot.type + '</p>' +
        '<p style="margin:4px 0;">评分：<span style="color:#ff9800;">' + spot.score + '</span></p>' +
        '<p style="margin:4px 0;">门票：' + spot.ticketInfo + '</p>' +
        '<p style="margin:4px 0;">时间：' + spot.openingTime + '</p>' +
        '<p style="margin:4px 0;color:#666;">' + spot.description + '</p>' +
        '</div>';

    new AMap.InfoWindow({ content: html, offset: new AMap.Pixel(0, -20) }).open(map, marker.getPosition());
}

function showRoute() {
    if (!routeData || !routeData.found || !map) return;

    if (routePolyline) routePolyline.setMap(null);
    routeMarkers.forEach(function (m) { m.setMap(null); });
    routeMarkers = [];

    try {
        var path = routeData.path.map(function (p) { return new AMap.LngLat(p[0], p[1]); });

        routePolyline = new AMap.Polyline({
            path: path,
            strokeColor: '#ff5722',
            strokeWeight: 6,
            strokeOpacity: 0.8,
            showDir: true
        });
        routePolyline.setMap(map);

        routeData.waypoints.forEach(function (point, i) {
            var isFood = point.type === 'food';
            var label = i === 0 ? 'S' : (i === routeData.waypoints.length - 1 ? 'E' : String(i));

            var marker = new AMap.Marker({
                position: new AMap.LngLat(point.lng, point.lat),
                content: '<div style="background:' + (isFood ? '#ff5722' : '#2196f3') +
                    ';color:white;width:24px;height:24px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:11px;font-weight:bold;border:2px solid white;">' + label + '</div>',
                offset: new AMap.Pixel(-12, -12),
                zIndex: 100
            });
            marker.setMap(map);
            routeMarkers.push(marker);
        });

        map.setFitView();
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

    var html = '<div style="background:#fff3e0;padding:10px;border-radius:6px;border-left:4px solid #ff9800;margin-bottom:10px;">' +
        '<h4 style="color:#e65100;margin:0 0 5px 0;">' + routeData.name + '</h4>' +
        '<p style="margin:2px 0;font-size:13px;color:#666;">总距离: ' + (routeData.totalDistance / 1000).toFixed(1) + ' 公里</p>' +
        '<p style="margin:2px 0;font-size:13px;color:#666;">预计时间: ' + routeData.totalTime.toFixed(1) + ' 分钟</p>' +
        '</div>';

    routeData.waypoints.forEach(function (point) {
        var isFood = point.type === 'food';
        html += '<div style="display:flex;align-items:center;padding:6px 0;border-bottom:1px dashed #eee;">' +
            '<div style="width:24px;height:24px;border-radius:50%;background:' + (isFood ? '#ff5722' : '#2196f3') +
            ';color:white;display:flex;align-items:center;justify-content:center;font-size:11px;margin-right:8px;font-weight:bold;">' +
            (isFood ? 'F' : 'S') + '</div>' +
            '<span style="font-size:14px;">' + point.name + '</span></div>';
    });

    div.innerHTML = html;
}

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

    document.getElementById('btnZoomIn').addEventListener('click', function () { map.zoomIn(); });
    document.getElementById('btnZoomOut').addEventListener('click', function () { map.zoomOut(); });
    document.getElementById('btnReset').addEventListener('click', function () { map.setZoomAndCenter(12, HEZE_CENTER); });
}

function generateCategoryFilters() {
    var container = document.getElementById('categoryFilters');
    var cats = new Set();
    foodData.forEach(function (f) { if (f.category) cats.add(f.category); });

    var html = '<div class="category-tag active" data-category="all">全部</div>';
    cats.forEach(function (c) {
        html += '<div class="category-tag" data-category="' + c + '">' + c + '</div>';
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
            if (map) showFoodMarkers();
        });
    });
}

function updateStats() {
    document.getElementById('foodCount').textContent = foodData.length;
    document.getElementById('spotCount').textContent = spotData.length;
    if (routeData && routeData.found) {
        document.getElementById('routeDistance').textContent = (routeData.totalDistance / 1000).toFixed(1);
        document.getElementById('routeTime').textContent = routeData.totalTime.toFixed(1);
    }
}

window.onload = function () {
    console.log('[Map] 页面加载完成');
    loadMap();
};
