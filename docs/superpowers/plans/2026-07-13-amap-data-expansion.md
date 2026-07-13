# 高德API数据扩展 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 使用高德地图API获取菏泽市真实美食门店和景点POI数据，扩展现有数据集并更新全链路数据文件。

**Architecture:** 新建 `expand_data.py` 脚本，按9个区县多关键词搜索Amap API、去重、追加到TXT文件；更新 `gen_web_json.py` 支持可选address字段；然后依次运行 `recalc_roads.py` → `gen_web_json.py` 刷新全链路。

**Tech Stack:** Python 3 + urllib (stdlib) + json

## Global Constraints

- 向后兼容：现有63条美食和20个景点数据保持不变
- 新美食ID从164开始，新景点ID从21开始
- 美食新增address字段，空字段用 `-` 占位
- 请求间隔0.3秒，失败重试2次
- 坐标距离<200m且名称相似度>0.7视为重复

---

### Task 1: 创建 expand_data.py 数据扩展脚本

**Files:**
- Create: `HezeFoodSystem/tools/expand_data.py`

**Interfaces:**
- Produces: 追加行到 `data/food.txt` 和 `data/spot.txt`
- Consumes: 读取现有 `data/food.txt`, `data/spot.txt`, `config/amap_config.txt`

- [ ] **Step 1: 创建脚本框架**

```python
"""
高德地图数据扩展脚本
按区县+分类搜索美食和景点POI，去重后追加到TXT数据文件
使用: python tools/expand_data.py
"""
import json
import math
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
            if not line or line.startswith("#"): continue
            if "=" in line:
                k, v = line.split("=", 1)
                config[k.strip()] = v.strip()
    return config

def amap_request(url, retries=2):
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url)
            req.add_header("User-Agent", "Mozilla/5.0")
            with urllib.request.urlopen(req, timeout=10) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except Exception as e:
            if attempt < retries:
                time.sleep(2)
            else:
                print(f"  Request failed after {retries} retries: {e}")
                return None

def haversine(lng1, lat1, lng2, lat2):
    R = 6371000
    dlat = math.radians(lat2 - lat1)
    dlng = math.radians(lng2 - lng1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlng/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))
```

- [ ] **Step 2: 实现已有数据加载与去重匹配函数**

```python
def load_existing_names(filepath):
    names = set()
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                parts = [p.strip() for p in line.split("|")]
                if len(parts) >= 2:
                    names.add(parts[1])
    return names

def load_existing_food_locations(filepath):
    """返回 [(name, lng, lat), ...]"""
    items = []
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                parts = [p.strip() for p in line.split("|")]
                if len(parts) >= 4:
                    try:
                        items.append((parts[1], float(parts[2]), float(parts[3])))
                    except ValueError:
                        pass
    return items

def load_existing_spot_locations(filepath):
    items = []
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                parts = [p.strip() for p in line.split("|")]
                if len(parts) >= 6:
                    try:
                        items.append((parts[1], float(parts[4]), float(parts[5])))
                    except ValueError:
                        pass
    return items

def name_similarity(a, b):
    """简单Jaccard相似度"""
    sa = set(a)
    sb = set(b)
    if not sa or not sb: return 0
    return len(sa & sb) / len(sa | sb)

def is_duplicate(name, lng, lat, existing_names, existing_locations):
    if name in existing_names:
        return True
    for ename, elng, elat in existing_locations:
        dist = haversine(lng, lat, elng, elat)
        if dist < 200 and name_similarity(name, ename) > 0.7:
            return True
    return False

def get_max_id(filepath, id_index=0):
    max_id = 0
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                parts = [p.strip() for p in line.split("|")]
                try:
                    mid = int(parts[id_index])
                    if mid > max_id: max_id = mid
                except (ValueError, IndexError):
                    pass
    return max_id
```

- [ ] **Step 3: 实现POI搜索与分类推断**

```python
DISTRICTS = ["菏泽市", "牡丹区", "单县", "曹县", "郓城", "巨野", "东明", "定陶", "成武", "鄄城"]
FOOD_KEYWORDS = ["火锅", "烧烤", "面馆", "川菜", "鲁菜", "小吃", "快餐", "甜品",
                 "饮品", "海鲜", "自助餐", "农家菜", "特色菜", "汤", "糕点"]
SPOT_KEYWORDS = ["公园", "景区", "博物馆", "文化", "寺庙", "展览", "纪念馆"]

def fetch_poi_by_keywords(key, keywords, types, city, page_size=20, max_pages=2):
    all_pois = []
    for kw in keywords:
        for page in range(1, max_pages + 1):
            params = {
                "key": key, "keywords": kw, "city": city,
                "citylimit": "true", "types": types,
                "offset": str(page_size), "page": str(page), "output": "json"
            }
            url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
            data = amap_request(url)
            if not data or data.get("status") != "1":
                break
            pois = data.get("pois", [])
            if not pois:
                break
            all_pois.extend(pois)
            if len(pois) < page_size:
                break
            time.sleep(0.3)
    return all_pois

def infer_food_category(name, type_code):
    if "汤" in name or "粥" in name: return "汤类"
    if any(k in name for k in ["面", "馍", "饼", "包", "饺", "粉"]): return "面食"
    if any(k in name for k in ["鸡", "鸭", "鹅"]): return "正餐"
    if any(k in name for k in ["茶", "奶", "汁", "咖"]): return "饮品"
    if any(k in name for k in ["糕", "酥", "糖", "甜"]): return "甜品"
    if any(k in name for k in ["串", "烤", "羊"]): return "烧烤"
    if any(k in name for k in ["凉", "拌"]): return "凉菜"
    if type_code and type_code.startswith("050"): return "正餐"
    if "火锅" in name: return "正餐"
    return "小吃"

def generate_food_tags(name, category, district):
    tags = []
    tag_map = {"汤类":"汤品","面食":"面食","甜品":"甜品","烧烤":"烧烤","饮品":"饮品","正餐":"聚餐"}
    if category in tag_map: tags.append(tag_map[category])
    for kw, tag in [("羊肉","羊肉"),("牛肉","牛肉"),("鸡","鸡肉"),("鱼","鱼类"),("海鲜","海鲜")]:
        if kw in name: tags.append(tag)
    tags.append("地方特色")
    return tags

def infer_spot_type(type_name):
    if not type_name: return "自然景观"
    if any(k in type_name for k in ["公园","风景","山","湿地","河","湖"]): return "自然景观"
    if any(k in type_name for k in ["博物","古迹","纪念","历史","红色","庙"]): return "历史文化"
    if any(k in type_name for k in ["游乐","乐园","主题"]): return "主题公园"
    return "自然景观"

def parse_location(loc_str):
    if not loc_str: return 0, 0
    parts = loc_str.split(",")
    if len(parts) == 2:
        try: return float(parts[0]), float(parts[1])
        except: pass
    return 0, 0

def safe_float_from_biz(biz_ext, key, default):
    val = default
    if isinstance(biz_ext, dict):
        try: val = float(biz_ext.get(key, default))
        except: pass
    elif isinstance(biz_ext, str):
        try:
            ext_data = json.loads(biz_ext)
            val = float(ext_data.get(key, default))
        except: pass
    if val < 0 or val > 999: val = default
    return val
```

- [ ] **Step 4: 实现数据转换函数（含address字段）**

```python
def convert_new_foods(pois, start_id, existing_names, existing_locs, district):
    foods = []
    next_id = start_id
    for poi in pois:
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0: continue
        name = poi.get("name", "")
        if is_duplicate(name, lng, lat, existing_names, existing_locs):
            continue
        type_code = poi.get("typecode", "")
        category = infer_food_category(name, type_code)
        biz_ext = poi.get("biz_ext", {})
        score = safe_float_from_biz(biz_ext, "rating", 4.0)
        if score < 1.0 or score > 5.0: score = 4.0
        cost = safe_float_from_biz(biz_ext, "cost", 30.0)
        address = poi.get("address", "-").replace("|", " ")
        tags = generate_food_tags(name, category, district)
        foods.append({
            "id": next_id, "name": name, "lng": lng, "lat": lat,
            "price": cost, "score": score, "category": category,
            "address": address, "tags": tags
        })
        existing_names.add(name)
        existing_locs.append((name, lng, lat))
        next_id += 1
    return foods

def convert_new_spots(pois, start_id, existing_names, existing_locs):
    spots = []
    next_id = start_id
    for poi in pois:
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0: continue
        name = poi.get("name", "")
        if is_duplicate(name, lng, lat, existing_names, existing_locs):
            continue
        type_name = poi.get("type", "")
        biz_ext = poi.get("biz_ext", {})
        score = safe_float_from_biz(biz_ext, "rating", 4.0)
        if score < 1.0 or score > 5.0: score = 4.0
        address = poi.get("address", "-").replace("|", " ")
        spots.append({
            "id": next_id, "name": name, "lng": lng, "lat": lat,
            "description": name,
            "address": address,
            "type": infer_spot_type(type_name),
            "ticketInfo": "免费",
            "openingTime": "08:00-18:00",
            "recommendDuration": "1-2小时",
            "bestSeason": "全年",
            "score": score,
            "tags": []
        })
        existing_names.add(name)
        existing_locs.append((name, lng, lat))
        next_id += 1
    return spots

def format_food_line(food):
    tags_str = ",".join(food["tags"])
    return f"{food['id']}|{food['name']}|{food['lng']}|{food['lat']}|{food['price']}|{food['score']}|{food['category']}|{food['address']}|{tags_str}"

def format_spot_line(spot):
    tags_str = "|".join(spot["tags"]) if spot["tags"] else "新增"
    return (f"{spot['id']}|{spot['name']}|{spot['description']}|{spot['address']}|"
            f"{spot['lng']}|{spot['lat']}|{spot['type']}|{spot['ticketInfo']}|"
            f"{spot['openingTime']}|{spot['recommendDuration']}|{spot['bestSeason']}|"
            f"{spot['score']}|{tags_str}")
```

- [ ] **Step 5: 实现主函数**

```python
def main():
    print("=" * 60)
    print("  高德地图POI数据扩展")
    print("=" * 60)

    config = load_config()
    key = config.get("AMAP_KEY") or config.get("API_KEY")
    if not key:
        print("ERROR: API Key not found in config/amap_config.txt")
        return

    print(f"API Key: {key[:8]}...")

    # 加载已有数据
    existing_food_names = load_existing_names(FOOD_TXT)
    existing_spot_names = load_existing_names(SPOT_TXT)
    existing_food_locs = load_existing_food_locations(FOOD_TXT)
    existing_spot_locs = load_existing_spot_locations(SPOT_TXT)
    max_food_id = get_max_id(FOOD_TXT, 0)
    max_spot_id = get_max_id(SPOT_TXT, 0)

    print(f"现有美食: {len(existing_food_names)} 条 (最大ID: {max_food_id})")
    print(f"现有景点: {len(existing_spot_names)} 条 (最大ID: {max_spot_id})")
    print()

    all_new_foods = []
    all_new_spots = []

    for district in DISTRICTS:
        print(f"[District: {district}]")

        # 搜索美食
        print(f"  搜索美食 ({len(FOOD_KEYWORDS)} 关键词)...")
        food_pois = fetch_poi_by_keywords(key, FOOD_KEYWORDS,
            "050000|051000|052000", district, page_size=20, max_pages=2)
        new_foods = convert_new_foods(food_pois, max_food_id + len(all_new_foods) + 1,
                                       existing_food_names, existing_food_locs, district)
        all_new_foods.extend(new_foods)
        print(f"  新增美食: {len(new_foods)}")

        # 搜索景点
        print(f"  搜索景点 ({len(SPOT_KEYWORDS)} 关键词)...")
        spot_pois = fetch_poi_by_keywords(key, SPOT_KEYWORDS,
            "110000|110100|110200", district, page_size=20, max_pages=2)
        new_spots = convert_new_spots(spot_pois, max_spot_id + len(all_new_spots) + 1,
                                       existing_spot_names, existing_spot_locs)
        all_new_spots.extend(new_spots)
        print(f"  新增景点: {len(new_spots)}")
        print()

    print("=" * 60)
    print(f"总计新增美食: {len(all_new_foods)}")
    print(f"总计新增景点: {len(all_new_spots)}")

    # 写入TXT文件
    if all_new_foods:
        with open(FOOD_TXT, "a", encoding="utf-8") as f:
            f.write(f"\n# ========== 高德API扩展数据 ({time.strftime('%Y-%m-%d')}) ==========\n")
            for food in all_new_foods:
                f.write(format_food_line(food) + "\n")
        print(f"已追加到 {FOOD_TXT}")

    if all_new_spots:
        with open(SPOT_TXT, "a", encoding="utf-8") as f:
            f.write(f"\n# ========== 高德API扩展数据 ({time.strftime('%Y-%m-%d')}) ==========\n")
            for spot in all_new_spots:
                f.write(format_spot_line(spot) + "\n")
        print(f"已追加到 {SPOT_TXT}")

    print("完成！下一步: python tools/recalc_roads.py && python tools/gen_web_json.py")

if __name__ == "__main__":
    main()
```

- [ ] **Step 6: 运行验证脚本可执行**

```powershell
python HezeFoodSystem/tools/expand_data.py
```

Expected: 脚本连接到高德API，输出各区县搜索进度，最终打印汇总统计和文件路径。

- [ ] **Step 7: Commit**

```bash
git add HezeFoodSystem/tools/expand_data.py
git commit -m "feat: add expand_data.py for amap POI data expansion"
```

---

### Task 2: 更新 gen_web_json.py 兼容新address字段

**Files:**
- Modify: `HezeFoodSystem/tools/gen_web_json.py:9-26`

**Interfaces:**
- Consumes: `data/food.txt`（新格式含address字段）, `data/spot.txt`
- Produces: `web/data/food.json`（含address字段）, `web/data/spot.json`

- [ ] **Step 1: 更新 parse_food_line 支持可选address**

将现有的 `parse_food_line` 替换为：

```python
def parse_food_line(line):
    """解析美食数据行（支持可选address字段）"""
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

        # 检测是否包含address字段
        # 格式1 (旧): id|name|lng|lat|price|score|category|tags
        # 格式2 (新): id|name|lng|lat|price|score|category|address|tags
        if len(parts) >= 9:
            # 尝试判断第8个字段是address还是tags
            part8 = parts[7]
            # 如果是纯数字或含有逗号（tags格式），则为旧格式
            # 如果含有省市等中文地址特征，则是新格式的address
            is_address = ("区" in part8 or "路" in part8 or "街" in part8 or
                         "市" in part8 or "县" in part8 or part8 == "-")
            if is_address:
                address = part8
                tags_part = parts[8] if len(parts) >= 9 else ""
            else:
                address = "-"
                tags_part = parts[7]
        else:
            address = "-"
            tags_part = parts[7]

        return {
            'id': fid, 'name': name, 'lng': lng, 'lat': lat,
            'price': price, 'score': score, 'category': category,
            'address': address,
            'tags': [t.strip() for t in tags_part.split(',') if t.strip()]
        }
    except (ValueError, IndexError):
        return None
```

- [ ] **Step 2: 运行验证**

```powershell
python HezeFoodSystem/tools/gen_web_json.py
```

Expected: 新旧格式的food.txt行均正确解析，输出"解析美食数据: N 条"。

- [ ] **Step 3: Commit**

```bash
git add HezeFoodSystem/tools/gen_web_json.py
git commit -m "feat: gen_web_json support optional address field in food data"
```

---

### Task 3: 运行 expand_data.py 获取高德API数据

- [ ] **Step 1: 运行扩展脚本**

```powershell
python tools/expand_data.py
```
Workdir: `HezeFoodSystem`

Expected: 逐步输出各区县搜索进度，最后报告新增美食和景点数量。

- [ ] **Step 2: 检查TXT文件末尾追加数据**

```powershell
Get-Content data/food.txt -Tail 20
Get-Content data/spot.txt -Tail 15
```
Workdir: `HezeFoodSystem`

Expected: 可见"高德API扩展数据"注释块及新数据行。

- [ ] **Step 3: Commit**

```bash
git add data/food.txt data/spot.txt
git commit -m "data: append new amap POI data to food and spot txt files"
```

---

### Task 4: 运行 recalc_roads.py 重新计算道路

- [ ] **Step 1: 运行道路重算**

```powershell
python tools/recalc_roads.py
```
Workdir: `HezeFoodSystem`

Expected: 输出新道路数量和文件写入确认。

- [ ] **Step 2: 验证道路数据**

```powershell
$lines = Get-Content data/road.txt | Where-Object { $_ -notmatch '^#' -and $_.Trim() -ne '' }; Write-Host "Total road lines: $($lines.Count)"
```
Workdir: `HezeFoodSystem`

Expected: 道路数量 > 263（比之前更多，因为新增了美食和景点节点）。

- [ ] **Step 3: Commit**

```bash
git add data/road.txt
git commit -m "data: regenerate road connections after data expansion"
```

---

### Task 5: 运行 gen_web_json.py 重新生成JSON

- [ ] **Step 1: 运行JSON生成**

```powershell
python tools/gen_web_json.py
```
Workdir: `HezeFoodSystem`

Expected: 解析新增数据，写入food.json/spot.json/route.json。

- [ ] **Step 2: 检查JSON中新增数据**

```powershell
$foods = Get-Content web/data/food.json | ConvertFrom-Json; Write-Host "Total foods: $($foods.Count)"; $foods | Where-Object { $_.id -ge 164 } | Format-Table id, name, address -AutoSize
```
Workdir: `HezeFoodSystem`

Expected: 能看到新ID(164+)的数据，address字段有实际地址。

- [ ] **Step 3: Commit**

```bash
git add web/data/food.json web/data/spot.json web/data/route.json
git commit -m "data: regenerate web JSON after data expansion"
```

---

### Task 6: 验证web前端数据正确显示

- [ ] **Step 1: 启动本地web服务器**

```powershell
Set-Location HezeFoodSystem\web; python -m http.server 8080
```

手动在浏览器打开 `http://localhost:8080`

- [ ] **Step 2: 手动验证**
  - 美食标记数量增加，新增标记正确显示在地图上
  - 景点标记数量增加，新增景点有正确坐标
  - 点击标记后InfoWindow显示地址、评分等信息
  - 分类筛选功能正常

- [ ] **Step 3: 清理 — 停止web服务器 (Ctrl+C)**

无代码变更，无需commit。
