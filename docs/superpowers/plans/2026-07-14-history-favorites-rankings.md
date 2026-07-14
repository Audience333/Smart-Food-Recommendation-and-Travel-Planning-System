# History, Favorites & Rankings — Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development to implement. Single task modifies 3 files.

**Goal:** Add floating undo/redo history, favorites CRUD with localStorage persistence, and 3-tab rankings panel to the web frontend.

**Architecture:** Three JavaScript manager objects (HistoryManager, FavoriteManager, RankingManager) added to map.js; corresponding HTML panels added to index.html; styles in style.css. Pure frontend, no backend changes.

**Tech Stack:** JavaScript (AMap JS API 2.0), HTML, CSS, localStorage

## Global Constraints

- 非必要不使用emoji（奖牌符号除外）
- 收藏数据localStorage key: `heze_favorites`
- 历史记录上限50条
- 排行榜前10名显示完整信息
- 所有操作前端实现，无后端依赖

## File Structure

| File | Action |
|------|--------|
| `web/map.js` | Add HistoryManager, FavoriteManager, RankingManager + UI integration |
| `web/index.html` | Add favorites panel, rankings panel, history floating button HTML |
| `web/style.css` | Add all new panel styles, history dropdown, floating buttons |

---

### Task 1: Implement all three features

**Files:**
- Modify: `HezeFoodSystem/web/map.js`
- Modify: `HezeFoodSystem/web/index.html`
- Modify: `HezeFoodSystem/web/style.css`

Read all three files first, then implement.

#### A. ADD TO map.js — HistoryManager

Add after the global variables section (after `var routeMode = 'driving';`):

```js
var HistoryManager = {
    stack: [],
    pointer: -1,
    maxSize: 50,

    push: function(type, data) {
        // Discard redo items beyond pointer
        if (this.pointer < this.stack.length - 1) {
            this.stack = this.stack.slice(0, this.pointer + 1);
        }
        this.stack.push({ type: type, data: data, time: new Date().toLocaleTimeString('zh-CN', {hour:'2-digit',minute:'2-digit'}) });
        if (this.stack.length > this.maxSize) this.stack.shift();
        this.pointer = this.stack.length - 1;
        this.updateUI();
    },

    undo: function() {
        if (!this.canUndo()) return;
        var item = this.stack[this.pointer];
        this.pointer--;
        this._applyReverse(item);
        this.updateUI();
    },

    redo: function() {
        if (!this.canRedo()) return;
        this.pointer++;
        var item = this.stack[this.pointer];
        this._applyForward(item);
        this.updateUI();
    },

    canUndo: function() { return this.pointer >= 0; },
    canRedo: function() { return this.pointer < this.stack.length - 1; },

    _applyReverse: function(item) {
        switch(item.type) {
            case 'filter':
                if (item.data.prevCategories) {
                    activeCategories = new Set(item.data.prevCategories);
                    showFoodMarkers();
                    generateCategoryFilters();
                }
                break;
            case 'search':
                document.getElementById('searchInput').value = '';
                document.getElementById('searchResults').style.display = 'none';
                if (map) { foodMarkers.forEach(function(m) { m.setOpacity(1); }); }
                break;
            case 'fav_add':
                FavoriteManager.remove(item.data.id, item.data.type);
                break;
            case 'fav_del':
                FavoriteManager.add({ id: item.data.id, type: item.data.type, name: item.data.name });
                break;
            case 'route':
                clearRoute();
                autoFitView();
                break;
            case 'view':
                // Close any open info window
                if (map) map.clearInfoWindow();
                break;
        }
        RankingManager.refresh();
    },

    _applyForward: function(item) {
        switch(item.type) {
            case 'filter':
                if (item.data.newCategories) {
                    activeCategories = new Set(item.data.newCategories);
                    showFoodMarkers();
                    generateCategoryFilters();
                }
                break;
            case 'fav_add':
                FavoriteManager.add({ id: item.data.id, type: item.data.type, name: item.data.name });
                break;
            case 'fav_del':
                FavoriteManager.remove(item.data.id, item.data.type);
                break;
            case 'view':
                if (map && item.data.lng && item.data.lat) {
                    map.setZoomAndCenter(16, [item.data.lng, item.data.lat]);
                }
                break;
        }
        RankingManager.refresh();
    },

    updateUI: function() {
        var undoBtn = document.getElementById('btnHistoryUndo');
        var redoBtn = document.getElementById('btnHistoryRedo');
        if (undoBtn) undoBtn.style.opacity = this.canUndo() ? '1' : '0.4';
        if (redoBtn) redoBtn.style.opacity = this.canRedo() ? '1' : '0.4';
        this._renderDropdown();
    },

    _renderDropdown: function() {
        var list = document.getElementById('historyDropdownList');
        if (!list) return;
        var html = '';
        for (var i = this.stack.length - 1; i >= 0; i--) {
            var item = this.stack[i];
            var marker = i === this.pointer ? '&#9654;' : (i > this.pointer ? '&#9679;' : '&#9675;');
            var cls = i === this.pointer ? 'history-current' : (i > this.pointer ? 'history-done' : 'history-undone');
            var label = this._typeLabel(item.type);
            html += '<div class="history-item ' + cls + '" data-index="' + i + '">' +
                '<span class="history-marker">' + marker + '</span>' +
                '<span class="history-time">' + item.time + '</span>' +
                '<span class="history-label">' + label + '</span></div>';
        }
        list.innerHTML = html || '<div class="history-empty">暂无操作记录</div>';
    },

    _typeLabel: function(type) {
        var map = { view: '查看详情', filter: '筛选分类', search: '搜索', fav_add: '收藏', fav_del: '取消收藏', route: '路线规划' };
        return map[type] || type;
    },

    clear: function() {
        this.stack = [];
        this.pointer = -1;
        this.updateUI();
    }
};
```

Bind history dropdown events in `initControls()` or a new `initHistory()` function:

```js
function initHistory() {
    document.getElementById('btnHistoryUndo').addEventListener('click', function() { HistoryManager.undo(); });
    document.getElementById('btnHistoryRedo').addEventListener('click', function() { HistoryManager.redo(); });
    document.getElementById('btnHistoryClear').addEventListener('click', function() { HistoryManager.clear(); });

    var dropdown = document.getElementById('historyDropdown');
    var trigger = document.getElementById('btnHistoryDropdown');
    if (trigger && dropdown) {
        trigger.addEventListener('mouseenter', function() { dropdown.style.display = 'block'; });
        dropdown.addEventListener('mouseleave', function() { dropdown.style.display = 'none'; });
        trigger.addEventListener('click', function() {
            dropdown.style.display = dropdown.style.display === 'block' ? 'none' : 'block';
        });
        // Click on history item to jump to state
        dropdown.addEventListener('click', function(e) {
            var item = e.target.closest('.history-item');
            if (!item) return;
            var idx = parseInt(item.dataset.index);
            // Jump: undo/redo as needed
            while (HistoryManager.pointer > idx) HistoryManager.undo();
            while (HistoryManager.pointer < idx) HistoryManager.redo();
        });
    }
}
```

Call `initHistory()` from `loadData()` after other init calls.

**Hook history into existing operations:**

In `generateCategoryFilters()`, when a category filter changes, record:
```js
HistoryManager.push('filter', { prevCategories: Array.from(activeCategories), newCategories: Array.from(activeCategories) });
```
Wait - the push should happen after the category change. Add this inside the click handler for category tags, after updating activeCategories but before calling showFoodMarkers:

```js
// Record previous state before changing
var prevCats = Array.from(activeCategories);
// ... existing category toggle logic ...
HistoryManager.push('filter', { prevCategories: prevCats, newCategories: Array.from(activeCategories) });
```

In `performSearch()`, when a search executes:
```js
if (query) { HistoryManager.push('search', { query: query, searchType: searchType }); }
```

In `showFoodDetail()` and `showSpotDetail()`:
```js
HistoryManager.push('view', { poiId: food.id, poiType: 'food', lng: food.lng, lat: food.lat, poiName: food.name });
```

In `executeRoutePlan()`, after successful route plan:
```js
HistoryManager.push('route', { startId: startPOI.id, endId: endPOI.id, waypoints: routeWaypoints.length, mode: routeMode });
```

**FavoriteManager integration with history:** When adding/removing favorites, record in history after the action:
```js
// In FavoriteManager.add():
HistoryManager.push('fav_add', { id: poi.id, type: poi.type, name: poi.name });
// In FavoriteManager.remove():
HistoryManager.push('fav_del', { id: item.id, type: item.type, name: item.name });
```

#### B. ADD TO map.js — FavoriteManager

```js
var FavoriteManager = {
    items: [],
    storageKey: 'heze_favorites',

    init: function() {
        this.load();
        this.updateAllStarIcons();
    },

    add: function(poi) {
        if (this.isFavorite(poi.id, poi.type)) return;
        this.items.push({ id: poi.id, type: poi.type, name: poi.name, addedAt: Date.now() });
        this.save();
        this.updateStarIcon(poi.id, poi.type, true);
        RankingManager.refresh();
    },

    remove: function(id, type) {
        this.items = this.items.filter(function(f) { return !(f.id === id && f.type === type); });
        this.save();
        this.updateStarIcon(id, type, false);
        RankingManager.refresh();
    },

    isFavorite: function(id, type) {
        return this.items.some(function(f) { return f.id === id && f.type === type; });
    },

    getAll: function() {
        var self = this;
        return this.items.map(function(f) {
            var data = f.type === 'food' ? foodData.find(function(x) { return x.id === f.id; }) :
                       spotData.find(function(x) { return x.id === f.id; });
            return data ? Object.assign({}, data, { favType: f.type, addedAt: f.addedAt }) : null;
        }).filter(function(x) { return x !== null; });
    },

    getCount: function() { return this.items.length; },

    save: function() {
        var data = this.items.map(function(f) { return { id: f.id, type: f.type, addedAt: f.addedAt }; });
        try { localStorage.setItem(this.storageKey, JSON.stringify(data)); } catch(e) {}
    },

    load: function() {
        try {
            var raw = localStorage.getItem(this.storageKey);
            if (raw) { this.items = JSON.parse(raw); }
        } catch(e) { this.items = []; }
    },

    updateStarIcon: function(id, type, isFav) {
        // Update all star elements for this POI
        document.querySelectorAll('.fav-star[data-poi-id="' + id + '"][data-poi-type="' + type + '"]').forEach(function(el) {
            el.innerHTML = isFav ? '&#9733;' : '&#9734;';
            el.classList.toggle('faved', isFav);
        });
    },

    updateAllStarIcons: function() {
        var self = this;
        document.querySelectorAll('.fav-star').forEach(function(el) {
            var id = parseInt(el.dataset.poiId);
            var type = el.dataset.poiType;
            var isFav = self.isFavorite(id, type);
            el.innerHTML = isFav ? '&#9733;' : '&#9734;';
            el.classList.toggle('faved', isFav);
        });
    },

    renderPanel: function() {
        var container = document.getElementById('favoritesList');
        if (!container) return;
        var favs = this.getAll();
        var countEl = document.getElementById('favoritesCount');
        if (countEl) countEl.textContent = '(' + favs.length + ')';

        if (favs.length === 0) {
            container.innerHTML = '<div class="favorites-empty">暂无收藏，点击标记详情的 [收藏] 按钮添加</div>';
        } else {
            var html = '';
            favs.forEach(function(f) {
                var isFood = f.favType === 'food';
                var color = isFood ? (CATEGORY_COLORS[f.category] || CATEGORY_COLORS.default) : '#1565c0';
                var icon = isFood ? f.name.charAt(0) : '景';
                var sub = isFood ? f.score.toFixed(1) + '分 / Y' + f.price : f.type;
                html += '<div class="favorite-item" data-lng="' + f.lng + '" data-lat="' + f.lat + '" data-id="' + f.id + '" data-type="' + f.favType + '">' +
                    '<div class="favorite-icon" style="background:' + color + '">' + icon + '</div>' +
                    '<div class="favorite-info">' +
                    '<div class="favorite-name">' + f.name + '</div>' +
                    '<div class="favorite-meta">' + sub + '</div>' +
                    '</div>' +
                    '<span class="fav-star faved" data-poi-id="' + f.id + '" data-poi-type="' + f.favType + '">&#9733;</span>' +
                    '</div>';
            });
            container.innerHTML = html;

            // Click on favorite item -> navigate
            container.querySelectorAll('.favorite-item').forEach(function(el) {
                el.addEventListener('click', function(e) {
                    if (e.target.classList.contains('fav-star')) return; // handled separately
                    var lng = parseFloat(this.dataset.lng);
                    var lat = parseFloat(this.dataset.lat);
                    if (map) { map.setZoomAndCenter(16, [lng, lat]); }
                });
            });

            // Click on star -> remove
            container.querySelectorAll('.fav-star').forEach(function(el) {
                el.addEventListener('click', function(e) {
                    e.stopPropagation();
                    var id = parseInt(this.dataset.poiId);
                    var type = this.dataset.poiType;
                    FavoriteManager.remove(id, type);
                    FavoriteManager.renderPanel();
                    showFoodMarkers();
                    showSpotMarkers();
                });
            });
        }
    }
};
```

**Integrate star into info windows:** In `showFoodDetail()`, add a star button before the `<h4>`:
```js
    var isFav = FavoriteManager.isFavorite(food.id, 'food');
    var starIcon = isFav ? '&#9733;' : '&#9734;';
    var starHtml = '<span class="fav-star ' + (isFav ? 'faved' : '') + '" data-poi-id="' + food.id + '" data-poi-type="food" style="cursor:pointer;float:right;font-size:20px;color:#ff9800;">' + starIcon + '</span>';
```

And in the info window HTML, put `starHtml` before the `<h4>`.

After creating the info window, bind the star click:
```js
    setTimeout(function() {
        var starEl = document.querySelector('.fav-star[data-poi-id="' + food.id + '"][data-poi-type="food"]');
        if (starEl) {
            starEl.addEventListener('click', function(e) {
                e.stopPropagation();
                if (FavoriteManager.isFavorite(food.id, 'food')) {
                    FavoriteManager.remove(food.id, 'food');
                } else {
                    FavoriteManager.add({ id: food.id, type: 'food', name: food.name });
                }
                FavoriteManager.renderPanel();
                showFoodMarkers();
                showSpotMarkers();
            });
        }
    }, 100);
```

Same pattern for `showSpotDetail()`.

**Favorite marker borders:** In `showFoodMarkers()`, after creating the marker, check if favorite and add gold border:
```js
            if (FavoriteManager.isFavorite(food.id, 'food')) {
                markerContent = markerContent.replace('border:2px solid white', 'border:3px solid #ffd700');
            }
```

Do the same for spot markers with `border-color:#ffd700`.

Call `FavoriteManager.init()` in `loadData()` after data is loaded and before markers are shown.

#### C. ADD TO map.js — RankingManager

```js
var RankingManager = {
    currentTab: 'popular',

    getPopularRanking: function() {
        return foodData.slice().sort(function(a, b) { return b.score - a.score; });
    },

    getValueRanking: function() {
        return foodData.slice().sort(function(a, b) {
            var va = a.score / Math.log(a.price + 1);
            var vb = b.score / Math.log(b.price + 1);
            return vb - va;
        });
    },

    getFavoriteRanking: function() {
        var favIds = {};
        FavoriteManager.items.forEach(function(f) {
            if (f.type === 'food') favIds[f.id] = (favIds[f.id] || 0) + 1;
        });
        return foodData.slice().sort(function(a, b) {
            var fa = favIds[a.id] || 0;
            var fb = favIds[b.id] || 0;
            if (fb !== fa) return fb - fa;
            return b.score - a.score;
        });
    },

    refresh: function() {
        this.renderPanel();
    },

    renderPanel: function() {
        var list = document.getElementById('rankingList');
        if (!list) return;
        var ranking;
        switch(this.currentTab) {
            case 'popular': ranking = this.getPopularRanking(); break;
            case 'value': ranking = this.getValueRanking(); break;
            case 'favorites': ranking = this.getFavoriteRanking(); break;
            default: ranking = this.getPopularRanking();
        }

        var medals = ['&#129351;', '&#129352;', '&#129353;'];
        var html = '';
        var display = ranking.slice(0, 10);
        display.forEach(function(f, i) {
            var rank = i + 1;
            var rankStr = i < 3 ? '<span style="font-size:18px;">' + medals[i] + '</span>' : '<span class="ranking-num">' + rank + '</span>';
            var isFav = FavoriteManager.isFavorite(f.id, 'food');
            var starHtml = isFav ? ' &#9733;' : '';
            html += '<div class="ranking-item" data-lng="' + f.lng + '" data-lat="' + f.lat + '" data-id="' + f.id + '">' +
                '<div class="ranking-rank">' + rankStr + '</div>' +
                '<div class="ranking-info">' +
                '<div class="ranking-name">' + f.name + starHtml + '</div>' +
                '<div class="ranking-meta">' + f.score.toFixed(1) + '分 / Y' + f.price + ' / ' + f.category + '</div>' +
                '</div></div>';
        });
        list.innerHTML = html;

        list.querySelectorAll('.ranking-item').forEach(function(el) {
            el.addEventListener('click', function() {
                var lng = parseFloat(this.dataset.lng);
                var lat = parseFloat(this.dataset.lat);
                if (map) { map.setZoomAndCenter(16, [lng, lat]); }
            });
        });
    },

    init: function() {
        var self = this;
        document.querySelectorAll('.ranking-tab').forEach(function(tab) {
            tab.addEventListener('click', function() {
                document.querySelectorAll('.ranking-tab').forEach(function(t) { t.classList.remove('active'); });
                this.classList.add('active');
                self.currentTab = this.dataset.tab;
                self.renderPanel();
            });
        });
    }
};
```

Call `RankingManager.init()` and `RankingManager.renderPanel()` from `loadData()`.

#### D. ADD TO index.html

Add history floating buttons inside `.map-container`, right after `<div id="map"></div>` and before `.map-controls`:

```html
            <div class="history-controls">
                <button id="btnHistoryUndo" class="history-btn" title="撤销">&#x2190; 撤销</button>
                <button id="btnHistoryRedo" class="history-btn" title="重做">重做 &#x2192;</button>
                <button id="btnHistoryDropdown" class="history-btn">&#x25BC; 历史</button>
                <div id="historyDropdown" class="history-dropdown" style="display:none;">
                    <div class="history-dropdown-header">
                        <span>操作历史</span>
                        <span id="btnHistoryClear" class="history-clear">清空</span>
                    </div>
                    <div id="historyDropdownList" class="history-dropdown-list"></div>
                </div>
            </div>
```

Add favorites panel (before rankings panel):

```html
            <div class="panel">
                <h3>收藏夹 <span id="favoritesCount">(0)</span></h3>
                <div id="favoritesList" class="favorites-list">
                    <div class="favorites-empty">暂无收藏</div>
                </div>
            </div>
```

Add rankings panel:

```html
            <div class="panel">
                <h3>排行榜</h3>
                <div class="ranking-tabs">
                    <span class="ranking-tab active" data-tab="popular">人气</span>
                    <span class="ranking-tab" data-tab="value">性价比</span>
                    <span class="ranking-tab" data-tab="favorites">收藏</span>
                </div>
                <div id="rankingList" class="ranking-list"></div>
            </div>
```

#### E. ADD TO style.css

```css
/* History floating controls */
.history-controls {
    position: absolute; top: 15px; left: 15px; z-index: 60;
    display: flex; gap: 4px; align-items: flex-start;
}
.history-btn {
    padding: 6px 10px; background: white; border: 1px solid #ccc;
    border-radius: 4px; font-size: 12px; cursor: pointer;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1); transition: all 0.15s;
}
.history-btn:hover { background: #f5f5f5; border-color: #999; }
.history-dropdown {
    position: absolute; top: 100%; left: 0; margin-top: 4px;
    width: 240px; background: white; border-radius: 6px;
    box-shadow: 0 4px 16px rgba(0,0,0,0.15); z-index: 70; overflow: hidden;
}
.history-dropdown-header {
    display: flex; justify-content: space-between; align-items: center;
    padding: 8px 12px; font-size: 13px; font-weight: bold; border-bottom: 1px solid #eee;
}
.history-clear { color: #d32f2f; cursor: pointer; font-size: 12px; font-weight: normal; }
.history-dropdown-list { max-height: 300px; overflow-y: auto; }
.history-item {
    padding: 6px 12px; font-size: 12px; border-bottom: 1px solid #f5f5f5;
    cursor: pointer; display: flex; gap: 6px; align-items: center;
}
.history-item:hover { background: #fafafa; }
.history-current { background: #fff3e0; font-weight: bold; }
.history-done { color: #666; }
.history-undone { color: #ccc; }
.history-marker { width: 16px; text-align: center; flex-shrink: 0; }
.history-time { color: #999; width: 48px; flex-shrink: 0; }
.history-label { flex: 1; }
.history-empty { padding: 20px; text-align: center; color: #999; font-size: 13px; }

/* Favorites */
.favorites-list { max-height: 300px; overflow-y: auto; }
.favorites-empty { padding: 20px; text-align: center; color: #999; font-size: 13px; }
.favorite-item {
    display: flex; align-items: center; gap: 8px; padding: 8px 10px;
    border-bottom: 1px solid #eee; cursor: pointer; transition: background 0.15s;
}
.favorite-item:hover { background: #fff3e0; }
.favorite-icon {
    width: 28px; height: 28px; border-radius: 50%; display: flex;
    align-items: center; justify-content: center; font-size: 12px;
    font-weight: bold; color: white; flex-shrink: 0;
}
.favorite-info { flex: 1; min-width: 0; }
.favorite-name { font-size: 14px; color: #333; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.favorite-meta { font-size: 12px; color: #999; }
.fav-star { cursor: pointer; font-size: 18px; color: #ccc; transition: color 0.15s; flex-shrink: 0; }
.fav-star.faved { color: #ff9800; }
.fav-star:hover { color: #ff9800; }

/* Rankings */
.ranking-tabs { display: flex; gap: 4px; margin-bottom: 10px; }
.ranking-tab {
    padding: 5px 14px; background: #f0f0f0; border-radius: 14px; font-size: 12px;
    cursor: pointer; transition: all 0.2s; user-select: none;
}
.ranking-tab:hover { background: #ffcdd2; }
.ranking-tab.active { background: #d32f2f; color: white; }
.ranking-list { max-height: 400px; overflow-y: auto; }
.ranking-item {
    display: flex; align-items: center; gap: 8px; padding: 8px 10px;
    border-bottom: 1px solid #eee; cursor: pointer; transition: background 0.15s;
}
.ranking-item:hover { background: #fff3e0; }
.ranking-item:first-child .ranking-rank { font-size: 22px; }
.ranking-rank { width: 30px; text-align: center; font-size: 14px; font-weight: bold; color: #d32f2f; flex-shrink: 0; }
.ranking-num { color: #999; font-weight: normal; }
.ranking-info { flex: 1; min-width: 0; }
.ranking-name { font-size: 14px; color: #333; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.ranking-meta { font-size: 12px; color: #999; }
```

#### F. Integration checklist

After all changes:
1. In `loadData()`: after data loads, call `FavoriteManager.init()` then `FavoriteManager.renderPanel()` then `RankingManager.init()` then `RankingManager.renderPanel()`
2. Call `initHistory()` from `loadData()`
3. Update `showFoodMarkers()` to show gold border for favorites
4. Update `showSpotMarkers()` to show gold border for favorites
5. Update `showFoodDetail()` and `showSpotDetail()` to show star button
6. Verify all functions exist with grep checks

Commit with: "feat: add operation history, favorites panel, and rankings"

Return: commit hash plus summary of lines added.