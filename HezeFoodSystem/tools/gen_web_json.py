#!/usr/bin/env python3
"""
==========================================================================
Web JSON数据生成脚本 (gen_web_json.py)
==========================================================================
【脚本用途】
  从 data/food.txt 和 data/spot.txt 解析POI数据，转换为JSON格式写入
  web/data/ 目录，供前端Web应用加载渲染。

  同时生成一条示例旅游路线（route.json），包含景点↔美食↔景点的路径规划。

【在数据流水线中的位置】
  expand_data.py(扩展数据) -> fill_addresses.py -> fetch_photos.py ->
  recalc_roads.py -> 本脚本(生成JSON) -> Web端加载展示

  本脚本是数据流水线的最后一步：将TXT中间格式转换为前端可直接使用的JSON。

【何时运行】
  - 数据有任何更新后（新增/修改POI），运行本脚本重新生成JSON
  - 前端页面需要刷新数据时

【运行方式】
  cd HezeFoodSystem
  python tools/gen_web_json.py

【前置条件】
  - 已存在 data/food.txt 和 data/spot.txt（经过完整流水线处理的版本）
  - web/data/ 目录存在（脚本会自动创建）

【输出】
  - web/data/food.json       - 美食POI JSON数组
  - web/data/spot.json       - 景点POI JSON数组
  - web/data/route.json      - 示例旅游路线JSON

【JSON格式说明】
  美食JSON: [
    {"id": 101, "name": "xxx", "lng": 116.39, "lat": 39.91,
     "price": 30, "score": 4.5, "category": "汤类",
     "address": "...", "opentime": "...", "photos": [...], "tags": [...]}, ...
  ]
  景点JSON: [
    {"id": 1, "name": "xxx", "description": "...", "address": "...",
     "lng": 116.39, "lat": 39.91, "type": "自然景观",
     "ticketInfo": "免费", "openingTime": "...", "recommendDuration": "...",
     "bestSeason": "...", "score": 4.5, "photos": [...], "tags": [...]}, ...
  ]
  路线JSON: {
    "name": "路线名称", "found": true,
    "path": [[lng,lat], ...], "waypoints": [...],
    "totalDistance": 12345, "totalTime": 20.5}
==========================================================================
"""
import json
import os
import math

def parse_food_line(line):
    """
    解析美食数据行（支持可选的address、opentime和photos字段）

    美食数据格式兼容多种情况：
    基础: ID|名称|经度|纬度|价格|评分|分类
    扩展: ...|地址|营业时间|照片(;分隔)|标签(,分隔)
    简化: ...|标签

    地址字段启发式识别：检查第8个字段是否包含地址特征词
    （区/路/街/市/县），是则为地址，否则直接视为标签

    参数:
        line (str): 美食数据行字符串

    返回:
        dict | None: 解析成功返回美食数据字典，解析失败返回None
    """
    parts = [p.strip() for p in line.split('|')]
    if len(parts) < 8:
        return None
    try:
        # 固定字段：ID、名称、经度、纬度、价格、评分、分类
        fid = int(parts[0])
        name = parts[1]
        lng = float(parts[2])
        lat = float(parts[3])
        price = float(parts[4])
        score = float(parts[5])
        category = parts[6]

        address = "-"
        opentime = "-"
        tags_part = ""

        # 解析可选字段：根据字段数量推断各字段含义
        if len(parts) >= 9:
            part8 = parts[7]
            # 判断第8个字段是否为地址（含有地址特征词）
            is_address = ("区" in part8 or "路" in part8 or "街" in part8 or
                         "市" in part8 or "县" in part8 or part8 == "-")
            if is_address:
                address = part8
                if len(parts) >= 10:
                    part9 = parts[8]
                    # 判断第9个字段是否为营业时间（包含":"或"-"等时间特征）
                    is_time = (":" in part9 or "-" in part9 or "全天" in part9 or
                              any(c.isdigit() for c in part9))
                    if is_time:
                        opentime = part9
                        # parts[9]可能为照片, parts[10]为标签
                        if len(parts) >= 11:
                            tags_part = parts[10]
                        else:
                            tags_part = parts[9]
                    else:
                        # parts[8]可能为照片, parts[9]为标签
                        if len(parts) >= 10:
                            tags_part = parts[9]
                        else:
                            tags_part = parts[8]
                else:
                    tags_part = parts[8]
            else:
                tags_part = parts[7]
        else:
            tags_part = parts[7]

        # 标签始终取最后一个有效字段，以确保兼容性
        tags_part = parts[-1] if len(parts) >= 1 else ""
        # 照片取倒数第二个字段
        photos_str = parts[-2] if len(parts) >= 2 else '-'
        photos = photos_str.split(';') if photos_str and photos_str != '-' else []

        return {
            'id': fid, 'name': name, 'lng': lng, 'lat': lat,
            'price': price, 'score': score, 'category': category,
            'address': address, 'opentime': opentime,
            'photos': photos,
            'tags': [t.strip() for t in tags_part.split(',') if t.strip()]
        }
    except (ValueError, IndexError):
        return None

def parse_spot_line(line):
    """
    解析景点数据行

    景点数据格式：
    固定字段（前12个）: ID|名称|描述|地址|经度|纬度|类型|票价|营业时间|推荐时长|最佳季节|评分
    剩余字段: 照片(;分隔)|标签(|分隔)
    或直接: 标签(|分隔)

    照片识别策略：遍历剩余字段，若字段包含";"或以"http"开头则视为照片URL列；
    其余字段全部视为标签

    参数:
        line (str): 景点数据行字符串

    返回:
        dict | None: 解析成功返回景点数据字典，解析失败返回None
    """
    parts = [p.strip() for p in line.split('|')]
    if len(parts) < 13:
        return None
    try:
        # 前12个固定字段
        fixed_fields = parts[:12]
        remaining = parts[12:]
        photos_str = '-'
        tag_fields = []
        # 遍历剩余字段，第一个包含";"或"http"的视为照片，其余为标签
        for field in remaining:
            if photos_str == '-' and (';' in field or field.startswith('http')):
                photos_str = field
            else:
                tag_fields.append(field)
        photos = photos_str.split(';') if photos_str and photos_str != '-' else []
        return {
            'id': int(fixed_fields[0]),
            'name': fixed_fields[1],
            'description': fixed_fields[2],
            'address': fixed_fields[3],
            'lng': float(fixed_fields[4]),
            'lat': float(fixed_fields[5]),
            'type': fixed_fields[6],
            'ticketInfo': fixed_fields[7],
            'openingTime': fixed_fields[8],
            'recommendDuration': fixed_fields[9],
            'bestSeason': fixed_fields[10],
            'score': float(fixed_fields[11]),
            'photos': photos,
            'tags': [t.strip() for t in tag_fields if t.strip()]
        }
    except (ValueError, IndexError):
        return None

def parse_file(filepath, parser):
    """
    解析整个数据文件，逐行处理并返回有效数据的列表

    自动跳过空行和#开头的注释行

    参数:
        filepath (str): 数据文件路径
        parser (callable): 行解析函数（parse_food_line 或 parse_spot_line）

    返回:
        list: 解析后的数据字典列表
    """
    data = []
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            item = parser(line)
            if item:
                data.append(item)
    return data

def haversine(lon1, lat1, lon2, lat2):
    """
    使用Haversine公式计算地球表面两点间的大圆距离

    用于计算路线中各途经点之间的实际地理距离

    参数:
        lon1, lat1 (float): 第一个点的经纬度
        lon2, lat2 (float): 第二个点的经纬度

    返回:
        float: 两点间的球面距离（单位：米）
    """
    R = 6371000                                 # 地球平均半径（米）
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlam = math.radians(lon2 - lon1)
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlam/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def main():
    """
    主函数：执行完整的JSON生成流程

    流程：
    1. 确定数据目录和输出目录
    2. 解析 food.txt -> food.json
    3. 解析 spot.txt -> spot.json
    4. 生成示例旅游路线 route.json：
       曹州牡丹园(起点) -> 烧牛肉 -> 胡辣汤 -> 羊肉汤 -> 天香公园(终点)
    5. 输出统计信息和路线详情

    路线生成说明：
    - 选取菏泽市代表性景点和美食串联成一日游路线
    - 使用Haversine公式计算路径总距离
    - 按40km/h市区车速估算总耗时
    """
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, 'data')
    web_data_dir = os.path.join(base_dir, 'web', 'data')
    os.makedirs(web_data_dir, exist_ok=True)

    # 第一步：解析数据文件
    foods = parse_file(os.path.join(data_dir, 'food.txt'), parse_food_line)
    spots = parse_file(os.path.join(data_dir, 'spot.txt'), parse_spot_line)

    print(f"解析美食数据: {len(foods)} 条")
    print(f"解析景点数据: {len(spots)} 条")

    # 第二步：写入 food.json
    food_path = os.path.join(web_data_dir, 'food.json')
    with open(food_path, 'w', encoding='utf-8') as f:
        json.dump(foods, f, ensure_ascii=False, indent=2)
    print(f"已写入: {food_path}")

    # 第三步：写入 spot.json
    spot_path = os.path.join(web_data_dir, 'spot.json')
    with open(spot_path, 'w', encoding='utf-8') as f:
        json.dump(spots, f, ensure_ascii=False, indent=2)
    print(f"已写入: {spot_path}")

    # 第四步：生成示例路线 - "菏泽美食文化一日游路线"
    # 筛选构成路线的特定景点和美食
    route_spots = [s for s in spots if s['id'] in [1, 8, 9, 11]]
    route_foods = [f for f in foods if f['id'] in [101, 106, 120]]
    route_spots.sort(key=lambda s: s['id'])

    waypoints = []                              # 途经点列表
    # 起点: 曹州牡丹园 (id=1)
    start = spots[0]                            # id=1
    waypoints.append({
        'type': 'spot',
        'lng': start['lng'],
        'lat': start['lat'],
        'name': start['name']
    })
    # 中间美食点: 烧牛肉(101), 胡辣汤(106), 羊肉汤(120)
    for fid in [101, 106, 120]:
        food = next((f for f in foods if f['id'] == fid), None)
        if food:
            waypoints.append({
                'type': 'food',
                'lng': food['lng'],
                'lat': food['lat'],
                'name': food['name']
            })
    # 终点: 天香公园 (id=9)
    end = spots[8]                              # id=9, 天香公园
    waypoints.append({
        'type': 'spot',
        'lng': end['lng'],
        'lat': end['lat'],
        'name': end['name']
    })

    # 计算路径坐标数组和总距离
    path = [[wp['lng'], wp['lat']] for wp in waypoints]
    total_dist = 0
    for i in range(len(waypoints) - 1):
        total_dist += haversine(
            waypoints[i]['lng'], waypoints[i]['lat'],
            waypoints[i+1]['lng'], waypoints[i+1]['lat']
        )

    # 构建路线JSON对象
    route = {
        'name': '菏泽美食文化一日游路线',
        'found': True,
        'path': path,
        'waypoints': waypoints,
        'totalDistance': round(total_dist, 1),
        'totalTime': round(total_dist / (40 * 1000 / 60), 1)  # 按40km/h市区车速估算
    }

    # 第五步：写入 route.json
    route_path = os.path.join(web_data_dir, 'route.json')
    with open(route_path, 'w', encoding='utf-8') as f:
        json.dump(route, f, ensure_ascii=False, indent=2)
    print(f"已写入: {route_path}")
    print(f"路线总距离: {total_dist/1000:.1f} km")
    print(f"路线总时间: {route['totalTime']:.1f} 分钟")

if __name__ == '__main__':
    main()
