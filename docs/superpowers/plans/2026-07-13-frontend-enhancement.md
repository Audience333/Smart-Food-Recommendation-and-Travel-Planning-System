# Frontend Enhancement & Real Routing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix toggle bugs, add search, comprehensive 12-dimension food tags, real-time business hours, use Amap driving API for road distances, remove unnecessary emoji.

**Architecture:** Backend Python pipeline (expand_data → recalc_roads → gen_web_json) generates enriched data with detailed tags and opentime; reworked recalc_roads calls Amap driving API; frontend map.js consumes enriched JSON with toggle fix, search, and real-time open/closed status.

**Tech Stack:** Python 3 (urllib), JavaScript (AMap JS API 2.0), HTML/CSS

## Global Constraints

- 非必要不使用emoji（用CSS形状和文字替代）
- 新增 opentime 字段到 food.txt，空值用 `-` 占位
- 驾车API失败时降级到Haversine距离
- 营业时间解析失败默认视为营业中
- 前端搜索无结果显示"未找到匹配项"

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `tools/expand_data.py` | Modify | Enhanced tag generation (12 dims) + opentime extraction |
| `tools/gen_web_json.py` | Modify | Parse new opentime field from food.txt |
| `tools/recalc_roads.py` | Modify | Integrate Amap driving API for real road distances |
| `web/map.js` | Modify | Toggle fix, search, real-time status, no-emoji markers |
| `web/index.html` | Modify | Search panel HTML, "open only" toggle |
| `web/style.css` | Modify | Search panel styles, status indicator styles |
| `data/food.txt` | Appended | Re-expanded with enhanced tags + opentime |
| `data/spot.txt` | Appended | Re-expanded |

---

### Task 1: 增强 expand_data.py 标签生成和营业时间

**Files:**
- Modify: `HezeFoodSystem/tools/expand_data.py`

**Interfaces:**
- Produces: food.txt lines with format `id|name|lng|lat|price|score|category|address|opentime|tags`
  - `opentime`: from `biz_ext.opentime` or inferred, `-` if unknown
  - `tags`: 20-35 comma-separated tags from all 12 dimensions
- Consumes: `config/amap_config.txt`, existing `data/food.txt`

**Current state:** `generate_food_tags()` returns 3-5 tags. `format_food_line()` writes `address|tags`.

- [ ] **Step 1: Replace `generate_food_tags()` with 12-dimension tag generation**

Replace the entire function with:

```python
TASTE_KEYWORDS = {
    "麻辣": ["麻辣", "辣", "麻婆", "水煮", "冒菜"],
    "酸辣": ["酸辣", "酸菜", "醋", "泡椒"],
    "清淡": ["清淡", "清汤", "白汤", "素", "斋"],
    "酱香": ["酱", "卤", "酱香", "焖"],
    "蒜蓉": ["蒜蓉", "蒜泥", "蒜"],
    "鲜香": ["鲜", "原汤", "三鲜", "菌汤", "骨汤"],
    "五香": ["五香", "卤", "酱"],
    "甜口": ["甜", "糖", "蜜", "拔丝"],
    "孜然": ["孜然", "烧烤", "烤"],
    "咖喱": ["咖喱", "咖"],
    "鲜嫩": ["嫩", "滑"],
    "酥脆": ["酥", "脆", "炸", "煎"],
    "软糯": ["糯", "软", "黏"],
    "筋道": ["筋道", "弹", "拉面", "板面"],
}

INGREDIENT_KEYWORDS = {
    "羊肉": ["羊肉", "羊杂", "羊"],
    "牛肉": ["牛肉", "牛杂", "牛"],
    "猪肉": ["猪肉", "猪", "肘子", "排骨", "粉肚", "焖子"],
    "鸡肉": ["鸡", "烧鸡", "熏鸡"],
    "鸭肉": ["鸭", "烤鸭"],
    "鱼肉": ["鱼", "鲤", "烤鱼"],
    "海鲜": ["海鲜", "虾", "蟹", "贝", "鱿"],
    "蔬菜": ["菜", "蔬", "素"],
    "豆制品": ["豆腐", "豆", "腐", "豆皮", "豆干"],
    "面食": ["面", "饼", "馍", "包", "饺", "馓"],
    "菌菇": ["菇", "菌", "蘑菇"],
    "内脏": ["肚", "肠", "肝", "腰", "杂"],
}

COOKING_KEYWORDS = {
    "烤": ["烤", "烧", "烧烤"],
    "涮": ["涮", "火锅"],
    "炒": ["炒", "爆"],
    "炖": ["炖", "熬", "煲"],
    "蒸": ["蒸", "烝"],
    "炸": ["炸", "酥"],
    "卤": ["卤", "酱"],
    "拌": ["拌", "凉", "调"],
    "煮": ["煮", "汤", "面", "粉"],
}

CUISINE_KEYWORDS = {
    "鲁菜": ["鲁菜", "山东", "孔府"],
    "川菜": ["川菜", "四川", "重庆", "麻辣", "火锅", "串串"],
    "粤菜": ["粤菜", "广东", "烧腊", "点心"],
    "湘菜": ["湘菜", "湖南", "剁椒"],
    "清真": ["清真", "回", "清真寺", "穆斯林"],
    "东北菜": ["东北", "铁锅炖", "锅包肉"],
}

SCENE_KEYWORDS = ["堂食", "外卖", "快餐", "聚餐", "一人食", "宴请", "团餐"]
FACILITY_KEYWORDS = ["WiFi", "停车方便", "包厢", "露天座位", "空调", "儿童座椅"]
HONOR_KEYWORDS = ["老字号", "非遗美食", "中华名吃", "地方名吃", "网红打卡", "必吃榜", "百年老店", "口碑店"]
PEOPLE_KEYWORDS = ["亲子", "情侣约会", "朋友聚会", "商务宴请", "独自用餐", "家庭聚餐"]
FEATURE_KEYWORDS = ["现点现做", "秘制配方", "限量供应", "季节限定", "配酒推荐"]


def generate_food_tags(name, category, district, score, price, type_code, opentime):
    tags = []

    # 1. 口味口感
    for taste, keywords in TASTE_KEYWORDS.items():
        if any(k in name for k in keywords):
            tags.append(taste)

    # 2. 食材主料
    for ingredient, keywords in INGREDIENT_KEYWORDS.items():
        if any(k in name for k in keywords):
            tags.append(ingredient)

    # 3. 烹饪方式
    for cooking, keywords in COOKING_KEYWORDS.items():
        if any(k in name for k in keywords):
            tags.append(cooking)

    # 4. 菜系流派
    cuisine_found = False
    for cuisine, keywords in CUISINE_KEYWORDS.items():
        if any(k in name for k in keywords):
            tags.append(cuisine)
            cuisine_found = True
    if not cuisine_found:
        if any(k in name for k in ["羊肉", "烧鸡", "壮馍", "罐子", "胡辣", "焖子"]):
            tags.append("鲁菜")

    # 5. 用餐时段
    if any(k in name for k in ["早", "豆浆", "油条", "煎饼"]):
        tags.append("早餐")
    elif any(k in name for k in ["火锅", "烧烤", "宵夜", "夜"]):
        tags.append("宵夜")
    elif category in ["甜品", "饮品"]:
        tags.append("下午茶")
    else:
        tags.append("午餐")
        tags.append("晚餐")

    if any(k in name for k in ["早餐", "早点"]):
        tags.append("早餐")
    if any(k in name for k in ["烧烤", "烤串", "烤全羊"]):
        tags.append("宵夜")

    # 6. 用餐方式/场景
    if any(k in name for k in ["火锅", "烤全羊", "宴", "聚餐"]) or price > 80:
        tags.append("聚餐")
    if any(k in name for k in ["快", "便当", "简餐"]) or price <= 10:
        tags.append("快餐")
    if price <= 15:
        tags.append("一人食")
    if price > 100:
        tags.append("宴请")
    tags.append("堂食")
    if price < 30:
        tags.append("外卖")

    # 7. 服务设施 (from type_code)
    if type_code and "0501" in type_code:
        tags.append("包厢")
        tags.append("停车方便")

    # 8. 荣誉标签
    if score >= 4.7:
        tags.append("必吃榜")
        tags.append("口碑店")
    if score >= 4.5:
        tags.append("人气高")
        tags.append("口碑店")
    if any(k in name for k in ["老", "百年", "传统", "正宗"]):
        tags.append("老字号")
    if any(k in name for k in ["单县", "曹县", "菏泽", "郓城", "东明", "巨野", "定陶", "成武", "鄄城"]):
        tags.append("地方名吃")

    # 9. 价格档次
    if price <= 10:
        tags.append("人均10元以下")
    elif price <= 30:
        tags.append("人均10-30元")
    elif price <= 60:
        tags.append("人均30-60元")
    elif price <= 100:
        tags.append("人均60-100元")
    else:
        tags.append("人均100元以上")

    # 10. 门店状态
    if opentime and opentime != "-":
        if "24小时" in opentime or "全天" in opentime:
            tags.append("24小时营业")

    # 11. 适合人群
    if any(k in name for k in ["亲子", "儿童", "游乐园"]):
        tags.append("亲子")
    if "火锅" in name or "烤" in name or "聚餐" in tags:
        tags.append("朋友聚会")
    if price > 80:
        tags.append("商务宴请")
    if any(k in name for k in ["约会", "浪漫", "西餐", "牛排"]):
        tags.append("情侣约会")
    if price <= 20:
        tags.append("独自用餐")
    if "火" in name or price > 50:
        tags.append("家庭聚餐")

    # 12. 特色标记
    if "秘制" in name or "秘" in name:
        tags.append("秘制配方")
    if "限定" in name or "季节" in name:
        tags.append("季节限定")

    if district != "菏泽市":
        tags.append(district + "特色")

    # 去重（保持顺序）
    seen = set()
    unique_tags = []
    for t in tags:
        if t not in seen:
            seen.add(t)
            unique_tags.append(t)

    return unique_tags
```

- [ ] **Step 2: Add `extract_opentime()` function**

```python
def extract_opentime(biz_ext, category):
    if isinstance(biz_ext, dict):
        ot = biz_ext.get("opentime", "")
        if ot and ot.strip() and ot.strip() != "[]" and ot.strip() != "null":
            return ot.strip().replace("|", " ")
    elif isinstance(biz_ext, str):
        try:
            ext_data = json.loads(biz_ext)
            ot = ext_data.get("opentime", "")
            if ot and ot.strip():
                return ot.strip().replace("|", " ")
        except:
            pass

    # 推断默认营业时间
    defaults = {
        "汤类": "06:00-14:00,17:00-21:00",
        "面食": "06:00-14:00,17:00-21:00",
        "小吃": "08:00-21:00",
        "正餐": "10:00-14:00,17:00-22:00",
        "烧烤": "17:00-02:00",
        "甜品": "09:00-21:00",
        "饮品": "09:00-21:00",
        "凉菜": "10:00-21:00",
    }
    return defaults.get(category, "09:00-21:00")
```

- [ ] **Step 3: Update `convert_new_foods()` to use new tag function and opentime**

Replace the line `tags = generate_food_tags(name, category, district)` with:

```python
        opentime = extract_opentime(biz_ext, category)
        tags = generate_food_tags(name, category, district, score, cost, type_code, opentime)
```

Change the candidates dict to include `opentime`:

```python
        candidates.append({
            "name": name, "lng": lng, "lat": lat,
            "price": cost, "score": score, "category": category,
            "address": address, "opentime": opentime, "tags": tags
        })
```

- [ ] **Step 4: Update `format_food_line()` to include opentime**

```python
def format_food_line(food):
    tags_str = ",".join(food["tags"])
    opentime = food.get("opentime", "-")
    return (f"{food['id']}|{food['name']}|{food['lng']}|{food['lat']}|"
            f"{food['price']}|{food['score']}|{food['category']}|"
            f"{food['address']}|{opentime}|{tags_str}")
```

- [ ] **Step 5: Verify syntax**

Run: `python -c "import py_compile; py_compile.compile('tools/expand_data.py', doraise=True); print('OK')"`
Workdir: `HezeFoodSystem`

- [ ] **Step 6: Commit**

```bash
git add tools/expand_data.py
git commit -m "feat: add 12-dimension tags and opentime extraction to expand_data"
```

---

### Task 2: 更新 gen_web_json.py 解析opentime字段

**Files:**
- Modify: `HezeFoodSystem/tools/gen_web_json.py`

**Interfaces:**
- Consumes: food.txt with new format `id|name|lng|lat|price|score|category|address|opentime|tags`
- Produces: food.json with `opentime` field

- [ ] **Step 1: Update `parse_food_line()` to extract opentime**

Replace the parse_food_line function with:

```python
def parse_food_line(line):
    """解析美食数据行（支持address+opentime字段）"""
    parts = [p.strip() for p in line.split('|')]
    if len(parts) < 8:
        return None
    try:
        fid = int(parts[0])
        name = parts[1]
        lng = float(parts[2])
        lat = float(parts[3])
        price = float(parts[4])
        score = float(parts[5])
        category = parts[6]

        # 格式1 (旧): id|name|lng|lat|price|score|category|tags
        # 格式2 (address): id|name|lng|lat|price|score|category|address|tags
        # 格式3 (address+opentime): id|name|lng|lat|price|score|category|address|opentime|tags

        address = "-"
        opentime = "-"
        tags_part = ""

        if len(parts) >= 9:
            part8 = parts[7]
            is_address = ("区" in part8 or "路" in part8 or "街" in part8 or
                         "市" in part8 or "县" in part8 or part8 == "-")
            if is_address:
                address = part8
                if len(parts) >= 10:
                    # 尝试判断 parts[8] 是 opentime 还是 tags
                    part9 = parts[8]
                    is_time = (":" in part9 or part9 == "-" or
                              "全天" in part9 or "24小时" in part9 or
                              any(c.isdigit() for c in part9))
                    if is_time:
                        opentime = part9
                        tags_part = parts[9] if len(parts) >= 10 else ""
                    else:
                        tags_part = part9
                else:
                    tags_part = parts[8] if len(parts) >= 9 else ""
            else:
                tags_part = parts[7]
        else:
            tags_part = parts[7]

        return {
            'id': fid, 'name': name, 'lng': lng, 'lat': lat,
            'price': price, 'score': score, 'category': category,
            'address': address,
            'opentime': opentime,
            'tags': [t.strip() for t in tags_part.split(',') if t.strip()]
        }
    except (ValueError, IndexError):
        return None
```

- [ ] **Step 2: Run verification**

```powershell
python tools/gen_web_json.py
```
Workdir: `HezeFoodSystem`
Expected: "解析美食数据: 288 条" (old data still parses correctly)

- [ ] **Step 3: Verify opentime in JSON**

```powershell
python -c "import json; f=json.load(open('web/data/food.json','r',encoding='utf-8')); print('Keys:', list(f[0].keys())); print('Has opentime:', all('opentime' in x for x in f))"
```
Workdir: `HezeFoodSystem`
Expected: Keys include `opentime`, all entries have it.

- [ ] **Step 4: Commit**

```bash
git add tools/gen_web_json.py
git commit -m "feat: parse opentime field in gen_web_json"
```

---

### Task 3: 集成高德驾车路径API到 recalc_roads.py

**Files:**
- Modify: `HezeFoodSystem/tools/recalc_roads.py`

**Interfaces:**
- Consumes: `config/amap_config.txt` (API key), `data/food.txt`, `data/spot.txt`
- Produces: `data/road.txt` with real driving distances where available

- [ ] **Step 1: Add driving API function**

Add after `time_from_distance()`:

```python
def amap_driving(key, origin_lng, origin_lat, dest_lng, dest_lat):
    url = ("https://restapi.amap.com/v3/direction/driving?"
           "origin={},{}&destination={},{}&key={}&strategy=0".format(
               origin_lng, origin_lat, dest_lng, dest_lat, key))
    try:
        req = urllib.request.Request(url)
        req.add_header("User-Agent", "Mozilla/5.0")
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            if data.get("status") == "1" and data.get("route", {}).get("paths"):
                path = data["route"]["paths"][0]
                distance = int(path["distance"])
                duration = int(path["duration"]) // 60  # 秒转分钟
                return distance, max(1, duration)
    except Exception:
        pass
    return None, None
```

- [ ] **Step 2: Add `urllib.request` and `json` imports**

Add at top of recalc_roads.py (after existing imports):

```python
import urllib.request
import json
```

- [ ] **Step 3: Update spot-spot connections to use driving API**

Before spot_connections loop, add key loading:

```python
config = {}
config_path = os.path.join(base_dir, 'config', 'amap_config.txt')
if os.path.exists(config_path):
    with open(config_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"): continue
            if "=" in line:
                k, v = line.split("=", 1)
                config[k.strip()] = v.strip()
amap_key = config.get("AMAP_KEY", "")
```

Then modify the spot_connections loop:

```python
    for a, b in spot_connections:
        if a in all_nodes and b in all_nodes:
            if amap_key:
                dist, t = amap_driving(amap_key,
                    all_nodes[a][1], all_nodes[a][2],
                    all_nodes[b][1], all_nodes[b][2])
                if dist is not None:
                    edges.append((a, b, dist, t))
                    edges.append((b, a, dist, t))
                    time.sleep(0.5)
                    continue
            # Fallback to Haversine
            dist = haversine(all_nodes[a][1], all_nodes[a][2],
                           all_nodes[b][1], all_nodes[b][2])
            t = time_from_distance(dist)
            edges.append((a, b, int(dist), t))
            edges.append((b, a, int(dist), t))
```

- [ ] **Step 4: Update mudan food internal connections with driving API**

Add for new_mudan_foods loop — check distance < 20000m (20km) before calling API:

```python
    for i in range(len(new_mudan_foods)):
        for j in range(i+1, len(new_mudan_foods)):
            a, b = new_mudan_foods[i], new_mudan_foods[j]
            if a in all_nodes and b in all_nodes:
                haversine_dist = haversine(all_nodes[a][1], all_nodes[a][2],
                                          all_nodes[b][1], all_nodes[b][2])
                if haversine_dist < 20000 and amap_key:
                    dist, t = amap_driving(amap_key,
                        all_nodes[a][1], all_nodes[a][2],
                        all_nodes[b][1], all_nodes[b][2])
                    if dist is not None:
                        edges.append((a, b, dist, t))
                        edges.append((b, a, dist, t))
                        time.sleep(0.5)
                        continue
                if haversine_dist < 5000:
                    t = time_from_distance(haversine_dist)
                    edges.append((a, b, int(haversine_dist), t))
                    edges.append((b, a, int(haversine_dist), t))
```

- [ ] **Step 5: Same for new_county_ranges loop**

Wrap the existing Haversine call with driving API try:

```python
    for county, (start, end) in new_county_ranges.items():
        county_foods = [fid for fid in range(start, end + 1) if fid in all_nodes]
        for i in range(len(county_foods)):
            for j in range(i+1, len(county_foods)):
                a, b = county_foods[i], county_foods[j]
                if a in all_nodes and b in all_nodes:
                    haversine_dist = haversine(all_nodes[a][1], all_nodes[a][2],
                                              all_nodes[b][1], all_nodes[b][2])
                    if haversine_dist < 20000 and amap_key:
                        dist, t = amap_driving(amap_key,
                            all_nodes[a][1], all_nodes[a][2],
                            all_nodes[b][1], all_nodes[b][2])
                        if dist is not None:
                            edges.append((a, b, dist, t))
                            edges.append((b, a, dist, t))
                            time.sleep(0.5)
                            continue
                    if haversine_dist < 10000:
                        t = time_from_distance(haversine_dist)
                        edges.append((a, b, int(haversine_dist), t))
                        edges.append((b, a, int(haversine_dist), t))
```

- [ ] **Step 6: Add `import time`**

At top: `import time`

- [ ] **Step 7: Verify syntax**

```powershell
python -c "import py_compile; py_compile.compile('tools/recalc_roads.py', doraise=True); print('OK')"
```
Workdir: `HezeFoodSystem`

- [ ] **Step 8: Commit**

```bash
git add tools/recalc_roads.py
git commit -m "feat: integrate amap driving API for real road distances in recalc_roads"
```

---

### Task 4: 重新运行数据管道生成完整数据

- [ ] **Step 1: Restore baseline data and re-run expand**

```powershell
git checkout 6988001 -- data/food.txt data/spot.txt
python tools/expand_data.py
```
Workdir: `HezeFoodSystem`
Timeout: 600000ms
Expected: 225 new foods + 90 new spots with enhanced tags

- [ ] **Step 2: Verify tag count per new food**

```powershell
python -c "import json, sys; f=json.load(open('web/data/food.json','r',encoding='utf-8')); n=[x for x in f if x['id']>=164]; lens=[len(x['tags']) for x in n]; print('Min tags:', min(lens), 'Max tags:', max(lens), 'Avg:', sum(lens)/len(lens))"
```
Workdir: `HezeFoodSystem`
Expected: Min tags >= 12, Max tags <= 35

- [ ] **Step 3: Run recalc_roads with driving API**

```powershell
python tools/recalc_roads.py
```
Workdir: `HezeFoodSystem`
Timeout: 300000ms
Expected: Road connections generated, some using real driving distances

- [ ] **Step 4: Run gen_web_json**

```powershell
python tools/gen_web_json.py
```
Workdir: `HezeFoodSystem`
Expected: 288 foods + 110 spots with opentime field

- [ ] **Step 5: Commit all data**

```bash
git add data/food.txt data/spot.txt data/road.txt web/data/food.json web/data/spot.json web/data/route.json
git commit -m "data: regenerate with enhanced tags, opentime, and real driving distances"
```

---

### Task 5: 修复前端开关bug并移除不必要的emoji

**Files:**
- Modify: `HezeFoodSystem/web/map.js`
- Modify: `HezeFoodSystem/web/style.css`

- [ ] **Step 1: Add toggle state caching variables**

After `var addressCache = {};` add:

```js
var toggleFoodVisible = true;
var toggleSpotVisible = true;
var toggleRouteVisible = true;
```

- [ ] **Step 2: Update `initControls()` to cache state**

Replace existing toggle handlers:

```js
function initControls() {
    document.getElementById('toggleFood').addEventListener('change', function () {
        toggleFoodVisible = this.checked;
        foodMarkers.forEach(function (m) { m.setVisible(toggleFoodVisible); });
    });
    document.getElementById('toggleSpot').addEventListener('change', function () {
        toggleSpotVisible = this.checked;
        spotMarkers.forEach(function (m) { m.setVisible(toggleSpotVisible); });
    });
    document.getElementById('toggleRoute').addEventListener('change', function () {
        toggleRouteVisible = this.checked;
        if (routePolyline) routePolyline.setVisible(toggleRouteVisible);
        routeMarkers.forEach(function (m) { m.setVisible(toggleRouteVisible); });
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
```

- [ ] **Step 3: Fix `showFoodMarkers()` to respect toggle state**

After `marker.setMap(map)` (line ~217), add:

```js
            marker.setMap(map);
            if (!toggleFoodVisible) marker.setVisible(false);
            foodMarkers.push(marker);
```

- [ ] **Step 4: Fix `showSpotMarkers()` to respect toggle state**

After `marker.setMap(map)` (line ~263), add:

```js
            marker.setMap(map);
            if (!toggleSpotVisible) marker.setVisible(false);
            spotMarkers.push(marker);
```

- [ ] **Step 5: Fix `showRoute()` to respect toggle state**

After `routePolyline.setMap(map)` and before the function ends, add:

```js
        routePolyline.setMap(map);
        if (!toggleRouteVisible) routePolyline.setVisible(false);
```

Also after each route marker `marker.setMap(map)`:

```js
            marker.setMap(map);
            if (!toggleRouteVisible) marker.setVisible(false);
            routeMarkers.push(marker);
```

- [ ] **Step 6: Replace emoji in food markers with CSS text markers**

Replace the `showFoodMarkers()` marker content with:

```js
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
```

- [ ] **Step 7: Replace emoji in spot markers**

Replace `🏛` with text `景`:

```js
            '<span style="transform:rotate(-45deg);font-size:12px;font-weight:bold;">景</span>' +
```

- [ ] **Step 8: Replace emoji in route info window**

Replace `🍴` with `[美食]` and `🏛` with `[景点]` in `showRoute()` and `updateRouteInfo()`.

- [ ] **Step 9: Replace emoji in info windows**

In `showFoodDetail()` and `showSpotDetail()`, replace `🏛` with appropriate text.

- [ ] **Step 10: Commit**

```bash
git add web/map.js web/style.css
git commit -m "fix: toggle state respected on marker rebuild, remove unnecessary emoji"
```

---

### Task 6: 添加搜索功能

**Files:**
- Modify: `HezeFoodSystem/web/index.html`
- Modify: `HezeFoodSystem/web/map.js`
- Modify: `HezeFoodSystem/web/style.css`

- [ ] **Step 1: Add search panel HTML**

Insert after `<div class="panel"><h3>图层控制</h3>...` and before `<div class="panel"><h3>数据统计</h3>...` in index.html:

```html
            <div class="panel">
                <h3>搜索</h3>
                <div class="search-box">
                    <input type="text" id="searchInput" class="search-input" placeholder="搜索美食/景点名称、地址、标签...">
                    <div class="search-type">
                        <span class="search-type-tag active" data-type="all">全部</span>
                        <span class="search-type-tag" data-type="food">美食</span>
                        <span class="search-type-tag" data-type="spot">景点</span>
                    </div>
                </div>
                <div id="searchResults" class="search-results" style="display:none;">
                    <div class="search-results-header">
                        <span id="searchResultCount">0</span> 个结果
                    </div>
                    <div id="searchResultList" class="search-result-list"></div>
                </div>
            </div>
```

- [ ] **Step 2: Add search CSS**

Append to style.css:

```css
.search-box { margin-bottom: 8px; }
.search-input {
    width: 100%; padding: 8px 12px; border: 1px solid #ddd; border-radius: 6px;
    font-size: 14px; outline: none; transition: border-color 0.2s;
}
.search-input:focus { border-color: #d32f2f; box-shadow: 0 0 0 2px rgba(211,47,47,0.1); }
.search-type { display: flex; gap: 6px; margin-top: 8px; }
.search-type-tag {
    padding: 4px 12px; background: #f0f0f0; border-radius: 14px; font-size: 12px;
    cursor: pointer; transition: all 0.2s;
}
.search-type-tag:hover { background: #ffcdd2; }
.search-type-tag.active { background: #d32f2f; color: white; }
.search-results { margin-top: 8px; }
.search-results-header { font-size: 12px; color: #888; margin-bottom: 6px; }
.search-result-list { max-height: 300px; overflow-y: auto; }
.search-result-item {
    padding: 8px 12px; border-bottom: 1px solid #eee; cursor: pointer;
    transition: background 0.15s; display: flex; align-items: center; gap: 8px;
}
.search-result-item:hover { background: #fff3e0; }
.search-result-icon {
    width: 28px; height: 28px; border-radius: 50%; display: flex;
    align-items: center; justify-content: center; font-size: 12px;
    font-weight: bold; color: white; flex-shrink: 0;
}
.search-result-name { font-size: 14px; color: #333; flex: 1; }
.search-result-meta { font-size: 12px; color: #999; }
.search-result-nohits { padding: 20px; text-align: center; color: #999; }
.search-result-tag { padding: 2px 6px; background: #fff3e0; color: #e65100; border-radius: 10px; font-size: 11px; }
.status-dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; margin-right: 4px; }
.status-open { background: #4caf50; }
.status-closing { background: #ff9800; }
.status-closed { background: #ccc; }
```

- [ ] **Step 3: Add search JavaScript functions to map.js**

Add after `updateStats()`:

```js
// ==================== 拼音首字母匹配 ====================

var PINYIN_MAP = null;

function buildPinyinMap() {
    var allItems = foodData.concat(spotData.map(function(s) { s.isSpot = true; return s; }));
    PINYIN_MAP = {};
    allItems.forEach(function(item) {
        var name = item.name;
        var initials = '';
        for (var i = 0; i < name.length; i++) {
            var code = name.charCodeAt(i);
            if (code >= 0x4e00 && code <= 0x9fff) {
                initials += String.fromCharCode(code);
            } else {
                initials += name[i].toLowerCase();
            }
        }
        PINYIN_MAP[item.id + '_' + (item.isSpot ? 's' : 'f')] = initials;
    });
}

function matchesQuery(text, query) {
    if (!query) return true;
    var lowerText = text.toLowerCase();
    var lowerQuery = query.toLowerCase();
    if (lowerText.indexOf(lowerQuery) !== -1) return true;
    // Simple pinyin first-letter match
    if (query.length >= 2) {
        // Check if all query chars exist in text in order (for pinyin initials)
        var ti = 0;
        for (var qi = 0; qi < lowerQuery.length && ti < lowerText.length; qi++) {
            while (ti < lowerText.length && lowerText[ti] !== lowerQuery[qi]) ti++;
            if (ti >= lowerText.length) return false;
            ti++;
        }
        if (qi === lowerQuery.length) return true;
    }
    return false;
}

function performSearch() {
    var query = document.getElementById('searchInput').value.trim();
    var searchType = document.querySelector('.search-type-tag.active').dataset.type;
    var results = [];
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

    if (searchType === 'all' || searchType === 'food') {
        foodData.forEach(function(f) {
            var matchName = matchesQuery(f.name, query);
            var matchAddr = f.address && matchesQuery(f.address, query);
            var matchTags = f.tags && f.tags.some(function(t) { return matchesQuery(t, query); });
            if (matchName || matchAddr || matchTags) {
                results.push({ type: 'food', data: f, match: matchName ? 'name' : (matchAddr ? 'address' : 'tags') });
            }
        });
    }

    if (searchType === 'all' || searchType === 'spot') {
        spotData.forEach(function(s) {
            var matchName = matchesQuery(s.name, query);
            var matchAddr = s.address && matchesQuery(s.address, query);
            if (matchName || matchAddr) {
                results.push({ type: 'spot', data: s, match: matchName ? 'name' : 'address' });
            }
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
            var sub = isFood ? item.category + ' · ' + item.score.toFixed(1) + '分' : item.type + ' · ' + item.score.toFixed(1) + '分';
            var statusHtml = '';
            if (isFood && item.opentime) {
                var status = getOpenStatus(item.opentime);
                statusHtml = '<span class="status-dot ' + status.cls + '" title="' + status.text + '"></span>';
            }
            html += '<div class="search-result-item" data-type="' + r.type + '" data-id="' + item.id + '" data-lng="' + item.lng + '" data-lat="' + item.lat + '">' +
                '<div class="search-result-icon" style="background:' + color + '">' + icon + '</div>' +
                '<div><div class="search-result-name">' + statusHtml + item.name + '</div>' +
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

    // Dim non-matching markers
    if (map) {
        var resultIds = new Set(results.map(function(r) { return r.data.id; }));
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
```

- [ ] **Step 4: Call `initSearch()` in `loadData()`**

After `generateCategoryFilters()` in `loadData()`, add:

```js
        initSearch();
```

- [ ] **Step 5: Commit**

```bash
git add web/index.html web/map.js web/style.css
git commit -m "feat: add search functionality with pinyin and real-time filtering"
```

---

### Task 7: 营业状态实时判断和展示

**Files:**
- Modify: `HezeFoodSystem/web/map.js`
- Modify: `HezeFoodSystem/web/index.html`
- Modify: `HezeFoodSystem/web/style.css`

- [ ] **Step 1: Add `getOpenStatus()` function to map.js**

```js
function parseOpenTime(opentime) {
    if (!opentime || opentime === '-') return null;
    var result = [];
    var segments = opentime.split(',');
    segments.forEach(function(seg) {
        seg = seg.trim();
        var parts = seg.split('-');
        if (parts.length === 2) {
            var start = parts[0].trim().split(':');
            var end = parts[1].trim().split(':');
            if (start.length >= 2 && end.length >= 2) {
                var startMin = parseInt(start[0]) * 60 + parseInt(start[1]);
                var endMin = parseInt(end[0]) * 60 + parseInt(end[1]);
                if (endMin < startMin) endMin += 24 * 60; // overnight
                result.push({ start: startMin, end: endMin });
            }
        }
    });
    return result.length > 0 ? result : null;
}

function getOpenStatus(opentime) {
    if (!opentime || opentime === '-' || opentime === '24小时' || opentime === '全天') {
        return { cls: 'status-open', text: '营业中' };
    }
    var slots = parseOpenTime(opentime);
    if (!slots) return { cls: 'status-open', text: '未知' };

    var now = new Date();
    var currentMin = now.getHours() * 60 + now.getMinutes();

    for (var i = 0; i < slots.length; i++) {
        var s = slots[i];
        // Check current day
        if (currentMin >= s.start && currentMin <= s.end) {
            var remaining = s.end - currentMin;
            if (remaining <= 30) return { cls: 'status-closing', text: '即将打烊(' + remaining + '分钟)' };
            return { cls: 'status-open', text: '营业中' };
        }
        // Check next day rollover
        var nextDayMin = currentMin + 24 * 60;
        if (nextDayMin >= s.start && nextDayMin <= s.end) {
            var remaining = s.end - nextDayMin;
            if (remaining <= 30) return { cls: 'status-closing', text: '即将打烊(' + remaining + '分钟)' };
            return { cls: 'status-open', text: '营业中' };
        }
    }
    return { cls: 'status-closed', text: '已打烊' };
}
```

- [ ] **Step 2: Add "仅显示营业中" toggle to index.html**

In the search panel, add after search type tags:

```html
                    <label class="open-only-toggle" style="margin-top:8px;display:flex;align-items:center;gap:6px;cursor:pointer;font-size:12px;color:#666;">
                        <input type="checkbox" id="toggleOpenOnly">
                        仅显示营业中
                    </label>
```

- [ ] **Step 3: Show open status in food markers**

In `showFoodMarkers()`, after `marker.setMap(map)`, add:

```js
            var status = getOpenStatus(food.opentime);
            if (status.cls === 'status-closed') {
                marker.setOpacity(0.4);
            } else if (status.cls === 'status-closing') {
                var statusBorder = '<div style="position:absolute;top:-4px;left:-4px;right:-4px;bottom:-4px;border:2px solid #ff9800;border-radius:50%;animation:pulse 1s infinite;"></div>';
                marker.setContent(markerContent.replace('box-shadow', statusBorder + 'box-shadow'));
            }
```

- [ ] **Step 4: Handle "open only" toggle in map.js**

```js
document.getElementById('toggleOpenOnly').addEventListener('change', function() {
    var showOnlyOpen = this.checked;
    foodMarkers.forEach(function(m, i) {
        var food = foodData[i];
        if (!food) return;
        var status = getOpenStatus(food.opentime);
        if (showOnlyOpen && status.cls === 'status-closed') {
            m.setVisible(false);
        } else {
            m.setVisible(toggleFoodVisible);
        }
    });
});
```

- [ ] **Step 5: Verify**

Run: Start `python -m http.server 8080` in `web/` and manually check open status indicators.

- [ ] **Step 6: Commit**

```bash
git add web/map.js web/index.html web/style.css
git commit -m "feat: real-time open/closed status with business hours"
```
