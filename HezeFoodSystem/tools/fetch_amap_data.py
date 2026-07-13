"""
高德地图 POI 数据采集脚本

功能：
  1. 从高德 API 获取菏泽市美食 POI
  2. 从高德 API 获取菏泽市景点 POI
  3. 生成路线数据
  4. 保存为 web/data/ 目录下的 JSON 文件

使用：
  cd HezeFoodSystem
  python tools/fetch_amap_data.py

注意：
  - 需要网络连接
  - API Key 在 config/amap_config.txt 中配置
"""

import json
import math
import os
import sys
import urllib.request
import urllib.parse

# ==================== 配置 ====================

CONFIG_PATH = "config/amap_config.txt"
OUTPUT_DIR = "web/data"

# 读取配置
def load_config():
    config = {}
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                key, value = line.split("=", 1)
                config[key.strip()] = value.strip()
    return config

# ==================== HTTP 请求 ====================

def amap_request(url):
    """发送高德 API 请求"""
    try:
        req = urllib.request.Request(url)
        req.add_header("User-Agent", "Mozilla/5.0")
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except Exception as e:
        print(f"  请求失败: {e}")
        return None

# ==================== POI 获取 ====================

def fetch_poi(key, keywords, types, city="菏泽", page_size=25, max_pages=4):
    """获取 POI 数据"""
    all_pois = []
    base_url = "https://restapi.amap.com/v3/place/text"

    for page in range(1, max_pages + 1):
        params = {
            "key": key,
            "keywords": keywords,
            "city": city,
            "citylimit": "true",
            "types": types,
            "offset": str(page_size),
            "page": str(page),
            "output": "json"
        }
        url = base_url + "?" + urllib.parse.urlencode(params)
        print(f"  请求第 {page} 页...")

        data = amap_request(url)
        if not data or data.get("status") != "1":
            print(f"  API 返回错误: {data}")
            break

        pois = data.get("pois", [])
        if not pois:
            break

        all_pois.extend(pois)
        print(f"  获取 {len(pois)} 条")

        if len(pois) < page_size:
            break

    return all_pois

# ==================== 数据转换 ====================

def parse_location(loc_str):
    """解析坐标 'lng,lat'"""
    if not loc_str:
        return 0, 0
    parts = loc_str.split(",")
    if len(parts) == 2:
        try:
            return float(parts[0]), float(parts[1])
        except:
            pass
    return 0, 0

def infer_food_category(name, type_code):
    """推断美食分类"""
    if "汤" in name or "粥" in name:
        return "汤类"
    if any(k in name for k in ["面", "馍", "饼", "包", "饺"]):
        return "面食"
    if any(k in name for k in ["鸡", "鸭", "鹅"]):
        return "正餐"
    if any(k in name for k in ["茶", "奶", "汁"]):
        return "饮品"
    if any(k in name for k in ["糕", "酥", "糖"]):
        return "甜品"
    if any(k in name for k in ["串", "烤"]):
        return "烧烤"
    if any(k in name for k in ["凉", "拌"]):
        return "凉菜"
    if type_code and type_code.startswith("050"):
        return "正餐"
    return "小吃"

def generate_food_tags(name, category):
    """生成美食标签"""
    tags = []
    tag_map = {
        "汤类": "汤品", "面食": "面食", "甜品": "甜品",
        "烧烤": "烧烤", "饮品": "饮品"
    }
    if category in tag_map:
        tags.append(tag_map[category])

    for keyword, tag in [("羊肉", "羊肉"), ("牛肉", "牛肉"), ("鸡", "鸡肉"), ("鱼", "鱼类")]:
        if keyword in name:
            tags.append(tag)

    for region in ["单县", "曹县", "郓城", "巨野", "东明", "鄄城", "定陶", "成武", "菏泽", "牡丹"]:
        if region in name:
            tags.append(region + "特色")

    tags.append("地方特色")
    return tags

def infer_spot_type(type_name, type_code):
    """推断景点类型"""
    if not type_name:
        return "自然景观"
    if any(k in type_name for k in ["公园", "风景", "山"]):
        return "自然景观"
    if any(k in type_name for k in ["博物", "古迹", "纪念", "历史"]):
        return "历史文化"
    if "宗教" in type_name or "寺" in type_name:
        return "宗教场所"
    return "自然景观"

def convert_food(pois):
    """转换美食 POI 为系统格式"""
    foods = []
    for i, poi in enumerate(pois):
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0:
            continue

        name = poi.get("name", "")
        type_code = poi.get("typecode", "")
        category = infer_food_category(name, type_code)

        # 获取评分
        biz_ext = poi.get("biz_ext", {})
        score = 4.0
        if isinstance(biz_ext, dict):
            try:
                score = float(biz_ext.get("rating", "4.0"))
            except:
                pass
        elif isinstance(biz_ext, str):
            # 有时 biz_ext 是字符串
            try:
                ext_data = json.loads(biz_ext)
                score = float(ext_data.get("rating", "4.0"))
            except:
                pass

        # 获取价格
        cost = 30
        if isinstance(biz_ext, dict):
            try:
                cost = float(biz_ext.get("cost", "30"))
            except:
                pass

        foods.append({
            "id": i + 1,
            "name": name,
            "lng": lng,
            "lat": lat,
            "price": cost,
            "score": score,
            "category": category,
            "tags": generate_food_tags(name, category)
        })

    return foods

def convert_spot(pois):
    """转换景点 POI 为系统格式"""
    spots = []
    for i, poi in enumerate(pois):
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0:
            continue

        name = poi.get("name", "")
        type_name = poi.get("type", "")
        type_code = poi.get("typecode", "")

        biz_ext = poi.get("biz_ext", {})
        score = 4.0
        if isinstance(biz_ext, dict):
            try:
                score = float(biz_ext.get("rating", "4.0"))
            except:
                pass

        spots.append({
            "id": i + 1,
            "name": name,
            "lng": lng,
            "lat": lat,
            "type": infer_spot_type(type_name, type_code),
            "score": score,
            "address": poi.get("address", ""),
            "ticketInfo": "免费",
            "openingTime": "08:00-18:00",
            "description": name
        })

    return spots

# ==================== 路线生成 ====================

def haversine(lng1, lat1, lng2, lat2):
    """Haversine 距离计算"""
    R = 6371000
    dlat = math.radians(lat2 - lat1)
    dlng = math.radians(lng2 - lng1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlng/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def generate_route(foods, spots):
    """生成示例路线：景点 -> 美食 -> 美食"""
    if not foods or not spots:
        return None

    # 选择第一个景点作为起点
    start = spots[0]
    # 选择最近的两个美食
    distances = []
    for f in foods:
        d = haversine(start["lng"], start["lat"], f["lng"], f["lat"])
        distances.append((d, f))
    distances.sort(key=lambda x: x[0])

    food1 = distances[0][1]
    food2 = distances[1][1] if len(distances) > 1 else distances[0][1]

    total_dist = haversine(start["lng"], start["lat"], food1["lng"], food1["lat"]) + \
                 haversine(food1["lng"], food1["lat"], food2["lng"], food2["lat"])

    return {
        "name": f"推荐路线：{start['name']} -> {food1['name']} -> {food2['name']}",
        "found": True,
        "totalDistance": total_dist,
        "totalTime": total_dist / 500,  # 城市驾车 30km/h
        "waypoints": [
            {"name": start["name"], "lng": start["lng"], "lat": start["lat"], "type": "spot"},
            {"name": food1["name"], "lng": food1["lng"], "lat": food1["lat"], "type": "food"},
            {"name": food2["name"], "lng": food2["lng"], "lat": food2["lat"], "type": "food"}
        ],
        "path": [
            [start["lng"], start["lat"]],
            [food1["lng"], food1["lat"]],
            [food2["lng"], food2["lat"]]
        ]
    }

# ==================== 主函数 ====================

def main():
    print("=" * 50)
    print("  高德地图 POI 数据采集")
    print("=" * 50)

    # 加载配置
    config = load_config()
    key = config.get("AMAP_KEY") or config.get("API_KEY")
    city = config.get("CITY", "菏泽")

    if not key:
        print("错误: 未找到 API Key，请检查 config/amap_config.txt")
        return

    print(f"城市: {city}")
    print(f"API Key: {key[:8]}...")
    print()

    # 创建输出目录
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 获取美食 POI
    print("[1/3] 获取美食数据...")
    food_pois = fetch_poi(key, "美食", "050000|051000|052000", city, page_size=25, max_pages=4)
    foods = convert_food(food_pois)
    print(f"  转换完成: {len(foods)} 条美食")
    print()

    # 获取景点 POI
    print("[2/3] 获取景点数据...")
    spot_pois = fetch_poi(key, "景点", "110000|110100|110200", city, page_size=25, max_pages=2)
    spots = convert_spot(spot_pois)
    print(f"  转换完成: {len(spots)} 条景点")
    print()

    # 生成路线
    print("[3/3] 生成路线数据...")
    route = generate_route(foods, spots)
    print()

    # 保存文件
    food_path = os.path.join(OUTPUT_DIR, "food.json")
    spot_path = os.path.join(OUTPUT_DIR, "spot.json")
    route_path = os.path.join(OUTPUT_DIR, "route.json")

    with open(food_path, "w", encoding="utf-8") as f:
        json.dump(foods, f, ensure_ascii=False, indent=2)
    print(f"  保存: {food_path} ({len(foods)} 条)")

    with open(spot_path, "w", encoding="utf-8") as f:
        json.dump(spots, f, ensure_ascii=False, indent=2)
    print(f"  保存: {spot_path} ({len(spots)} 条)")

    if route:
        with open(route_path, "w", encoding="utf-8") as f:
            json.dump(route, f, ensure_ascii=False, indent=2)
        print(f"  保存: {route_path}")

    print()
    print("=" * 50)
    print("  采集完成！")
    print(f"  美食: {len(foods)} 条")
    print(f"  景点: {len(spots)} 条")
    print("  请刷新浏览器查看地图")
    print("=" * 50)

if __name__ == "__main__":
    main()
