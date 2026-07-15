#!/usr/bin/env python3
"""
==========================================================================
道路连接距离重新计算脚本 (recalc_roads.py)
==========================================================================
【脚本用途】
  基于 data/food.txt 和 data/spot.txt 中POI的真实经纬度坐标，重新计算
  各节点之间的道路连接距离和预估时间，生成 data/road.txt 道路网络文件。

  道路网络是后续路径规划算法（如最短路径、推荐路线）的基础数据结构。

【在数据流水线中的位置】
  expand_data.py(扩展数据) -> fill_addresses.py -> fetch_photos.py ->
  本脚本(重算道路) -> gen_web_json.py(生成JSON) -> Web端展示

  本脚本应在所有POI数据更新完成后运行，确保道路网络反映最新的节点集合。

【何时运行】
  - 数据扩展后（新增了POI），需重新计算路网
  - 修改了POI坐标后
  - 道路连接关系需要调整时

【运行方式】
  cd HezeFoodSystem
  python tools/recalc_roads.py

【前置条件】
  - 已存在 data/food.txt 和 data/spot.txt
  - config/amap_config.txt 中配置了有效的高德API Key（可选，无Key则使用Haversine估算）
  - 网络连接正常（如果需要使用高德驾车路径规划API）

【输出】
  - data/road.txt - 道路连接数据（格式: 起点ID|终点ID|距离(米)|预计时间(分钟)）

【距离计算策略】
  1. 优先使用高德驾车路径规划API获取真实驾车距离和时间
  2. 若API不可用，使用Haversine球面距离公式计算直线距离
  3. 跨区道路乘以1.3系数估算实际道路距离

【道路网络结构说明】
  节点类型：
  - 景点节点: ID 1-100（按 spot.txt 中的ID分配）
  - 美食节点: ID 101+  （按 food.txt 中的ID分配）

  连接类型：
  - 牡丹区景点间连接 (ID <= 110 且 ID <= 110)
  - 景点↔美食连接 (跨类型的双向连接)
  - 牡丹区美食间连接 (101 <= ID <= 188)
  - 跨区连接 (牡丹区 ↔ 各县)
  - 各县内部连接 (ID >= 189)

  连接关系：
  - 景点↔景点: 根据实际地理位置和预设连接关系
  - 景点↔附近美食: 5km范围内自动建立双向连接
  - 美食↔美食: 同一聚类内距离 < 3km 的相互连接
  - 跨区hub: 各区代表美食与最近景点间连接
==========================================================================
"""
import math
import re
import os
import urllib.request
import json
import time

def haversine(lon1, lat1, lon2, lat2):
    """
    使用Haversine公式计算地球表面两点间的球面距离

    该公式考虑了地球曲率，给出大圆距离（最短路径的弧长）的近似值
    在菏泽城市尺度下（几十公里范围），误差在可接受范围内

    参数:
        lon1, lat1 (float): 第一个点的经纬度
        lon2, lat2 (float): 第二个点的经纬度

    返回:
        float: 两点间的球面距离（单位：米）
    """
    R = 6371000                                 # 地球平均半径（米）
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlam = math.radians(lon2 - lon1)
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlam/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def parse_file(filepath):
    """
    解析数据文件，返回节点字典 {id: (name, lng, lat)}

    自动扫描行中的数值字段，取第一个作为经度，下一个作为纬度
    这种方法兼容美食文件和景点文件的不同字段排列

    参数:
        filepath (str): 数据文件路径（food.txt 或 spot.txt）

    返回:
        dict: {id: (name, lng, lat), ...} 节点信息字典
    """
    data = {}
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = [p.strip() for p in line.split('|')]
            if len(parts) < 2:
                continue
            oid = int(parts[0])                 # ID始终在第一个字段
            name = parts[1]                     # 名称在第二个字段
            # 扫描后续字段，找到第一个数值字段作为经度，下一个作为纬度
            lng, lat = None, None
            for i, p in enumerate(parts):
                if lng is None and i >= 2:      # 从第3个字段开始查找数值
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
    """
    根据直线距离估算驾车时间

    默认车速40km/h，模拟菏泽市区平均通行速度
    返回结果至少为1分钟，避免极端情况下出现0分钟

    参数:
        dist_m (float): 距离（米）
        speed_kmh (float): 平均速度（公里/小时），默认40

    返回:
        int: 预估时间（分钟），最小值1
    """
    return max(1, round(dist_m / (speed_kmh * 1000 / 60)))

def amap_driving(key, origin_lng, origin_lat, dest_lng, dest_lat):
    """
    调用高德驾车路径规划API获取实际驾车距离和时间

    【高德API: 驾车路径规划 (direction/driving)】
      - 端点: https://restapi.amap.com/v3/direction/driving
      - 参数:
          key         - API Key
          origin      - 起点坐标 "经度,纬度"
          destination - 终点坐标 "经度,纬度"
          strategy    - 路径规划策略（0=速度优先/默认）
      - 响应: {
          "status": "1",
          "route": {
            "paths": [{
              "distance": 12345,    // 单位：米
              "duration": 1800      // 单位：秒
            }]
          }
        }
      - 说明: 返回的是实际道路路径距离，比Haversine直线距离更精确

    参数:
        key (str): 高德API Key
        origin_lng, origin_lat (float): 起点经纬度
        dest_lng, dest_lat (float): 终点经纬度

    返回:
        tuple: (距离(米), 时间(分钟))，失败返回 (None, None)
    """
    try:
        url = (f"https://restapi.amap.com/v3/direction/driving?"
               f"origin={origin_lng},{origin_lat}&destination={dest_lng},{dest_lat}"
               f"&key={key}&strategy=0")
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        if data.get("status") == "1" and data.get("route", {}).get("paths"):
            path = data["route"]["paths"][0]
            return int(path["distance"]), int(path["duration"]) // 60  # 秒转分钟
    except Exception:
        pass
    return None, None


def main():
    """
    主函数：计算并生成完整的道路连接网络

    执行流程：
    1. 加载配置（API Key）
    2. 解析 spot.txt 和 food.txt，构建节点集合
    3. 计算各类连接关系：
       a. 牡丹区景点间连接 - 基于预设的邻接关系
       b. 各区美食hub与最近美食的连接 - 使用驾车API
       c. 景点↔附近美食连接 - 5km半径自动匹配
       d. 牡丹区美食间连接 - 聚类内部互相连接
       e. 新增牡丹区美食间连接 - 扩展数据的新增美食
       f. 跨区连接 - 各区代表节点全互联
       g. 新增景点连接 - 新景点与曹州牡丹园的连接
       h. 各县内部连接 - 各区县美食间内部连通
       i. 新增各县内部连接 - 扩展数据的新增美食
    4. 去重：每条双向边只保留一条（取两端ID较小的为key）
    5. 按分类写入 data/road.txt

    节点ID分配规则：
    - 景点ID: 1-100（高德POI搜索返回的原始ID）
    - 原始美食: 101+ （food.txt中的原始ID）
    - 新增美食: 根据扩展后的实际ID范围
    """
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, 'data')

    # 加载高德API Key（如果有的话）
    config = {}
    config_path = os.path.join(base_dir, 'config', 'amap_config.txt')
    if os.path.exists(config_path):
        with open(config_path, 'r', encoding='utf-8') as cf:
            for line in cf:
                line = line.strip()
                if not line or line.startswith("#"): continue
                if "=" in line:
                    k, v = line.split("=", 1)
                    config[k.strip()] = v.strip()
    amap_key = config.get("AMAP_KEY", "")

    # 第一步：加载所有节点
    spots = parse_file(os.path.join(data_dir, 'spot.txt'))
    foods = parse_file(os.path.join(data_dir, 'food.txt'))

    print(f"加载景点: {len(spots)} 个")
    print(f"加载美食: {len(foods)} 个")

    # 合并所有节点到一个字典
    all_nodes = {}
    for sid, (name, lng, lat) in spots.items():
        all_nodes[sid] = (name, lng, lat)
    for fid, (name, lng, lat) in foods.items():
        all_nodes[fid] = (name, lng, lat)       # 美食ID保持原样

    # 边集合：存储所有计算结果 [(from_id, to_id, distance, time), ...]
    edges = []

    # ========== 第二部分：牡丹区景点间连接 ==========
    # 预设牡丹区景点的邻接关系，基于实际地理位置分析
    mudan_spots = [1, 2, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 18]

    # 景点邻接关系对：每组 (a, b) 表示两个景点之间应有道路连接
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
            na, nb = all_nodes[a], all_nodes[b]
            dist = None
            # 如果有API Key，优先使用高德驾车路径规划获取真实距离
            if amap_key:
                dist, dur = amap_driving(amap_key, na[1], na[2], nb[1], nb[2])
                if dist is not None:
                    edges.append((a, b, dist, dur))
                    edges.append((b, a, dist, dur))   # 无向边，双向添加
                    time.sleep(0.5)                   # 控制API调用频率
                    continue
            # 降级方案：使用Haversine公式计算直线距离并估算时间
            dist = haversine(na[1], na[2], nb[1], nb[2])
            t = time_from_distance(dist)
            edges.append((a, b, int(dist), t))
            edges.append((b, a, int(dist), t))

    # ========== 第三部分：各区美食hub连接最近美食 ==========
    # 每个区县有一个代表美食（hub），它与最近的几个美食节点建立连接
    # hub_id -> 该区县的所有美食ID范围
    county_hub_ranges = {
        121: list(range(122, 129)) + list(range(189, 214)),   # 单县
        129: list(range(130, 136)) + list(range(214, 239)),   # 曹县
        136: list(range(137, 142)) + list(range(239, 264)),   # 郓城
        142: list(range(143, 148)) + list(range(289, 314)),   # 东明
        148: list(range(149, 153)) + list(range(264, 289)),   # 巨野
        153: list(range(154, 157)) + list(range(314, 339)),   # 定陶
        157: list(range(158, 161)) + list(range(339, 364)),   # 成武
        161: list(range(162, 164)) + list(range(364, 389)),   # 鄄城
    }

    # 使用驾车API为每个hub连接最近的3个美食节点
    if amap_key:
        for hub_id, range_ids in county_hub_ranges.items():
            if hub_id not in all_nodes:
                continue
            hlng, hlat = all_nodes[hub_id][1], all_nodes[hub_id][2]
            candidates = []
            # 计算hub到每个候选美食的直线距离
            for fid in range_ids:
                if fid in all_nodes:
                    dist = haversine(hlng, hlat, all_nodes[fid][1], all_nodes[fid][2])
                    candidates.append((fid, dist))
            candidates.sort(key=lambda x: x[1])   # 按距离排序
            for fid, hdist in candidates[:3]:     # 取最近的3个
                rdist, rdur = amap_driving(amap_key, hlng, hlat, all_nodes[fid][1], all_nodes[fid][2])
                if rdist is not None:
                    edges.append((hub_id, fid, rdist, rdur))
                    edges.append((fid, hub_id, rdist, rdur))
                    time.sleep(0.5)

    # ========== 第四部分：景点 ↔ 附近美食 ==========
    # 每个景点连接其5km范围内的美食节点，取最近的5个
    for sid in mudan_spots:
        if sid not in all_nodes:
            continue
        slng, slat = all_nodes[sid][1], all_nodes[sid][2]
        nearby = []
        # 牡丹区美食ID范围: 原始101-120 + 新增164-188
        mudan_food_ids = list(range(101, 121)) + list(range(164, 189))
        for fid in mudan_food_ids:
            if fid not in all_nodes:
                continue
            flng, flat = all_nodes[fid][1], all_nodes[fid][2]
            dist = haversine(slng, slat, flng, flat)
            if dist < 5000:                     # 5km半径
                nearby.append((fid, dist))
        # 按距离排序，取最近的5个
        nearby.sort(key=lambda x: x[1])
        for fid, dist in nearby[:5]:
            t = time_from_distance(dist)
            edges.append((sid, fid, int(dist), t))
            edges.append((fid, sid, int(dist), t))

    # ========== 第五部分：牡丹区美食间连接 ==========
    # 将牡丹区美食按地理位置聚类，同一聚类内部互相建立双向连接
    food_clusters_1 = [101, 102, 108, 111, 112, 120]    # 南部美食
    food_clusters_2 = [103, 105, 106, 109, 110, 116, 117, 118]  # 中部美食
    food_clusters_3 = [104, 107, 113, 114, 119]         # 北部美食
    food_clusters_4 = [115]                             # 北部边缘（孤立节点）

    # 每个聚类内，距离 < 3km 的节点对互相连接
    for cluster in [food_clusters_1, food_clusters_2, food_clusters_3]:
        for i in range(len(cluster)):
            for j in range(i+1, len(cluster)):
                a, b = cluster[i], cluster[j]
                if a in all_nodes and b in all_nodes:
                    dist = haversine(all_nodes[a][1], all_nodes[a][2],
                                   all_nodes[b][1], all_nodes[b][2])
                    if dist < 3000:             # 3km阈值
                        t = time_from_distance(dist)
                        edges.append((a, b, int(dist), t))
                        edges.append((b, a, int(dist), t))

    # 新增牡丹区美食间连接 (ID 164-188)
    new_mudan_foods = list(range(164, 189))
    for i in range(len(new_mudan_foods)):
        for j in range(i+1, len(new_mudan_foods)):
            a, b = new_mudan_foods[i], new_mudan_foods[j]
            if a in all_nodes and b in all_nodes:
                na, nb = all_nodes[a], all_nodes[b]
                hdist = haversine(na[1], na[2], nb[1], nb[2])
                if hdist < 5000:                # 新增美食的阈值放宽到5km
                    t = time_from_distance(hdist)
                    edges.append((a, b, int(hdist), t))
                    edges.append((b, a, int(hdist), t))

    # ========== 第六部分：跨区连接（菏泽↔各县城） ==========
    # 定义各区县的hub代表节点
    district_hubs = {
        'mudan': 1,       # 曹州牡丹园（牡丹区hub）
        'shanxian': 121,  # 单县羊肉汤（单县hub）
        'caoxian': 129,   # 曹县烧牛肉（曹县hub）
        'yuncheng': 136,  # 郓城壮馍（郓城hub）
        'dongming': 142,  # 东明粉肚（东明hub）
        'juye': 148,      # 巨野罐子汤（巨野hub）
        'dingtao': 153,   # 定陶焖子（定陶hub）
        'chengwu': 157,   # 成武酱大头（成武hub）
        'juancheng': 161, # 鄄城烧鸡（鄄城hub）
    }

    # 所有hub之间两两建立连接（全互联）
    hub_ids = list(district_hubs.values())
    for i in range(len(hub_ids)):
        for j in range(i+1, len(hub_ids)):
            a, b = hub_ids[i], hub_ids[j]
            aid, bid = a, b
            if aid in all_nodes and bid in all_nodes:
                dist = haversine(all_nodes[aid][1], all_nodes[aid][2],
                               all_nodes[bid][1], all_nodes[bid][2])
                # 跨区道路距离乘以1.3系数，因实际道路并非直线
                road_dist = int(dist * 1.3)
                t = max(1, round(road_dist / (60 * 1000 / 60)))  # 按60km/h估算
                edges.append((aid, bid, road_dist, t))
                edges.append((bid, aid, road_dist, t))

    # 新增景点连接：牡丹区新增景点连接曹州牡丹园
    new_mudan_spots = [sid for sid in range(21, 31) if sid in all_nodes]
    for nsp in new_mudan_spots:
        if 1 in all_nodes and nsp in all_nodes:
            dist = haversine(all_nodes[1][1], all_nodes[1][2],
                           all_nodes[nsp][1], all_nodes[nsp][2])
            road_dist = int(dist * 1.3)         # 跨区系数
            t = max(1, round(road_dist / (60 * 1000 / 60)))
            edges.append((1, nsp, road_dist, t))
            edges.append((nsp, 1, road_dist, t))

    # ========== 第七部分：各县内部连接 ==========
    # 每个县内的美食节点，距离 < 5km 的互相连接
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
                    if dist < 5000:             # 5km阈值
                        t = time_from_distance(dist)
                        edges.append((a, b, int(dist), t))
                        edges.append((b, a, int(dist), t))

    # 新增各县内部连接（扩展数据新增的美食，ID范围189+）
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
                    na, nb = all_nodes[a], all_nodes[b]
                    hdist = haversine(na[1], na[2], nb[1], nb[2])
                    if hdist < 10000:           # 新增美食的阈值放宽到10km
                        t = time_from_distance(hdist)
                        edges.append((a, b, int(hdist), t))
                        edges.append((b, a, int(hdist), t))

    # ========== 第八部分：去重 ==========
    # 使用字典去重：以 (min_id, max_id) 为键，保留最短距离
    edge_map = {}
    for a, b, dist, t in edges:
        key = (min(a, b), max(a, b))            # 标准化键，忽略方向
        if key not in edge_map or dist < edge_map[key][0]:
            edge_map[key] = (dist, t)            # 保留更短的路径

    # ========== 第九部分：写入文件 ==========
    output_path = os.path.join(data_dir, 'road.txt')
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("# 菏泽城市道路连接数据（基于高德地图真实坐标计算）\n")
        f.write("# 格式: 起点ID|终点ID|距离(米)|预计时间(分钟)\n")
        f.write("# 节点类型: 1-110=景点, 101-388=美食\n")
        f.write("# 距离使用Haversine公式计算，跨区道路×1.3系数\n")
        f.write("# 更新日期: 2026-07-13\n")
        f.write("#\n")

        # 按连接类型分组输出
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
