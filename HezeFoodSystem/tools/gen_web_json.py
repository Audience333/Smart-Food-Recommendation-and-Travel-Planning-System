#!/usr/bin/env python3
"""
从更新后的TXT数据文件生成Web JSON数据
"""
import json
import os
import math

def parse_food_line(line):
    """解析美食数据行（支持可选address和opentime字段）"""
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
                    part9 = parts[8]
                    is_time = (":" in part9 or "-" in part9 or "全天" in part9 or
                              any(c.isdigit() for c in part9))
                    if is_time:
                        opentime = part9
                        tags_part = parts[9]
                    else:
                        tags_part = part9
                else:
                    tags_part = parts[8]
            else:
                tags_part = parts[7]
        else:
            tags_part = parts[7]

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
    """解析景点数据行"""
    parts = [p.strip() for p in line.split('|')]
    if len(parts) < 13:
        return None
    try:
        # 前12个字段是固定字段，接着可能为photos（嵌入在标签中），剩余的全部作为标签
        fixed_fields = parts[:12]
        remaining = parts[12:]
        photos_str = '-'
        tag_fields = []
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
    """解析数据文件"""
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
    """计算两点间球面距离（米）"""
    R = 6371000
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlam = math.radians(lon2 - lon1)
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlam/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, 'data')
    web_data_dir = os.path.join(base_dir, 'web', 'data')
    os.makedirs(web_data_dir, exist_ok=True)

    # 解析数据文件
    foods = parse_file(os.path.join(data_dir, 'food.txt'), parse_food_line)
    spots = parse_file(os.path.join(data_dir, 'spot.txt'), parse_spot_line)

    print(f"解析美食数据: {len(foods)} 条")
    print(f"解析景点数据: {len(spots)} 条")

    # 写入 food.json
    food_path = os.path.join(web_data_dir, 'food.json')
    with open(food_path, 'w', encoding='utf-8') as f:
        json.dump(foods, f, ensure_ascii=False, indent=2)
    print(f"已写入: {food_path}")

    # 写入 spot.json
    spot_path = os.path.join(web_data_dir, 'spot.json')
    with open(spot_path, 'w', encoding='utf-8') as f:
        json.dump(spots, f, ensure_ascii=False, indent=2)
    print(f"已写入: {spot_path}")

    # 生成示例路线: 曹州牡丹园 → 附近美食 → 天香公园 → 老城曹州
    route_spots = [s for s in spots if s['id'] in [1, 8, 9, 11]]
    route_foods = [f for f in foods if f['id'] in [101, 106, 120]]
    route_spots.sort(key=lambda s: s['id'])

    waypoints = []
    # 起点: 曹州牡丹园
    start = spots[0]  # id=1
    waypoints.append({
        'type': 'spot',
        'lng': start['lng'],
        'lat': start['lat'],
        'name': start['name']
    })
    # 中间点: 烧牛肉, 胡辣汤, 羊肉汤
    for fid in [101, 106, 120]:
        food = next((f for f in foods if f['id'] == fid), None)
        if food:
            waypoints.append({
                'type': 'food',
                'lng': food['lng'],
                'lat': food['lat'],
                'name': food['name']
            })
    # 终点: 天香公园
    end = spots[8]  # id=9, 天香公园
    waypoints.append({
        'type': 'spot',
        'lng': end['lng'],
        'lat': end['lat'],
        'name': end['name']
    })

    # 计算路径和总距离
    path = [[wp['lng'], wp['lat']] for wp in waypoints]
    total_dist = 0
    for i in range(len(waypoints) - 1):
        total_dist += haversine(
            waypoints[i]['lng'], waypoints[i]['lat'],
            waypoints[i+1]['lng'], waypoints[i+1]['lat']
        )

    route = {
        'name': '菏泽美食文化一日游路线',
        'found': True,
        'path': path,
        'waypoints': waypoints,
        'totalDistance': round(total_dist, 1),
        'totalTime': round(total_dist / (40 * 1000 / 60), 1)  # 40km/h estimate
    }

    route_path = os.path.join(web_data_dir, 'route.json')
    with open(route_path, 'w', encoding='utf-8') as f:
        json.dump(route, f, ensure_ascii=False, indent=2)
    print(f"已写入: {route_path}")
    print(f"路线总距离: {total_dist/1000:.1f} km")
    print(f"路线总时间: {route['totalTime']:.1f} 分钟")

if __name__ == '__main__':
    main()
