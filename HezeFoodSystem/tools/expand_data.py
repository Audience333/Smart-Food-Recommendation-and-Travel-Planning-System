"""
高德地图数据扩展脚本
按区县+分类搜索美食和景点POI，去重后追加到TXT数据文件
使用: python tools/expand_data.py
"""
import json
import math
import os
import time
import urllib.request
import urllib.parse

CONFIG_PATH = "config/amap_config.txt"
FOOD_TXT = "data/food.txt"
SPOT_TXT = "data/spot.txt"

DISTRICTS = ["牡丹区", "单县", "曹县", "郓城", "巨野", "东明", "定陶", "成武", "鄄城"]
FOOD_KEYWORDS = ["火锅", "烧烤", "面馆", "川菜", "鲁菜", "小吃", "快餐", "甜品",
                 "饮品", "海鲜", "自助餐", "农家菜", "特色菜", "汤", "糕点", "羊肉汤"]
SPOT_KEYWORDS = ["公园", "景区", "博物馆", "文化", "寺庙", "展览", "纪念馆"]

MAX_FOODS_PER_DISTRICT = 25
MAX_SPOTS_PER_DISTRICT = 10


def load_config():
    config = {}
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
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


def load_existing_names(filepath):
    names = set()
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [p.strip() for p in line.split("|")]
                if len(parts) >= 2:
                    names.add(parts[1])
    return names


def load_existing_food_locations(filepath):
    items = []
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
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
                if not line or line.startswith("#"):
                    continue
                parts = [p.strip() for p in line.split("|")]
                if len(parts) >= 6:
                    try:
                        items.append((parts[1], float(parts[4]), float(parts[5])))
                    except ValueError:
                        pass
    return items


def name_similarity(a, b):
    sa = set(a)
    sb = set(b)
    if not sa or not sb:
        return 0
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
                if not line or line.startswith("#"):
                    continue
                parts = [p.strip() for p in line.split("|")]
                try:
                    mid = int(parts[id_index])
                    if mid > max_id:
                        max_id = mid
                except (ValueError, IndexError):
                    pass
    return max_id


def fetch_poi_by_keywords(key, keywords, types, city, page_size=20, max_pages=2):
    all_pois = []
    seen = set()
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
            for poi in pois:
                pid = poi.get("id", "") or poi.get("name", "")
                if pid not in seen:
                    seen.add(pid)
                    all_pois.append(poi)
            if len(pois) < page_size:
                break
            time.sleep(0.3)
    return all_pois


def infer_food_category(name, type_code):
    if "汤" in name or "粥" in name:
        return "汤类"
    if any(k in name for k in ["面", "馍", "饼", "包", "饺", "粉"]):
        return "面食"
    if any(k in name for k in ["鸡", "鸭", "鹅"]):
        return "正餐"
    if any(k in name for k in ["茶", "奶", "汁", "咖"]):
        return "饮品"
    if any(k in name for k in ["糕", "酥", "糖", "甜"]):
        return "甜品"
    if any(k in name for k in ["串", "烤", "羊"]):
        return "烧烤"
    if any(k in name for k in ["凉", "拌"]):
        return "凉菜"
    if type_code and type_code.startswith("050"):
        return "正餐"
    if "火锅" in name:
        return "正餐"
    return "小吃"


def generate_food_tags(name, category, district):
    tags = []
    tag_map = {"汤类":"汤品","面食":"面食","甜品":"甜品","烧烤":"烧烤","饮品":"饮品","小吃":"小吃","正餐":"聚餐","凉菜":"凉菜"}
    if category in tag_map:
        tags.append(tag_map[category])
    for kw, tag in [("羊肉","羊肉"),("牛肉","牛肉"),("鸡","鸡肉"),("鱼","鱼类"),("海鲜","海鲜")]:
        if kw in name:
            tags.append(tag)
    if district != "菏泽市":
        tags.append(district + "特色")
    tags.append("地方特色")
    return tags


def infer_spot_type(type_name):
    if not type_name:
        return "自然景观"
    if any(k in type_name for k in ["公园","风景","山","湿地","河","湖"]):
        return "自然景观"
    if any(k in type_name for k in ["博物","古迹","纪念","历史","红色","庙"]):
        return "历史文化"
    if any(k in type_name for k in ["游乐","乐园","主题"]):
        return "主题公园"
    return "自然景观"


def parse_location(loc_str):
    if not loc_str:
        return 0, 0
    parts = loc_str.split(",")
    if len(parts) == 2:
        try:
            return float(parts[0]), float(parts[1])
        except:
            pass
    return 0, 0


def safe_str(value, default="-"):
    if isinstance(value, list):
        return ", ".join(str(v) for v in value)
    if isinstance(value, str):
        return value
    if value is None:
        return default
    return str(value)


def safe_float_from_biz(biz_ext, key, default):
    val = default
    if isinstance(biz_ext, dict):
        try:
            val = float(biz_ext.get(key, default))
        except:
            pass
    elif isinstance(biz_ext, str):
        try:
            ext_data = json.loads(biz_ext)
            val = float(ext_data.get(key, default))
        except:
            pass
    if val < 0 or val > 999:
        val = default
    return val


def convert_new_foods(pois, start_id, existing_names, existing_locs, district):
    candidates = []
    for poi in pois:
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0:
            continue
        name = poi.get("name", "")
        if is_duplicate(name, lng, lat, existing_names, existing_locs):
            continue
        type_code = poi.get("typecode", "")
        category = infer_food_category(name, type_code)
        biz_ext = poi.get("biz_ext", {})
        score = safe_float_from_biz(biz_ext, "rating", 4.0)
        if score < 1.0 or score > 5.0:
            score = 4.0
        cost = safe_float_from_biz(biz_ext, "cost", 30.0)
        address = safe_str(poi.get("address", "-")).replace("|", " ")
        tags = generate_food_tags(name, category, district)
        candidates.append({
            "name": name, "lng": lng, "lat": lat,
            "price": cost, "score": score, "category": category,
            "address": address, "tags": tags
        })

    candidates.sort(key=lambda x: x["score"], reverse=True)
    selected = candidates[:MAX_FOODS_PER_DISTRICT]

    foods = []
    for item in selected:
        item["id"] = start_id
        foods.append(item)
        existing_names.add(item["name"])
        existing_locs.append((item["name"], item["lng"], item["lat"]))
        start_id += 1

    return foods


def convert_new_spots(pois, start_id, existing_names, existing_locs):
    candidates = []
    for poi in pois:
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0:
            continue
        name = poi.get("name", "")
        if is_duplicate(name, lng, lat, existing_names, existing_locs):
            continue
        type_name = poi.get("type", "")
        biz_ext = poi.get("biz_ext", {})
        score = safe_float_from_biz(biz_ext, "rating", 4.0)
        if score < 1.0 or score > 5.0:
            score = 4.0
        address = safe_str(poi.get("address", "-")).replace("|", " ")
        candidates.append({
            "name": name, "lng": lng, "lat": lat,
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

    candidates.sort(key=lambda x: x["score"], reverse=True)
    selected = candidates[:MAX_SPOTS_PER_DISTRICT]

    spots = []
    for item in selected:
        item["id"] = start_id
        spots.append(item)
        existing_names.add(item["name"])
        existing_locs.append((item["name"], item["lng"], item["lat"]))
        start_id += 1

    return spots


def format_food_line(food):
    tags_str = ",".join(food["tags"])
    return (f"{food['id']}|{food['name']}|{food['lng']}|{food['lat']}|"
            f"{food['price']}|{food['score']}|{food['category']}|"
            f"{food['address']}|{tags_str}")


def format_spot_line(spot):
    tags_str = "|".join(spot["tags"]) if spot["tags"] else "新增"
    return (f"{spot['id']}|{spot['name']}|{spot['description']}|{spot['address']}|"
            f"{spot['lng']}|{spot['lat']}|{spot['type']}|{spot['ticketInfo']}|"
            f"{spot['openingTime']}|{spot['recommendDuration']}|{spot['bestSeason']}|"
            f"{spot['score']}|{tags_str}")


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

        print(f"  搜索美食 ({len(FOOD_KEYWORDS)} 关键词)...")
        food_pois = fetch_poi_by_keywords(key, FOOD_KEYWORDS,
            "050000|051000|052000", district, page_size=20, max_pages=2)
        new_foods = convert_new_foods(food_pois, max_food_id + len(all_new_foods) + 1,
                                       existing_food_names, existing_food_locs, district)
        all_new_foods.extend(new_foods)
        print(f"  新增美食: {len(new_foods)}")

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

    print()
    print("完成！下一步: python tools/recalc_roads.py && python tools/gen_web_json.py")


if __name__ == "__main__":
    main()
