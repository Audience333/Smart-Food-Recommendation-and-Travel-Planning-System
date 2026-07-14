#!/usr/bin/env python3
"""
为美食和景点POI获取高德地图POI详情中的照片URL
通过POI名称+坐标搜索获取amap_id，再调用详情API获取照片
"""
import json
import os
import re
import sys
import time
import urllib.request
import urllib.parse
import math

sys.stdout.reconfigure(encoding='utf-8')

KEY = "647bb3e7a596c479b998f3e20a5a486a"
CITY = "菏泽"
RATE_LIMIT = 0.15


def http_get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read().decode("utf-8"))


def haversine(lng1, lat1, lng2, lat2):
    R = 6371000
    dlat = math.radians(lat2 - lat1)
    dlng = math.radians(lng2 - lng1)
    a = math.sin(dlat / 2) ** 2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlng / 2) ** 2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def find_amap_id(name, lng, lat):
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
        for p in pois:
            plng = float(p.get("location", "0,0").split(",")[0])
            plat = float(p.get("location", "0,0").split(",")[1])
            dist = haversine(lng, lat, plng, plat)
            if dist < best_dist:
                best_dist = dist
                best = p
        if best and best_dist < 5000:
            return best.get("id")
    except Exception as e:
        print(f"  [搜索失败] {name}: {e}")
    return None


def fetch_photos(amap_id):
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
        for p in photos[:3]:
            u = p.get("url", "")
            if u:
                urls.append(u)
        return ";".join(urls) if urls else "-"
    except Exception as e:
        print(f"  [详情失败] {amap_id}: {e}")
        return "-"


def parse_food_line(line):
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
    print(f"\n=== 处理 {label}: {filepath} ===")
    lines = []
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    updated = []
    data_items = []
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            updated.append(line)
            continue
        item = parser(stripped)
        if item:
            item["raw_line"] = line
            item["stripped"] = stripped
            data_items.append(item)
        else:
            updated.append(line)

    total = len(data_items)
    success = 0
    for i, item in enumerate(data_items):
        name = item["name"]
        lng = item["lng"]
        lat = item["lat"]
        print(f"[{i+1}/{total}] {name} ({lng}, {lat}) ...", end=" ", flush=True)

        amap_id = find_amap_id(name, lng, lat)
        if amap_id:
            print(f"id={amap_id}", end=" ", flush=True)
        else:
            print("无搜索结果", end=" ", flush=True)

        photos = fetch_photos(amap_id)
        if photos != "-":
            print(f"photos={len(photos.split(';'))}", end="")
            success += 1
        else:
            print("无照片", end="")

        parts = item["parts"]
        parts.insert(-1, photos)
        updated.append("|".join(parts) + "\n")

        time.sleep(RATE_LIMIT)
        print()

    print(f"\n{label} 完成: {success}/{total} 个POI有照片")

    with open(filepath, "w", encoding="utf-8") as f:
        f.writelines(updated)
    print(f"已写回: {filepath}")


def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, "data")

    food_path = os.path.join(data_dir, "food.txt")
    spot_path = os.path.join(data_dir, "spot.txt")

    process_file(food_path, parse_food_line, "美食")
    process_file(spot_path, parse_spot_line, "景点")


if __name__ == "__main__":
    main()
