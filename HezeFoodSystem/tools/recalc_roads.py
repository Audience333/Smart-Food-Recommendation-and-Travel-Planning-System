#!/usr/bin/env python3
"""
基于真实坐标重新计算道路连接距离
使用 Haversine 公式计算球面距离
"""
import math
import re
import os

def haversine(lon1, lat1, lon2, lat2):
    """计算两点间的球面距离（米）"""
    R = 6371000
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlam = math.radians(lon2 - lon1)
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlam/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def parse_file(filepath):
    """解析数据文件，返回 {id: (name, lng, lat)}"""
    data = {}
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = [p.strip() for p in line.split('|')]
            if len(parts) < 2:
                continue
            oid = int(parts[0])
            name = parts[1]
            # Find lng/lat - they should be numeric
            lng, lat = None, None
            for i, p in enumerate(parts):
                if lng is None and i >= 2:
                    try:
                        lng = float(p)
                        if i+1 < len(parts):
                            lat = float(parts[i+1])
                        break
                    except ValueError:
                        continue
            if lng is not None and lat is not None:
                data[oid] = (name, lng, lat)
    return data

def time_from_distance(dist_m, speed_kmh=40):
    """根据距离估算时间（分钟），默认市区车速40km/h"""
    return max(1, round(dist_m / (speed_kmh * 1000 / 60)))

def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, 'data')

    # 加载数据
    spots = parse_file(os.path.join(data_dir, 'spot.txt'))
    foods = parse_file(os.path.join(data_dir, 'food.txt'))

    print(f"加载景点: {len(spots)} 个")
    print(f"加载美食: {len(foods)} 个")

    # 合并所有节点: 景点ID 1-100, 美食ID 101+
    all_nodes = {}
    for sid, (name, lng, lat) in spots.items():
        all_nodes[sid] = (name, lng, lat)
    for fid, (name, lng, lat) in foods.items():
        all_nodes[fid] = (name, lng, lat)  # Food ID + 100 offset

    # 定义连接关系
    edges = []

    # ===== 牡丹区景点间连接 =====
    mudan_spots = [1, 2, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 18]
    # 相邻景点连接（根据实际地理位置）
    spot_connections = [
        (1, 2), (1, 13), (1, 14),   # 曹州牡丹园 ↔ 百花园、古今园、七里河
        (2, 13),                      # 百花园 ↔ 古今园
        (8, 9), (8, 12), (8, 16),    # 博物馆 ↔ 天香公园、纪念馆、新天地
        (9, 10), (9, 11), (9, 16),   # 天香公园 ↔ 赵王河、老城、新天地
        (10, 15),                     # 赵王河 ↔ 万福河
        (11, 20), (11, 12),          # 老城曹州 ↔ 动物园、纪念馆
        (12, 16),                     # 纪念馆 ↔ 新天地
        (19, 15),                     # 鲁西南记忆 ↔ 万福河
        (3, 17),                     # 水浒好汉城 ↔ 郓城博物馆
        (5, 19),                     # 仿山 ↔ 鲁西南记忆
    ]
    for a, b in spot_connections:
        if a in all_nodes and b in all_nodes:
            dist = haversine(all_nodes[a][1], all_nodes[a][2],
                           all_nodes[b][1], all_nodes[b][2])
            t = time_from_distance(dist)
            edges.append((a, b, int(dist), t))
            edges.append((b, a, int(dist), t))  # 无向

    # ===== 景点 ↔ 附近美食 =====
    # 每个景点连接附近的美食（5km范围内）
    for sid in mudan_spots:
        if sid not in all_nodes:
            continue
        slng, slat = all_nodes[sid][1], all_nodes[sid][2]
        nearby = []
        # 牡丹区美食: 原有101-120 + 新增164-188
        mudan_food_ids = list(range(101, 121)) + list(range(164, 189))
        for fid in mudan_food_ids:
            if fid not in all_nodes:
                continue
            flng, flat = all_nodes[fid][1], all_nodes[fid][2]
            dist = haversine(slng, slat, flng, flat)
            if dist < 5000:  # 5km以内
                nearby.append((fid, dist))
        # 取最近的3-5个
        nearby.sort(key=lambda x: x[1])
        for fid, dist in nearby[:5]:
            t = time_from_distance(dist)
            edges.append((sid, fid, int(dist), t))
            edges.append((fid, sid, int(dist), t))

    # ===== 牡丹区美食间连接 =====
    # 相邻美食聚类连接
    food_clusters_1 = [101, 102, 108, 111, 112, 120]  # 南部美食
    food_clusters_2 = [103, 105, 106, 109, 110, 116, 117, 118]  # 中部美食
    food_clusters_3 = [104, 107, 113, 114, 119]  # 北部美食
    food_clusters_4 = [115]  # 北部边缘

    for cluster in [food_clusters_1, food_clusters_2, food_clusters_3]:
        for i in range(len(cluster)):
            for j in range(i+1, len(cluster)):
                a, b = cluster[i], cluster[j]
                if a in all_nodes and b in all_nodes:
                    dist = haversine(all_nodes[a][1], all_nodes[a][2],
                                   all_nodes[b][1], all_nodes[b][2])
                    if dist < 3000:
                        t = time_from_distance(dist)
                        edges.append((a, b, int(dist), t))
                        edges.append((b, a, int(dist), t))

    # 新增牡丹区美食间连接 (164-188)
    new_mudan_foods = list(range(164, 189))
    for i in range(len(new_mudan_foods)):
        for j in range(i+1, len(new_mudan_foods)):
            a, b = new_mudan_foods[i], new_mudan_foods[j]
            if a in all_nodes and b in all_nodes:
                dist = haversine(all_nodes[a][1], all_nodes[a][2],
                               all_nodes[b][1], all_nodes[b][2])
                if dist < 5000:
                    t = time_from_distance(dist)
                    edges.append((a, b, int(dist), t))
                    edges.append((b, a, int(dist), t))

    # ===== 跨区连接（菏泽↔各县城） =====
    district_hubs = {
        'mudan': 1,       # 曹州牡丹园（牡丹区）
        'shanxian': 121,  # 单县羊肉汤（单县）
        'caoxian': 129,   # 曹县烧牛肉（曹县）
        'yuncheng': 136,  # 郓城壮馍（郓城）
        'dongming': 142,  # 东明粉肚（东明）
        'juye': 148,      # 巨野罐子汤（巨野）
        'dingtao': 153,   # 定陶焖子（定陶）
        'chengwu': 157,   # 成武酱大头（成武）
        'juancheng': 161, # 鄄城烧鸡（鄄城）
    }

    hub_ids = list(district_hubs.values())
    for i in range(len(hub_ids)):
        for j in range(i+1, len(hub_ids)):
            a, b = hub_ids[i], hub_ids[j]
            aid, bid = a, b
            if aid in all_nodes and bid in all_nodes:
                dist = haversine(all_nodes[aid][1], all_nodes[aid][2],
                               all_nodes[bid][1], all_nodes[bid][2])
                road_dist = int(dist * 1.3)
                t = max(1, round(road_dist / (60 * 1000 / 60)))
                edges.append((aid, bid, road_dist, t))
                edges.append((bid, aid, road_dist, t))

    # 新增景点连接：牡丹区景点连接各区新景点
    new_mudan_spots = [sid for sid in range(21, 31) if sid in all_nodes]
    for nsp in new_mudan_spots:
        if 1 in all_nodes and nsp in all_nodes:
            dist = haversine(all_nodes[1][1], all_nodes[1][2],
                           all_nodes[nsp][1], all_nodes[nsp][2])
            road_dist = int(dist * 1.3)
            t = max(1, round(road_dist / (60 * 1000 / 60)))
            edges.append((1, nsp, road_dist, t))
            edges.append((nsp, 1, road_dist, t))

    # ===== 各县内部连接 =====
    county_ranges = {
        'shanxian': (121, 128),
        'caoxian': (129, 135),
        'yuncheng': (136, 141),
        'dongming': (142, 147),
        'juye': (148, 152),
        'dingtao': (153, 156),
        'chengwu': (157, 160),
        'juancheng': (161, 163),
    }

    for county, (start, end) in county_ranges.items():
        county_foods = list(range(start, end + 1))
        for i in range(len(county_foods)):
            for j in range(i+1, len(county_foods)):
                a, b = county_foods[i], county_foods[j]
                if a in all_nodes and b in all_nodes:
                    dist = haversine(all_nodes[a][1], all_nodes[a][2],
                                   all_nodes[b][1], all_nodes[b][2])
                    if dist < 5000:
                        t = time_from_distance(dist)
                        edges.append((a, b, int(dist), t))
                        edges.append((b, a, int(dist), t))

    # ===== 新增各县内部连接（扩展数据） =====
    new_county_ranges = {
        'shanxian': (189, 213),
        'caoxian': (214, 238),
        'yuncheng': (239, 263),
        'juye': (264, 288),
        'dongming': (289, 313),
        'dingtao': (314, 338),
        'chengwu': (339, 363),
        'juancheng': (364, 388),
    }

    for county, (start, end) in new_county_ranges.items():
        county_foods = [fid for fid in range(start, end + 1) if fid in all_nodes]
        for i in range(len(county_foods)):
            for j in range(i+1, len(county_foods)):
                a, b = county_foods[i], county_foods[j]
                if a in all_nodes and b in all_nodes:
                    dist = haversine(all_nodes[a][1], all_nodes[a][2],
                                   all_nodes[b][1], all_nodes[b][2])
                    if dist < 10000:
                        t = time_from_distance(dist)
                        edges.append((a, b, int(dist), t))
                        edges.append((b, a, int(dist), t))

    # 去重：使用字典，保留最短距离
    edge_map = {}
    for a, b, dist, t in edges:
        key = (min(a, b), max(a, b))
        if key not in edge_map or dist < edge_map[key][0]:
            edge_map[key] = (dist, t)

    # 写入文件
    output_path = os.path.join(data_dir, 'road.txt')
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("# 菏泽城市道路连接数据（基于高德地图真实坐标计算）\n")
        f.write("# 格式: 起点ID|终点ID|距离(米)|预计时间(分钟)\n")
        f.write("# 节点类型: 1-110=景点, 101-388=美食\n")
        f.write("# 距离使用Haversine公式计算，跨区道路×1.3系数\n")
        f.write("# 更新日期: 2026-07-13\n")
        f.write("#\n")

        sections = [
            ("牡丹区景点间连接", lambda e: e[0] <= 110 and e[1] <= 110),
            ("景点↔美食连接", lambda e: (e[0] <= 110 and e[1] >= 101) or (e[1] <= 110 and e[0] >= 101)),
            ("牡丹区美食间连接", lambda e: 101 <= e[0] <= 188 and 101 <= e[1] <= 188),
            ("跨区连接", lambda e: (e[0] <= 110 and e[1] >= 189) or (e[1] <= 110 and e[0] >= 189)),
            ("各县内部连接", lambda e: e[0] >= 189 and e[1] >= 189),
        ]

        for section_name, filter_fn in sections:
            section_edges = [(a, b, d, t) for a, b, d, t in
                           [(k[0], k[1], v[0], v[1]) for k, v in edge_map.items()]
                           if filter_fn((a, b, d, t))]
            section_edges.sort()
            if section_edges:
                f.write(f"\n# ========== {section_name} ==========\n")
                for a, b, dist, t in section_edges:
                    f.write(f"{a}|{b}|{dist}|{t}\n")

    print(f"\n生成道路连接: {len(edge_map)} 条")
    print(f"已写入: {output_path}")

if __name__ == '__main__':
    main()
