#!/usr/bin/env python3
"""
==========================================================================
高德地图POI照片获取脚本 (fetch_photos.py)
==========================================================================
【脚本用途】
  为 data/food.txt 和 data/spot.txt 中的每个美食和景点POI，通过高德地图
  POI关键字搜索API找到对应的POI ID（amap_id），再调用POI详情API获取照片
  URL列表，将照片URL嵌入到数据行的标签字段之前。

【在数据流水线中的位置】
  expand_data.py(扩展数据) -> fill_addresses.py(补全地址) ->
  本脚本(获取照片) -> recalc_roads.py -> gen_web_json.py(生成前端JSON)

  本脚本在数据补全后运行，为POI添加真实照片以改善前端展示效果。

【何时运行】
  - 数据扩展后，希望获取真实照片替代默认图片
  - 前端展示POI无照片或照片过时时

【运行方式】
  cd HezeFoodSystem
  python tools/fetch_photos.py

【前置条件】
  - 有效的API Key（硬编码在脚本顶部 KEY 变量中）
  - 已存在 data/food.txt 和 data/spot.txt
  - 网络连接正常

【输出】
  - 原地更新 data/food.txt，在标签字段前插入照片URL（;分隔）
  - 原地更新 data/spot.txt，同样插入照片URL

【API调用说明】
  本脚本使用两个高德API端点的组合：
  1. 关键字搜索 (place/text): 通过POI名称+城市搜索，获取 amap_id
  2. POI详情 (place/detail): 通过 amap_id 获取详细信息，从中提取照片URL

【注意】
  - 搜索结果可能不精确，通过坐标距离选取最匹配的POI
  - 每两个请求之间间隔0.15秒，控制API调用频率
==========================================================================
"""
import json
import os
import re
import sys
import time
import urllib.request
import urllib.parse
import math

# 设置控制台输出编码为UTF-8，确保中文字符正常显示
sys.stdout.reconfigure(encoding='utf-8')

# ==================== 配置常量 ====================
# 高德API Key（注意：此处硬编码，生产环境建议改用配置文件）
KEY = "647bb3e7a596c479b998f3e20a5a486a"
# 搜索城市限定（所有POI均在菏泽范围内搜索）
CITY = "菏泽"
# API请求频率限制（秒）：每次请求后等待0.15秒，避免触发高德API限流
RATE_LIMIT = 0.15


def http_get(url):
    """
    发送HTTP GET请求到高德API并返回JSON解析结果

    设置15秒超时，防止网络异常导致脚本长时间阻塞

    参数:
        url (str): 完整的API请求URL

    返回:
        dict: API响应JSON解析后的字典

    异常:
        网络异常会向上抛出，由调用方处理
    """
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read().decode("utf-8"))


def haversine(lng1, lat1, lng2, lat2):
    """
    使用Haversine公式计算地球表面两点间的球面距离

    参数:
        lng1, lat1 (float): 第一个点的经纬度
        lng2, lat2 (float): 第二个点的经纬度

    返回:
        float: 两点间的球面距离（单位：米）
    """
    R = 6371000                                 # 地球平均半径（米）
    dlat = math.radians(lat2 - lat1)
    dlng = math.radians(lng2 - lng1)
    a = math.sin(dlat / 2) ** 2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlng / 2) ** 2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def find_amap_id(name, lng, lat):
    """
    通过POI名称和坐标查找对应的高德POI ID (amap_id)

    【高德API: 关键字搜索 (place/text)】
      - 端点: https://restapi.amap.com/v3/place/text
      - 参数:
          key       - API Key
          keywords  - 搜索关键词（POI名称）
          city      - 搜索城市
          output    - 输出格式（json）
      - 响应: {
          "status": "1",
          "pois": [
            {"id": "B0FFFAB6J2", "name": "xxx", "location": "116.39,39.91", ...}
          ]
        }

    匹配策略：从搜索结果中选择与输入坐标距离最近且距离小于5km的POI
    5km阈值可过滤掉同名但位于不同位置的结果（如连锁店）

    参数:
        name (str): POI名称
        lng (float): POI经度
        lat (float): POI纬度

    返回:
        str | None: 匹配成功返回高德POI ID，失败返回None
    """
    url = "https://restapi.amap.com/v3/place/text?key={}&keywords={}&city={}&output=json".format(
        KEY, urllib.parse.quote(name), urllib.parse.quote(CITY)
    )
    try:
        data = http_get(url)
        if data.get("status") != "1" or not data.get("pois"):
            return None
        pois = data["pois"]
        best = None
        best_dist = float("inf")
        # 从所有搜索结果中选择距离最近的POI
        for p in pois:
            plng = float(p.get("location", "0,0").split(",")[0])
            plat = float(p.get("location", "0,0").split(",")[1])
            dist = haversine(lng, lat, plng, plat)
            if dist < best_dist:
                best_dist = dist
                best = p
        # 距离超过5km则认为搜索结果不相关
        if best and best_dist < 5000:
            return best.get("id")
    except Exception as e:
        print(f"  [搜索失败] {name}: {e}")
    return None


def fetch_photos(amap_id):
    """
    通过高德POI ID获取该POI的照片URL列表

    【高德API: POI详情 (place/detail)】
      - 端点: https://restapi.amap.com/v3/place/detail
      - 参数:
          key - API Key
          id  - POI的ID（由关键字搜索API返回）
      - 响应: {
          "status": "1",
          "pois": [{
            "photos": [
              {"url": "https://xxx.jpg", "title": "门面"},
              {"url": "https://yyy.jpg", "title": "大堂"}
            ]
          }]
        }
      - 说明: 每次调用返回某POI的详细信息，最多包含多张照片

    策略：最多取前3张照片，用分号(;)拼接URL

    参数:
        amap_id (str): 高德POI ID

    返回:
        str: 分号分隔的照片URL字符串，无照片则返回 "-"
    """
    if not amap_id:
        return "-"
    url = "https://restapi.amap.com/v3/place/detail?key={}&id={}".format(KEY, amap_id)
    try:
        data = http_get(url)
        if data.get("status") != "1" or not data.get("pois"):
            return "-"
        poi = data["pois"][0]
        photos = poi.get("photos", [])
        if not photos:
            return "-"
        urls = []
        for p in photos[:3]:                    # 最多取前3张照片
            u = p.get("url", "")
            if u:
                urls.append(u)
        return ";".join(urls) if urls else "-"
    except Exception as e:
        print(f"  [详情失败] {amap_id}: {e}")
        return "-"


def parse_food_line(line):
    """
    解析美食数据行，提取名称和坐标

    美食数据格式: ID|名称|经度|纬度|价格|评分|分类|...

    参数:
        line (str): 美食数据行字符串

    返回:
        dict | None: 解析成功返回 {"name", "lng", "lat", "parts"} 字典，
                     解析失败返回None
    """
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 8:
        return None
    try:
        name = parts[1]
        lng = float(parts[2])
        lat = float(parts[3])
        return {"name": name, "lng": lng, "lat": lat, "parts": parts}
    except (ValueError, IndexError):
        return None


def parse_spot_line(line):
    """
    解析景点数据行，提取名称和坐标

    景点数据格式: ID|名称|描述|地址|经度|纬度|类型|票价|...|标签

    参数:
        line (str): 景点数据行字符串

    返回:
        dict | None: 解析成功返回 {"name", "lng", "lat", "parts"} 字典，
                     解析失败返回None
    """
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 12:
        return None
    try:
        name = parts[1]
        lng = float(parts[4]) if len(parts) > 4 else 0
        lat = float(parts[5]) if len(parts) > 5 else 0
        return {"name": name, "lng": lng, "lat": lat, "parts": parts}
    except (ValueError, IndexError):
        return None


def process_file(filepath, parser, label):
    """
    处理单个数据文件：为每个POI获取照片并嵌入到数据行

    处理流程：
    1. 读取文件所有行
    2. 解析每个数据行（跳过注释和空行）
    3. 对每个POI：
       a. 调用 find_amap_id() 搜索高德POI ID
       b. 调用 fetch_photos() 获取照片URL
       c. 将照片URL插入到标签字段之前
    4. 写回更新后的文件

    照片的插入位置：紧邻标签字段之前
    - 美食: 原有字段顺序 ...|营业时间|(照片)|标签
    - 景点: 原有字段顺序 ...|评分|(照片)|标签

    参数:
        filepath (str): 数据文件路径
        parser (callable): 解析函数（parse_food_line 或 parse_spot_line）
        label (str): 数据类型标签（"美食" 或 "景点"），用于日志输出
    """
    print(f"\n=== 处理 {label}: {filepath} ===")
    lines = []
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    updated = []
    data_items = []
    # 第一遍扫描：分离注释行和数据行
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            updated.append(line)                # 注释行和空行原样保留
            continue
        item = parser(stripped)
        if item:
            item["raw_line"] = line
            item["stripped"] = stripped
            data_items.append(item)
        else:
            updated.append(line)                # 解析失败的行原样保留

    # 第二遍处理：逐个POI获取照片
    total = len(data_items)
    success = 0
    for i, item in enumerate(data_items):
        name = item["name"]
        lng = item["lng"]
        lat = item["lat"]
        print(f"[{i+1}/{total}] {name} ({lng}, {lat}) ...", end=" ", flush=True)

        # 搜索高德POI ID
        amap_id = find_amap_id(name, lng, lat)
        if amap_id:
            print(f"id={amap_id}", end=" ", flush=True)
        else:
            print("无搜索结果", end=" ", flush=True)

        # 获取照片
        photos = fetch_photos(amap_id)
        if photos != "-":
            print(f"photos={len(photos.split(';'))}", end="")
            success += 1
        else:
            print("无照片", end="")

        # 将照片URL插入到标签字段之前（倒数第一个字段之前）
        parts = item["parts"]
        parts.insert(-1, photos)
        updated.append("|".join(parts) + "\n")

        time.sleep(RATE_LIMIT)                  # 控制API请求频率
        print()

    print(f"\n{label} 完成: {success}/{total} 个POI有照片")

    # 写回文件
    with open(filepath, "w", encoding="utf-8") as f:
        f.writelines(updated)
    print(f"已写回: {filepath}")


def main():
    """
    主函数：依次处理美食和景点数据文件

    使用相对于脚本所在目录的路径计算基准目录
    处理流程：
    1. 计算基准目录（HezeFoodSystem/）
    2. 定位 data/food.txt 和 data/spot.txt
    3. 调用 process_file 依次处理两个文件
    """
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, "data")

    food_path = os.path.join(data_dir, "food.txt")
    spot_path = os.path.join(data_dir, "spot.txt")

    process_file(food_path, parse_food_line, "美食")
    process_file(spot_path, parse_spot_line, "景点")


if __name__ == "__main__":
    main()
