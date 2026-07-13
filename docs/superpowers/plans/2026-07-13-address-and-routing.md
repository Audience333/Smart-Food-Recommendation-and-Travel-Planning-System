# Address Fill & Real-Time Routing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** Fill all missing POI addresses via Amap reverse geocoding, replace static route with real-time driving/walking path planning, add user-selectable waypoint route panel.

**Architecture:** New Python script fills missing addresses in TXT files; frontend adds route planner panel that calls Amap driving/walking API directly, renders multi-segment paths with toll/distance/time details.

**Tech Stack:** Python 3 (urllib), JavaScript (AMap JS API 2.0), HTML/CSS

## Global Constraints

- 地址补全: 坐标去重，相同坐标只请求一次API
- 请求间隔: 逆地理0.1秒，路径API 0.3秒
- 途经点上限5个
- 驾车距离>500km提示分日出行
- API失败降级到Haversine直线
- 非必要不使用emoji

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `tools/fill_addresses.py` | CREATE | Reverse geocode missing addresses |
| `web/map.js` | MODIFY | Route planner panel logic, driving/walking API |
| `web/index.html` | MODIFY | Route planner panel HTML |
| `web/style.css` | MODIFY | Route planner styles |
| `data/food.txt` | MODIFY | Updated with filled addresses |
| `data/spot.txt` | MODIFY | Updated with filled addresses |
| `web/data/food.json` | REGEN | From updated TXT |
| `web/data/spot.json` | REGEN | From updated TXT |

---

### Task 1: Create fill_addresses.py

**Files:**
- Create: `HezeFoodSystem/tools/fill_addresses.py`

**Interfaces:**
- Produces: Updated food.txt and spot.txt with filled address fields
- Consumes: `config/amap_config.txt`, existing `data/food.txt`, `data/spot.txt`

- [ ] **Step 1: Create the script**

```python
"""
高德逆地理编码地址补全脚本
对缺失地址的POI调用逆地理编码API补全地址
"""
import json
import os
import sys
import time
import urllib.request
import urllib.parse

CONFIG_PATH = "config/amap_config.txt"
FOOD_TXT = "data/food.txt"
SPOT_TXT = "data/spot.txt"


def load_config():
    config = {}
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                k, v = line.split("=", 1)
                config[k.strip()] = v.strip()
    return config


def reverse_geocode(key, lng, lat):
    params = {
        "key": key,
        "location": "{},{}".format(lng, lat),
        "extensions": "base",
        "output": "json"
    }
    url = "https://restapi.amap.com/v3/geocode/regeo?" + urllib.parse.urlencode(params)
    try:
        req = urllib.request.Request(url)
        req.add_header("User-Agent", "Mozilla/5.0")
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            if data.get("status") == "1" and data.get("regeocode"):
                return data["regeocode"].get("formattedAddress", "")
    except Exception as e:
        print("  Geocode failed for {},{}: {}".format(lng, lat, e))
    return None


def parse_food_line(line):
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 8:
        return None
    d = {"raw": line, "parts": parts}
    try:
        d["id"] = int(parts[0])
        d["name"] = parts[1]
        d["lng"] = float(parts[2])
        d["lat"] = float(parts[3])
    except (ValueError, IndexError):
        return None
    return d


def parse_spot_line(line):
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 6:
        return None
    d = {"raw": line, "parts": parts}
    try:
        d["id"] = int(parts[0])
        d["name"] = parts[1]
        d["lng"] = float(parts[4])
        d["lat"] = float(parts[5])
    except (ValueError, IndexError):
        return None
    return d


def get_address_idx_food(parts):
    """Determine which index holds the address in food line parts."""
    if len(parts) < 9:
        return None
    part8 = parts[7]
    is_address = ("区" in part8 or "路" in part8 or "街" in part8 or
                  "市" in part8 or "县" in part8 or part8 == "-")
    if is_address:
        if part8 == "-":
            return 7
        return 7
    return None


def get_address_idx_spot(parts):
    """地址在景点数据中固定为索引3."""
    if len(parts) >= 4:
        addr = parts[3]
        if addr in ("", "-", None):
            return 3
    return None


def fill_addresses(filepath, parse_fn, get_addr_idx_fn, key):
    lines = []
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    addr_cache = {}  # key: "lng,lat" -> address string
    updated = 0
    new_lines = []

    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            new_lines.append(line + "\n" if line else "\n")
            continue

        info = parse_fn(line)
        if not info:
            new_lines.append(line + "\n")
            continue

        addr_idx = get_addr_idx_fn(info["parts"])
        if addr_idx is None or info["parts"][addr_idx] not in ("", "-", None):
            new_lines.append(line + "\n")
            continue

        cache_key = "{:.5f},{:.5f}".format(info["lng"], info["lat"])
        if cache_key in addr_cache:
            addr = addr_cache[cache_key]
        else:
            addr = reverse_geocode(key, info["lng"], info["lat"])
            time.sleep(0.1)
            if addr:
                addr_cache[cache_key] = addr
            else:
                addr_cache[cache_key] = "无法获取"
                addr = "无法获取"

        parts = info["parts"]
        parts[addr_idx] = addr.replace("|", " ")
        new_lines.append("|".join(parts) + "\n")
        updated += 1
        if updated % 10 == 0:
            print("  Updated {} entries...".format(updated))

    if updated > 0:
        with open(filepath, "w", encoding="utf-8") as f:
            f.writelines(new_lines)

    return updated


def main():
    print("=" * 50)
    print("  高德逆地理编码地址补全")
    print("=" * 50)

    config = load_config()
    key = config.get("AMAP_KEY") or config.get("API_KEY")
    if not key:
        print("ERROR: API Key not found")
        return

    print("API Key: {}...".format(key[:8]))

    # Count missing first
    for fpath, pfn, gfn, label in [
        (FOOD_TXT, parse_food_line, get_address_idx_food, "美食"),
        (SPOT_TXT, parse_spot_line, get_address_idx_spot, "景点")
    ]:
        missing = 0
        with open(fpath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                info = pfn(line)
                if not info: continue
                idx = gfn(info["parts"])
                if idx is not None and info["parts"][idx] in ("", "-", None):
                    missing += 1
        print("{}缺失地址: {} 条".format(label, missing))

    print()
    for fpath, pfn, gfn, label in [
        (FOOD_TXT, parse_food_line, get_address_idx_food, "美食"),
        (SPOT_TXT, parse_spot_line, get_address_idx_spot, "景点")
    ]:
        print("[{}] 补全地址...".format(label))
        n = fill_addresses(fpath, pfn, gfn, key)
        print("  完成: 更新 {} 条".format(n))

    print()
    print("完成！运行 gen_web_json.py 更新前端数据")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Verify syntax**

```powershell
python -c "import py_compile; py_compile.compile('tools/fill_addresses.py', doraise=True); print('OK')"
```
Workdir: `HezeFoodSystem`

- [ ] **Step 3: Run the script**

```powershell
python tools/fill_addresses.py
```
Workdir: `HezeFoodSystem`
Timeout: 300000ms

Expected: Shows count of missing addresses, fills them via API, writes updated TXT files.

- [ ] **Step 4: Verify addresses filled**

```powershell
python -c "import sys; sys.path.insert(0,'tools'); from gen_web_json import parse_food_line; lines=[l.strip() for l in open('data/food.txt','r',encoding='utf-8') if l.strip() and not l.startswith('#')]; foods=[parse_food_line(l) for l in lines]; missing=[f for f in foods if f and f.get('address','-')=='-']; print('Still missing:', len(missing)); from gen_web_json import parse_spot_line; sl=[parse_spot_line(l.strip()) for l in open('data/spot.txt','r',encoding='utf-8') if l.strip() and not l.startswith('#')]; sm=[s for s in sl if s and s.get('address','-')=='-' and s.get('address')!='']; print('Spots missing:', len(sm))"
```
Workdir: `HezeFoodSystem`
Expected: Still missing: 0, Spots missing: 0

- [ ] **Step 5: Regenerate JSON**

```powershell
python tools/gen_web_json.py
```
Workdir: `HezeFoodSystem`

- [ ] **Step 6: Commit**

```bash
git add tools/fill_addresses.py data/food.txt data/spot.txt web/data/food.json web/data/spot.json web/data/route.json
git commit -m "feat: fill all missing POI addresses via amap reverse geocoding"
```

---

### Task 2: Add route planner panel HTML and CSS

**Files:**
- Modify: `HezeFoodSystem/web/index.html`
- Modify: `HezeFoodSystem/web/style.css`

- [ ] **Step 1: Add route planner panel HTML**

Insert BEFORE the "推荐路线" panel in index.html (to replace it):

```html
            <div class="panel">
                <h3>路线规划</h3>
                <div class="route-planner">
                    <div class="route-mode">
                        <span class="route-mode-btn active" data-mode="driving">驾车</span>
                        <span class="route-mode-btn" data-mode="walking">步行</span>
                    </div>
                    <div class="route-select">
                        <label class="route-select-label">起点</label>
                        <select id="routeStart" class="route-select-input">
                            <option value="">-- 选择起点 --</option>
                        </select>
                    </div>
                    <div class="route-select">
                        <label class="route-select-label">途经点 <span style="font-size:11px;color:#999;">(最多5个)</span></label>
                        <div id="routeWaypoints" class="route-waypoints"></div>
                        <button id="btnAddWaypoint" class="route-add-btn">+ 添加途经点</button>
                    </div>
                    <div class="route-select">
                        <label class="route-select-label">终点</label>
                        <select id="routeEnd" class="route-select-input">
                            <option value="">-- 选择终点 --</option>
                        </select>
                    </div>
                    <div class="route-actions">
                        <button id="btnPlanRoute" class="route-plan-btn">规划路线</button>
                        <button id="btnClearRoute" class="route-clear-btn">清空</button>
                    </div>
                </div>
                <div id="routeDetail" class="route-detail" style="display:none;">
                    <div id="routeSummary" class="route-summary"></div>
                    <div id="routeSegments" class="route-segments"></div>
                </div>
            </div>
```

- [ ] **Step 2: Add route planner CSS**

Append to style.css:

```css
/* Route planner */
.route-planner { margin-bottom: 8px; }
.route-mode { display: flex; gap: 6px; margin-bottom: 10px; }
.route-mode-btn {
    padding: 6px 16px; border: 1px solid #ddd; border-radius: 16px; font-size: 13px;
    cursor: pointer; transition: all 0.2s; background: #f5f5f5; user-select: none;
}
.route-mode-btn:hover { border-color: #d32f2f; color: #d32f2f; }
.route-mode-btn.active { background: #d32f2f; color: white; border-color: #d32f2f; }
.route-select { margin-bottom: 10px; }
.route-select-label { font-size: 13px; color: #666; display: block; margin-bottom: 4px; }
.route-select-input {
    width: 100%; padding: 7px 10px; border: 1px solid #ddd; border-radius: 6px;
    font-size: 13px; outline: none; box-sizing: border-box;
}
.route-select-input:focus { border-color: #d32f2f; }
.route-waypoints { display: flex; flex-direction: column; gap: 4px; margin-bottom: 6px; }
.route-waypoint-item {
    display: flex; align-items: center; gap: 6px; padding: 6px 8px;
    background: #fafafa; border: 1px solid #eee; border-radius: 6px; font-size: 13px;
}
.route-waypoint-item .wp-name { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.route-waypoint-item .wp-del { color: #d32f2f; cursor: pointer; font-weight: bold; padding: 0 4px; }
.route-waypoint-item .wp-move { color: #999; cursor: pointer; padding: 0 2px; font-size: 11px; }
.route-add-btn {
    width: 100%; padding: 6px; background: #f0f0f0; border: 1px dashed #ccc;
    border-radius: 6px; font-size: 13px; color: #666; cursor: pointer;
}
.route-add-btn:hover { background: #fff3e0; border-color: #ff9800; }
.route-actions { display: flex; gap: 8px; margin-top: 8px; }
.route-plan-btn {
    flex: 1; padding: 8px; background: #d32f2f; color: white; border: none;
    border-radius: 6px; font-size: 14px; cursor: pointer; font-weight: bold;
}
.route-plan-btn:hover { background: #b71c1c; }
.route-clear-btn {
    padding: 8px 16px; background: #f5f5f5; border: 1px solid #ddd; border-radius: 6px;
    font-size: 14px; cursor: pointer;
}
.route-clear-btn:hover { background: #eee; }
.route-detail { margin-top: 10px; }
.route-summary {
    background: #fff3e0; padding: 10px; border-radius: 6px;
    border-left: 4px solid #ff9800; margin-bottom: 8px;
}
.route-summary-item { font-size: 13px; color: #333; margin-bottom: 4px; }
.route-summary-item strong { color: #e65100; }
.route-segments { max-height: 250px; overflow-y: auto; }
.route-segment {
    padding: 8px 10px; border-bottom: 1px solid #eee; font-size: 13px;
}
.route-segment-header { color: #d32f2f; font-weight: bold; margin-bottom: 3px; }
.route-segment-detail { color: #666; font-size: 12px; }
.route-segment-toll { color: #f44336; font-size: 12px; }
.route-loading { text-align: center; padding: 20px; color: #999; }
.route-error { text-align: center; padding: 20px; color: #d32f2f; }
```

- [ ] **Step 3: Commit**

```bash
git add web/index.html web/style.css
git commit -m "feat: add route planner panel UI"
```

---

### Task 3: Implement route planning logic in map.js

**Files:**
- Modify: `HezeFoodSystem/web/map.js`

**Interfaces:**
- Consumes: foodData, spotData, AMAP_KEY from globals; route mode/start/end/waypoints from DOM; Amap driving/walking API
- Produces: Multi-segment polyline on map + route detail panel

- [ ] **Step 1: Add global variables for route state**

After `var toggleRouteVisible = true;` add:

```js
var currentRoutePolylines = [];
var currentRouteMarkers = [];
var routeWaypoints = [];  // [{id, name, lng, lat, type}]
var routeMode = 'driving';
```

- [ ] **Step 2: Add `populateSelectOptions()` function**

After `loadData()` line (before the `if (map)` block), add:

```js
        populateSelectOptions();
```

And add the function after `updateStats()`:

```js
function populateSelectOptions() {
    var allPOIs = [];
    foodData.forEach(function(f) { allPOIs.push({name: f.name, id: f.id, type: 'food', category: f.category, lng: f.lng, lat: f.lat}); });
    spotData.forEach(function(s) { allPOIs.push({name: s.name, id: s.id, type: 'spot', category: s.type, lng: s.lng, lat: s.lat}); });
    allPOIs.sort(function(a,b) { return a.name.localeCompare(b.name); });

    var optionHtml = '<option value="">-- 选择POI --</option>';
    allPOIs.forEach(function(p) {
        optionHtml += '<option value="' + p.type + '_' + p.id + '">' + p.name + ' (' + p.category + ')</option>';
    });

    var startSel = document.getElementById('routeStart');
    var endSel = document.getElementById('routeEnd');
    if (startSel) startSel.innerHTML = optionHtml;
    if (endSel) endSel.innerHTML = optionHtml;
}
```

- [ ] **Step 3: Add `initRoutePlanner()` function**

```js
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
            routeWaypoints.push({ id: 0, name: '点击选择', type: 'food', lng: 0, lat: 0 });
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

    var wpContainer = document.getElementById('routeWaypoints');
    if (wpContainer) {
        wpContainer.addEventListener('click', function(e) {
            var target = e.target;
            if (target.classList.contains('wp-del')) {
                var idx = parseInt(target.dataset.index);
                routeWaypoints.splice(idx, 1);
                renderWaypointList();
            } else if (target.classList.contains('wp-up')) {
                var idx2 = parseInt(target.dataset.index);
                if (idx2 > 0) {
                    var tmp = routeWaypoints[idx2 - 1];
                    routeWaypoints[idx2 - 1] = routeWaypoints[idx2];
                    routeWaypoints[idx2] = tmp;
                    renderWaypointList();
                }
            } else if (target.classList.contains('wp-down')) {
                var idx3 = parseInt(target.dataset.index);
                if (idx3 < routeWaypoints.length - 1) {
                    var tmp2 = routeWaypoints[idx3 + 1];
                    routeWaypoints[idx3 + 1] = routeWaypoints[idx3];
                    routeWaypoints[idx3] = tmp2;
                    renderWaypointList();
                }
            } else if (target.classList.contains('wp-name')) {
                var idx4 = parseInt(target.dataset.index);
                routeWaypoints[idx4].name = '已点击';
                // Notify user to select from map
            }
        });
    }
}

function renderWaypointList() {
    var container = document.getElementById('routeWaypoints');
    if (!container) return;
    var html = '';
    routeWaypoints.forEach(function(wp, i) {
        html += '<div class="route-waypoint-item">' +
            '<span class="wp-name" data-index="' + i + '">' + (i + 1) + '. ' + wp.name + '</span>' +
            '<span class="wp-up" data-index="' + i + '">▲</span>' +
            '<span class="wp-down" data-index="' + i + '">▼</span>' +
            '<span class="wp-del" data-index="' + i + '">×</span>' +
            '</div>';
    });
    container.innerHTML = html;
}
```

- [ ] **Step 4: Implement core route planning `executeRoutePlan()`**

```js
function executeRoutePlan() {
    var startVal = document.getElementById('routeStart').value;
    var endVal = document.getElementById('routeEnd').value;
    if (!startVal || !endVal) { alert('请选择起点和终点'); return; }

    var startPOI = findPOI(startVal);
    var endPOI = findPOI(endVal);
    if (!startPOI || !endPOI) { alert('无效的POI选择'); return; }

    var allWaypoints = [startPOI].concat(routeWaypoints).concat([endPOI]);
    var validWaypoints = [];
    allWaypoints.forEach(function(wp) {
        if (wp.lng !== 0 && wp.lat !== 0) validWaypoints.push(wp);
    });
    if (validWaypoints.length < 2) { alert('至少需要起点和终点'); return; }

    clearRoute();
    planRouteSegments(validWaypoints, 0);
}

function findPOI(val) {
    if (!val) return null;
    var parts = val.split('_');
    var type = parts[0];
    var id = parseInt(parts[1]);
    if (type === 'food') {
        return foodData.find(function(f) { return f.id === id; });
    } else {
        return spotData.find(function(s) { return s.id === id; });
    }
}

function clearRoute() {
    currentRoutePolylines.forEach(function(p) { p.setMap(null); });
    currentRouteMarkers.forEach(function(m) { m.setMap(null); });
    currentRoutePolylines = [];
    currentRouteMarkers = [];
    document.getElementById('routeDetail').style.display = 'none';
}
```

- [ ] **Step 5: Implement `planRouteSegments()` — recursive call with driving/walking API**

```js
function planRouteSegments(waypoints, index) {
    if (!map) return;
    if (index >= waypoints.length - 1) {
        renderRouteSummary();
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
                        from: from.name,
                        to: to.name,
                        distance: path.distance,
                        duration: path.duration,
                        tolls: path.tolls || 0,
                        polyline: path.steps ? path.steps.map(function(s) { return s.polyline; }).join(';') : '',
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
        planRouteSegments(waypoints, index + 1);
    };
    xhr.onerror = function() {
        renderRouteSegmentFallback(from, to, index);
        planRouteSegments(waypoints, index + 1);
    };
    xhr.timeout = 10000;
    xhr.ontimeout = function() {
        renderRouteSegmentFallback(from, to, index);
        planRouteSegments(waypoints, index + 1);
    };
    xhr.send();
}

function parsePolyline(steps) {
    var allPoints = [];
    steps.forEach(function(step) {
        if (step.polyline) {
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
    var colors = ['#ff5722', '#e64a19', '#d84315', '#bf360c', '#ff9800', '#f57c00'];
    var color = colors[index % colors.length];

    if (segment.steps.length > 0) {
        var path = segment.steps.map(function(p) { return new AMap.LngLat(p[0], p[1]); });
        var polyline = new AMap.Polyline({
            path: path,
            strokeColor: color,
            strokeWeight: 5,
            strokeOpacity: 0.7,
            lineJoin: 'round'
        });
        polyline.setMap(map);
        currentRoutePolylines.push(polyline);
    }

    var midIdx = Math.floor(segment.steps.length / 2);
    if (segment.steps.length > 0) {
        var midPt = segment.steps[midIdx];
        var label = new AMap.Marker({
            position: new AMap.LngLat(midPt[0], midPt[1]),
            content: '<div style="background:' + color + ';color:white;padding:2px 8px;border-radius:10px;font-size:11px;white-space:nowrap;">' +
                (segment.distance / 1000).toFixed(1) + 'km / ' + Math.round(segment.duration / 60) + 'min' +
                (segment.tolls > 0 ? ' / ¥' + segment.tolls : '') + '</div>',
            offset: new AMap.Pixel(-30, -15),
            zIndex: 150
        });
        label.setMap(map);
        currentRouteMarkers.push(label);
    }

    // Store segment data for summary
    if (!window._routeSegments) window._routeSegments = [];
    window._routeSegments.push(segment);
}

function renderRouteSegmentFallback(from, to, index) {
    var dist = haversine(from.lng, from.lat, to.lng, to.lat);
    var segment = {
        from: from.name,
        to: to.name,
        distance: Math.round(dist),
        duration: Math.round(dist / (routeMode === 'walking' ? 5000 : 40000) * 60),
        tolls: 0,
        polyline: '',
        steps: [[from.lng, from.lat], [to.lng, to.lat]]
    };
    var line = new AMap.Polyline({
        path: [new AMap.LngLat(from.lng, from.lat), new AMap.LngLat(to.lng, to.lat)],
        strokeColor: '#999',
        strokeWeight: 3,
        strokeOpacity: 0.5,
        strokeStyle: 'dashed',
        lineJoin: 'round'
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

    var summaryHtml = '';
    summaryHtml += '<div class="route-summary-item">总距离: <strong>' + (totalDist / 1000).toFixed(1) + ' km</strong></div>';
    summaryHtml += '<div class="route-summary-item">总时间: <strong>' + Math.round(totalTime / 60) + ' 分钟</strong></div>';
    if (totalTolls > 0) summaryHtml += '<div class="route-summary-item">过路费: <strong>¥' + totalTolls + '</strong></div>';
    document.getElementById('routeSummary').innerHTML = summaryHtml;

    var segHtml = '';
    window._routeSegments.forEach(function(s, i) {
        segHtml += '<div class="route-segment">' +
            '<div class="route-segment-header">' + (i + 1) + '. ' + s.from + ' → ' + s.to + '</div>' +
            '<div class="route-segment-detail">距离 ' + (s.distance / 1000).toFixed(1) + 'km / 时间 ' + Math.round(s.duration / 60) + '分钟</div>' +
            (s.tolls > 0 ? '<div class="route-segment-toll">过路费 ¥' + s.tolls + '</div>' : '') +
            '</div>';
    });
    document.getElementById('routeSegments').innerHTML = segHtml;
    document.getElementById('routeDetail').style.display = 'block';
    window._routeSegments = [];

    // Fit view
    if (currentRoutePolylines.length > 0) {
        var allPoints = [];
        currentRoutePolylines.forEach(function(p) {
            var path = p.getPath();
            path.forEach(function(pt) { allPoints.push(pt); });
        });
        if (allPoints.length > 0) {
            map.setFitView(allPoints, false, [80, 80, 80, 380]);
        }
    }
}
```

- [ ] **Step 6: Call initRoutePlanner() in loadData()**

After `initSearch();` in `loadData()`, add:

```js
        initRoutePlanner();
```

- [ ] **Step 7: Verify all new functions present**

```powershell
$content = Get-Content web/map.js -Raw
$funcs = @('populateSelectOptions','initRoutePlanner','executeRoutePlan','findPOI','clearRoute','planRouteSegments','parsePolyline','renderRouteSegment','renderRouteSegmentFallback','haversine','renderRouteSummary','renderWaypointList','currentRoutePolylines','currentRouteMarkers','routeWaypoints','routeMode')
foreach($f in $funcs) { if($content -match $f) { Write-Host "OK: $f" } else { Write-Host "MISSING: $f" } }
```
Workdir: `HezeFoodSystem`

- [ ] **Step 8: Commit**

```bash
git add web/map.js
git commit -m "feat: implement real-time route planning with driving/walking API"
```

---

### Task 4: Remove old route dependency

- [ ] **Step 1: Remove old route loading and rendering**

In `loadData()`, remove the fetch of `data/route.json` and the `showRoute()` call:

Replace:
```js
    loadPromises.push(
        fetch('data/route.json')
            .then(r => r.ok ? r.json() : Promise.reject('HTTP ' + r.status))
            .then(d => { routeData = d; console.log('[Map] 路线:', d.name); })
            .catch(e => console.warn('[Map] route.json:', e))
    );
```

With: (just remove this block entirely)

And change:
```js
    if (map) {
        if (foodData.length > 0) showFoodMarkers();
        if (spotData.length > 0) showSpotMarkers();
        if (routeData) showRoute();
        setTimeout(autoFitView, 500);
    }
```

To:
```js
    if (map) {
        if (foodData.length > 0) showFoodMarkers();
        if (spotData.length > 0) showSpotMarkers();
        setTimeout(autoFitView, 500);
    }
```

- [ ] **Step 2: Remove old routeRoute toggle**

Remove the route toggle checkbox from index.html and the toggleRoute handler from initControls().

- [ ] **Step 3: Commit**

```bash
git add web/map.js web/index.html
git commit -m "refactor: remove static route, replace with dynamic route planner"
```
