/**
 * 菏泽美食推荐与城市漫游规划系统 - 地图可视化模块
 *
 * 功能：
 *   1. 加载高德地图
 *   2. 显示美食 Marker（红色）
 *   3. 显示景点 Marker（蓝色）
 *   4. 显示推荐路线（Polyline）
 *   5. 点击弹窗显示详情
 *   6. 图层控制
 *   7. 分类筛选
 *
 * 数据来源：
 *   - food.json   美食数据（由 C++ MapExporter 生成）
 *   - spot.json   景点数据（由 C++ MapExporter 生成）
 *   - route.json  路线数据（由 C++ Dijkstra 算法生成）
 */

// ==================== 全局变量 ====================

let map = null;                    // 高德地图实例
let foodMarkers = [];              // 美食 Marker 列表
let spotMarkers = [];              // 景点 Marker 列表
let routePolyline = null;          // 路线 Polyline
let routeMarkers = [];             // 路线途经点 Marker
let foodData = [];                 // 美食数据
let spotData = [];                 // 景点数据
let routeData = null;              // 路线数据
let activeCategories = new Set();  // 当前激活的分类

// 菏泽市中心坐标
const HEZE_CENTER = [115.4806, 35.2337];

// ==================== 初始化 ====================

/**
 * 初始化地图
 */
function initMap() {
    // 创建地图实例
    map = new AMap.Map('map', {
        zoom: 12,
        center: HEZE_CENTER,
        mapStyle: 'amap://styles/normal',
        features: ['bg', 'road', 'building', 'point']
    });

    // 添加工具条
    map.addControl(new AMap.ToolBar({
        position: 'LT'
    }));

    // 添加比例尺
    map.addControl(new AMap.Scale());

    console.log('[Map] 地图初始化完成');
}

/**
 * 加载数据
 */
async function loadData() {
    try {
        // 并行加载所有数据
        const [foodResponse, spotResponse, routeResponse] = await Promise.all([
            fetch('data/food.json').catch(() => null),
            fetch('data/spot.json').catch(() => null),
            fetch('data/route.json').catch(() => null)
        ]);

        // 解析美食数据
        if (foodResponse && foodResponse.ok) {
            foodData = await foodResponse.json();
            console.log('[Map] 美食数据加载完成:', foodData.length, '条');
        }

        // 解析景点数据
        if (spotResponse && spotResponse.ok) {
            spotData = await spotResponse.json();
            console.log('[Map] 景点数据加载完成:', spotData.length, '条');
        }

        // 解析路线数据
        if (routeResponse && routeResponse.ok) {
            routeData = await routeResponse.json();
            console.log('[Map] 路线数据加载完成');
        }

        // 更新统计信息
        updateStats();

        // 显示数据
        showFoodMarkers();
        showSpotMarkers();
        showRoute();

        // 生成分类筛选按钮
        generateCategoryFilters();

    } catch (error) {
        console.error('[Map] 数据加载失败:', error);
    }
}

// ==================== Marker 显示 ====================

/**
 * 显示美食 Marker
 */
function showFoodMarkers() {
    // 清除现有 Marker
    foodMarkers.forEach(marker => marker.setMap(null));
    foodMarkers = [];

    foodData.forEach(food => {
        // 检查分类筛选
        if (activeCategories.size > 0 && !activeCategories.has(food.category)) {
            return;
        }

        // 创建 Marker
        const marker = new AMap.Marker({
            position: [food.lng, food.lat],
            title: food.name,
            icon: new AMap.Icon({
                size: new AMap.Size(25, 34),
                image: 'https://webapi.amap.com/theme/v1.3/markers/n/mark_r.png',
                imageSize: new AMap.Size(25, 34)
            }),
            offset: new AMap.Pixel(-12, -34),
            extData: food
        });

        // 点击事件
        marker.on('click', function() {
            showFoodInfoWindow(this);
        });

        marker.setMap(map);
        foodMarkers.push(marker);
    });

    console.log('[Map] 美食 Marker 显示:', foodMarkers.length, '个');
}

/**
 * 显示景点 Marker
 */
function showSpotMarkers() {
    // 清除现有 Marker
    spotMarkers.forEach(marker => marker.setMap(null));
    spotMarkers = [];

    spotData.forEach(spot => {
        // 创建 Marker
        const marker = new AMap.Marker({
            position: [spot.lng, spot.lat],
            title: spot.name,
            icon: new AMap.Icon({
                size: new AMap.Size(25, 34),
                image: 'https://webapi.amap.com/theme/v1.3/markers/n/mark_b.png',
                imageSize: new AMap.Size(25, 34)
            }),
            offset: new AMap.Pixel(-12, -34),
            extData: spot
        });

        // 点击事件
        marker.on('click', function() {
            showSpotInfoWindow(this);
        });

        marker.setMap(map);
        spotMarkers.push(marker);
    });

    console.log('[Map] 景点 Marker 显示:', spotMarkers.length, '个');
}

// ==================== 信息窗口 ====================

/**
 * 显示美食信息窗口
 */
function showFoodInfoWindow(marker) {
    const food = marker.getExtData();

    // 构建标签 HTML
    let tagsHtml = '';
    if (food.tags && food.tags.length > 0) {
        tagsHtml = '<div class="info-tags">' +
            food.tags.map(tag => `<span class="info-tag">${tag}</span>`).join('') +
            '</div>';
    }

    const content = `
        <div class="info-window">
            <h4>🍜 ${food.name}</h4>
            <div class="info-row">
                <span class="info-label">评分：</span>
                <span class="info-value info-score">★ ${food.score}</span>
            </div>
            <div class="info-row">
                <span class="info-label">价格：</span>
                <span class="info-value info-price">¥ ${food.price}</span>
            </div>
            <div class="info-row">
                <span class="info-label">分类：</span>
                <span class="info-value">${food.category}</span>
            </div>
            ${tagsHtml}
        </div>
    `;

    const infoWindow = new AMap.InfoWindow({
        content: content,
        offset: new AMap.Pixel(0, -30),
        autoMove: true
    });

    infoWindow.open(map, marker.getPosition());
}

/**
 * 显示景点信息窗口
 */
function showSpotInfoWindow(marker) {
    const spot = marker.getExtData();

    const content = `
        <div class="info-window">
            <h4>🏛️ ${spot.name}</h4>
            <div class="info-row">
                <span class="info-label">类型：</span>
                <span class="info-value">${spot.type}</span>
            </div>
            <div class="info-row">
                <span class="info-label">评分：</span>
                <span class="info-value info-score">★ ${spot.score}</span>
            </div>
            <div class="info-row">
                <span class="info-label">门票：</span>
                <span class="info-value">${spot.ticketInfo}</span>
            </div>
            <div class="info-row">
                <span class="info-label">时间：</span>
                <span class="info-value">${spot.openingTime}</span>
            </div>
            <div class="info-row">
                <span class="info-label">地址：</span>
                <span class="info-value">${spot.address}</span>
            </div>
            <div class="info-row">
                <span class="info-label">简介：</span>
                <span class="info-value">${spot.description}</span>
            </div>
        </div>
    `;

    const infoWindow = new AMap.InfoWindow({
        content: content,
        offset: new AMap.Pixel(0, -30),
        autoMove: true
    });

    infoWindow.open(map, marker.getPosition());
}

// ==================== 路线显示 ====================

/**
 * 显示推荐路线
 */
function showRoute() {
    if (!routeData || !routeData.found) {
        console.log('[Map] 无可用路线数据');
        return;
    }

    // 清除现有路线
    if (routePolyline) {
        routePolyline.setMap(null);
    }
    routeMarkers.forEach(marker => marker.setMap(null));
    routeMarkers = [];

    // 绘制路线 Polyline
    const path = routeData.path.map(point => new AMap.LngLat(point[0], point[1]));

    routePolyline = new AMap.Polyline({
        path: path,
        strokeColor: '#ff5722',
        strokeWeight: 6,
        strokeOpacity: 0.8,
        strokeStyle: 'solid',
        lineJoin: 'round',
        lineCap: 'round',
        showDir: true
    });

    routePolyline.setMap(map);

    // 添加途经点 Marker
    routeData.waypoints.forEach((point, index) => {
        const isFood = point.type === 'food';
        const isFirst = index === 0;
        const isLast = index === routeData.waypoints.length - 1;

        let label = '';
        if (isFirst) label = '起';
        else if (isLast) label = '终';
        else label = index.toString();

        const marker = new AMap.Marker({
            position: [point.lng, point.lat],
            content: `<div style="
                background: ${isFood ? '#ff5722' : '#2196f3'};
                color: white;
                width: 28px;
                height: 28px;
                border-radius: 50%;
                display: flex;
                align-items: center;
                justify-content: center;
                font-size: 12px;
                font-weight: bold;
                border: 2px solid white;
                box-shadow: 0 2px 4px rgba(0,0,0,0.3);
            ">${label}</div>`,
            offset: new AMap.Pixel(-14, -14),
            zIndex: 100
        });

        marker.setMap(map);
        routeMarkers.push(marker);
    });

    // 调整视图以显示完整路线
    map.setFitView([routePolyline, ...routeMarkers]);

    // 更新路线信息面板
    updateRouteInfo();

    console.log('[Map] 路线显示完成');
}

/**
 * 更新路线信息面板
 */
function updateRouteInfo() {
    const routeInfoDiv = document.getElementById('routeInfo');
    if (!routeData || !routeData.found) {
        routeInfoDiv.innerHTML = '<p class="loading">暂无路线数据</p>';
        return;
    }

    let html = `
        <div class="route-header">
            <h4>${routeData.name}</h4>
            <p>总距离: ${(routeData.totalDistance / 1000).toFixed(1)} 公里</p>
            <p>预计时间: ${routeData.totalTime.toFixed(1)} 分钟</p>
        </div>
    `;

    // 显示途经点
    routeData.waypoints.forEach((point, index) => {
        const isFood = point.type === 'food';
        const icon = isFood ? '🍜' : '🏛️';
        const iconClass = isFood ? 'food' : 'spot';

        html += `
            <div class="waypoint">
                <div class="waypoint-icon ${iconClass}">${icon}</div>
                <span class="waypoint-name">${point.name}</span>
            </div>
        `;

        // 添加箭头（除了最后一个）
        if (index < routeData.waypoints.length - 1) {
            html += '<div class="waypoint-arrow">↓</div>';
        }
    });

    routeInfoDiv.innerHTML = html;
}

// ==================== 图层控制 ====================

/**
 * 初始化图层控制
 */
function initLayerControls() {
    // 美食图层开关
    document.getElementById('toggleFood').addEventListener('change', function() {
        const visible = this.checked;
        foodMarkers.forEach(marker => marker.setVisible(visible));
    });

    // 景点图层开关
    document.getElementById('toggleSpot').addEventListener('change', function() {
        const visible = this.checked;
        spotMarkers.forEach(marker => marker.setVisible(visible));
    });

    // 路线图层开关
    document.getElementById('toggleRoute').addEventListener('change', function() {
        const visible = this.checked;
        if (routePolyline) routePolyline.setVisible(visible);
        routeMarkers.forEach(marker => marker.setVisible(visible));
    });
}

/**
 * 初始化地图控制按钮
 */
function initMapControls() {
    // 放大
    document.getElementById('btnZoomIn').addEventListener('click', function() {
        map.zoomIn();
    });

    // 缩小
    document.getElementById('btnZoomOut').addEventListener('click', function() {
        map.zoomOut();
    });

    // 重置视图
    document.getElementById('btnReset').addEventListener('click', function() {
        map.setZoomAndCenter(12, HEZE_CENTER);
    });
}

// ==================== 分类筛选 ====================

/**
 * 生成分类筛选按钮
 */
function generateCategoryFilters() {
    const container = document.getElementById('categoryFilters');

    // 提取所有分类
    const categories = new Set();
    foodData.forEach(food => {
        if (food.category) categories.add(food.category);
    });

    // 生成按钮
    let html = '<div class="category-tag active" data-category="all">全部</div>';
    categories.forEach(cat => {
        html += `<div class="category-tag" data-category="${cat}">${cat}</div>`;
    });

    container.innerHTML = html;

    // 添加点击事件
    container.querySelectorAll('.category-tag').forEach(tag => {
        tag.addEventListener('click', function() {
            const category = this.dataset.category;

            if (category === 'all') {
                // 全部
                activeCategories.clear();
                container.querySelectorAll('.category-tag').forEach(t => t.classList.remove('active'));
                this.classList.add('active');
            } else {
                // 切换分类
                if (this.classList.contains('active')) {
                    this.classList.remove('active');
                    activeCategories.delete(category);
                } else {
                    this.classList.add('active');
                    activeCategories.add(category);
                }

                // 更新"全部"按钮状态
                const allTag = container.querySelector('[data-category="all"]');
                if (activeCategories.size === 0) {
                    allTag.classList.add('active');
                } else {
                    allTag.classList.remove('active');
                }
            }

            // 重新显示美食 Marker
            showFoodMarkers();
        });
    });
}

// ==================== 统计更新 ====================

/**
 * 更新统计信息
 */
function updateStats() {
    document.getElementById('foodCount').textContent = foodData.length;
    document.getElementById('spotCount').textContent = spotData.length;

    if (routeData && routeData.found) {
        document.getElementById('routeDistance').textContent =
            (routeData.totalDistance / 1000).toFixed(1);
        document.getElementById('routeTime').textContent =
            routeData.totalTime.toFixed(1);
    }
}

// ==================== 主函数 ====================

/**
 * 主函数 - 页面加载完成后执行
 */
window.onload = function() {
    console.log('[Map] 页面加载完成');

    // 初始化地图
    initMap();

    // 初始化控制
    initLayerControls();
    initMapControls();

    // 加载数据
    loadData();
};
