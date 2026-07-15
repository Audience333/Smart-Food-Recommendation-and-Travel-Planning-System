"""
==========================================================================
高德地图 POI 数据采集脚本 (fetch_amap_data.py)
==========================================================================
【脚本用途】
  从高德地图Web API直接获取菏泽市美食和景点POI，转换为系统JSON格式，
  保存到 web/data/ 目录下供前端直接加载。同时自动生成一条示例推荐路线。

【在数据流水线中的位置】
  本脚本是独立的快速采集方案，直接输出JSON，不经过TXT中间格式：
  高德API -> 本脚本 -> web/data/*.json -> Web端加载展示

  如果需要更丰富的数据加工（地址补全、照片获取、标签生成等），
  建议使用 expand_data.py + fill_addresses.py + fetch_photos.py +
  gen_web_json.py 的完整流水线。

【何时运行】
  - 需要快速生成前端展示数据时
  - 作为紧急数据更新的备选方案
  - 不想运行完整流水线时

【运行方式】
  cd HezeFoodSystem
  python tools/fetch_amap_data.py

【前置条件】
  - config/amap_config.txt 中配置了有效的高德API Key
  - 网络连接正常

【输出】
  - web/data/food.json       - 美食POI JSON数组（ID从101起）
  - web/data/spot.json       - 景点POI JSON数组（ID从1起）
  - web/data/route.json      - 示例推荐路线JSON

【与 expand_data.py 的区别】
  - 本脚本：直接调API获取 -> 简单转换 -> 输出JSON（轻量流程）
  - expand_data.py：逐区县搜索 -> 去重 -> 丰富标签 -> 写入TXT ->
                     gen_web_json.py -> 输出JSON（完整流程）
==========================================================================
"""

import json
import math
import os
import sys
import urllib.request
import urllib.parse

# ==================== 配置 ====================

# 高德API配置文件路径
CONFIG_PATH = "config/amap_config.txt"
# JSON输出目录
OUTPUT_DIR = "web/data"

def load_config():
    """
    加载高德API Key等配置

    从 config/amap_config.txt 读取键值对配置（==分隔），
    支持 # 开头的注释行和空行

    返回:
        dict: 配置字典
    """
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
    """
    发送HTTP GET请求到高德API，返回JSON解析后的数据

    设置10秒超时和浏览器UA伪装，防止被API服务器拒绝

    参数:
        url (str): 完整的API请求URL

    返回:
        dict | None: API响应的JSON字典，失败返回None
    """
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
    """
    从高德关键字搜索API获取POI列表，支持分页

    【高德API: 关键字搜索 (place/text)】
      - 端点: https://restapi.amap.com/v3/place/text
      - 参数:
          key         - API Key
          keywords    - 搜索关键词（如 "美食" 或 "景点"）
          city        - 城市限定（"菏泽"）
          citylimit   - 严格限制在城市范围内
          types       - POI类型代码，指定搜索的POI类别
          offset      - 每页返回数量（默认25）
          page        - 页码（从1开始）
          output      - 输出格式（json）
      - 响应: {
          "status": "1",
          "count": "200",       // 搜索结果总数
          "pois": [{...}, ...]  // POI列表
        }
      - 说明: 当返回数量 < page_size 时停止翻页

    参数:
        key (str): 高德API Key
        keywords (str): 搜索关键词
        types (str): POI类型代码字符串（如 "050000|051000|052000" 为餐饮类）
        city (str): 搜索城市，默认"菏泽"
        page_size (int): 每页POI数量，默认25
        max_pages (int): 最大翻页数，默认4

    返回:
        list: 展开后的所有POI数据列表
    """
    all_pois = []
    base_url = "https://restapi.amap.com/v3/place/text"

    for page in range(1, max_pages + 1):
        params = {
            "key": key,
            "keywords": keywords,
            "city": city,
            "citylimit": "true",                # 严格限制在指定城市
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
            break                               # 无更多数据，停止翻页

        all_pois.extend(pois)
        print(f"  获取 {len(pois)} 条")

        if len(pois) < page_size:
            break                               # 最后一页，不再翻页

    return all_pois

# ==================== 数据转换 ====================

def parse_location(loc_str):
    """
    解析高德坐标字符串 "经度,纬度" -> (经度, 纬度)

    参数:
        loc_str (str): 坐标字符串，如 "116.397428,39.90923"

    返回:
        tuple: (float经度, float纬度)，解析失败返回(0, 0)
    """
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
    """
    根据POI名称和类型代码推断美食分类

    采用关键词匹配策略：
    - "汤"/"粥" -> 汤类
    - "面"/"馍"/"饼"/"包"/"饺" -> 面食
    - "鸡"/"鸭"/"鹅" -> 正餐
    - "茶"/"奶"/"汁" -> 饮品
    - "糕"/"酥"/"糖" -> 甜品
    - "串"/"烤" -> 烧烤
    - "凉"/"拌" -> 凉菜
    - 高德types以"050"开头 -> 正餐
    - 默认 -> 小吃

    参数:
        name (str): POI名称
        type_code (str): 高德API返回的POI类型代码

    返回:
        str: 分类名称
    """
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
        return "正餐"                            # 高德餐饮大类
    return "小吃"

def generate_food_tags(name, category):
    """
    为美食POI生成标签列表（简化版）

    相比 expand_data.py 的全版标签生成，这里只做轻量级标签推断：
    1. 分类标签：基于分类添加
    2. 食材标签：匹配常见食材关键词
    3. 地域标签：匹配区县名称
    4. 固定标签：添加"地方特色"

    参数:
        name (str): POI名称
        category (str): 美食分类

    返回:
        list: 标签字符串列表
    """
    tags = []
    # 分类映射表
    tag_map = {
        "汤类": "汤品", "面食": "面食", "甜品": "甜品",
        "烧烤": "烧烤", "饮品": "饮品"
    }
    if category in tag_map:
        tags.append(tag_map[category])

    # 食材关键词匹配
    for keyword, tag in [("羊肉", "羊肉"), ("牛肉", "牛肉"), ("鸡", "鸡肉"), ("鱼", "鱼类")]:
        if keyword in name:
            tags.append(tag)

    # 地域标签匹配
    for region in ["单县", "曹县", "郓城", "巨野", "东明", "鄄城", "定陶", "成武", "菏泽", "牡丹"]:
        if region in name:
            tags.append(region + "特色")

    tags.append("地方特色")
    return tags

def infer_spot_type(type_name, type_code):
    """
    根据高德POI类型推断景点分类

    分类规则：
    - 公园/风景/山 -> 自然景观
    - 博物/古迹/纪念/历史 -> 历史文化
    - 宗教/寺 -> 宗教场所
    - 默认 -> 自然景观

    参数:
        type_name (str): 高德API返回的POI类型名称
        type_code (str): 高德API返回的POI类型代码

    返回:
        str: 景点分类名称
    """
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
    """
    将高德美食POI原始数据转换为系统JSON格式

    转换内容包括：
    - 提取并验证坐标
    - 推断美食分类
    - 提取评分（rating）和人均消费（cost）
    - 自动生成标签
    - 分配ID（从101起顺序分配）

    参数:
        pois (list): 高德API返回的美食POI原始列表

    返回:
        list: 转换后的美食数据字典列表
    """
    foods = []
    for i, poi in enumerate(pois):
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0:
            continue                            # 跳过坐标无效的POI

        name = poi.get("name", "")
        type_code = poi.get("typecode", "")
        category = infer_food_category(name, type_code)

        # 解析扩展信息中的评分
        biz_ext = poi.get("biz_ext", {})
        score = 4.0
        if isinstance(biz_ext, dict):
            try:
                score = float(biz_ext.get("rating", "4.0"))
            except:
                pass
        elif isinstance(biz_ext, str):
            # 有时biz_ext是JSON字符串，需要二次解析
            try:
                ext_data = json.loads(biz_ext)
                score = float(ext_data.get("rating", "4.0"))
            except:
                pass

        # 解析扩展信息中的人均消费
        cost = 30
        if isinstance(biz_ext, dict):
            try:
                cost = float(biz_ext.get("cost", "30"))
            except:
                pass

        foods.append({
            "id": i + 101,                      # 美食ID从101开始，避免与景点ID冲突
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
    """
    将高德景点POI原始数据转换为系统JSON格式

    景点有固定模板字段：ticketInfo(免费), openingTime(08:00-18:00)
    实际运营信息需后续手动修正

    参数:
        pois (list): 高德API返回的景点POI原始列表

    返回:
        list: 转换后的景点数据字典列表
    """
    spots = []
    for i, poi in enumerate(pois):
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0:
            continue

        name = poi.get("name", "")
        type_name = poi.get("type", "")
        type_code = poi.get("typecode", "")

        # 提取评分
        biz_ext = poi.get("biz_ext", {})
        score = 4.0
        if isinstance(biz_ext, dict):
            try:
                score = float(biz_ext.get("rating", "4.0"))
            except:
                pass

        spots.append({
            "id": i + 1,                        # 景点ID从1开始
            "name": name,
            "lng": lng,
            "lat": lat,
            "type": infer_spot_type(type_name, type_code),
            "score": score,
            "address": poi.get("address", ""),
            "ticketInfo": "免费",               # 默认值，需后续手动核实
            "openingTime": "08:00-18:00",       # 默认值
            "description": name                 # 默认用名称作为描述
        })

    return spots

# ==================== 路线生成 ====================

def haversine(lng1, lat1, lng2, lat2):
    """
    使用Haversine公式计算球面距离

    参数:
        lng1, lat1 (float): 第一个点的经纬度
        lng2, lat2 (float): 第二个点的经纬度

    返回:
        float: 两点间的球面距离（米）
    """
    R = 6371000                                 # 地球半径（米）
    dlat = math.radians(lat2 - lat1)
    dlng = math.radians(lng2 - lng1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlng/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def generate_route(foods, spots):
    """
    生成一条简单的"景点→美食→美食"推荐路线

    算法：
    1. 选第一个景点作为起点
    2. 从美食列表中选取离起点最近的两个美食
    3. 按 起点→美食1→美食2 串联成路线

    参数:
        foods (list): 美食数据列表
        spots (list): 景点数据列表

    返回:
        dict | None: 路线JSON对象，无数据时返回None
    """
    if not foods or not spots:
        return None

    # 选择第一个景点作为路线起点
    start = spots[0]
    # 计算所有美食到起点的距离，取最近的两个
    distances = []
    for f in foods:
        d = haversine(start["lng"], start["lat"], f["lng"], f["lat"])
        distances.append((d, f))
    distances.sort(key=lambda x: x[0])

    food1 = distances[0][1]
    food2 = distances[1][1] if len(distances) > 1 else distances[0][1]

    # 计算总直线距离
    total_dist = haversine(start["lng"], start["lat"], food1["lng"], food1["lat"]) + \
                 haversine(food1["lng"], food1["lat"], food2["lng"], food2["lat"])

    return {
        "name": f"推荐路线：{start['name']} -> {food1['name']} -> {food2['name']}",
        "found": True,
        "totalDistance": total_dist,
        "totalTime": total_dist / 500,          # 按约30km/h的城市驾车速度估算（米/500≈分钟）
        "waypoints": [
            {"name": start["name"], "lng": start["lng"], "lat": start["lat"], "type": "spot"},
            {"name": food1["name"], "lng": food1["lng"], "lat": food1["lat"], "type": "food"},
            {"name": food2["name"], "lng": food2["lng"], "lat": food2["lat"], "type": "food"}
        ],
        "path": [                               # 路径坐标数组（前端高德地图画线用）
            [start["lng"], start["lat"]],
            [food1["lng"], food1["lat"]],
            [food2["lng"], food2["lat"]]
        ]
    }

# ==================== 主函数 ====================

def main():
    """
    主函数：执行POI数据采集和JSON生成

    流程（共3步）：
    1. 获取美食POI：调用高德API搜索菏泽市餐饮类POI
       - types: "050000|051000|052000"（餐饮、中餐、西餐）
       - 最多4页 × 25条 = 100条
    2. 获取景点POI：调用高德API搜索菏泽市旅游类POI
       - types: "110000|110100|110200"（旅游、风景名胜、公园广场）
       - 最多2页 × 25条 = 50条
    3. 生成推荐路线：基于获取的美食和景点自动规划
    4. 写入JSON文件到 web/data/ 目录
    """
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

    # 第一步：获取美食POI并转换
    print("[1/3] 获取美食数据...")
    food_pois = fetch_poi(key, "美食", "050000|051000|052000", city, page_size=25, max_pages=4)
    foods = convert_food(food_pois)
    print(f"  转换完成: {len(foods)} 条美食")
    print()

    # 第二步：获取景点POI并转换
    print("[2/3] 获取景点数据...")
    spot_pois = fetch_poi(key, "景点", "110000|110100|110200", city, page_size=25, max_pages=2)
    spots = convert_spot(spot_pois)
    print(f"  转换完成: {len(spots)} 条景点")
    print()

    # 第三步：生成推荐路线
    print("[3/3] 生成路线数据...")
    route = generate_route(foods, spots)
    print()

    # 第四步：保存JSON文件（ensure_ascii=False确保中文正常显示）
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
