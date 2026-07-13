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

TASTE_KEYWORDS = {
    "麻辣": ["麻", "辣", "麻辣", "红油", "香辣", "辣味"],
    "酸辣": ["酸辣", "酸", "醋", "泡椒", "酸菜"],
    "清淡": ["清淡", "清汤", "白味", "淡雅", "清蒸", "清炖", "清炒"],
    "酱香": ["酱", "酱油", "红烧", "老抽", "卤", "酱香"],
    "蒜蓉": ["蒜", "蒜蓉", "蒜泥", "蒜香"],
    "鲜香": ["鲜", "鲜香", "鲜美", "浓汤", "高汤", "醇厚"],
    "五香": ["五香", "八角", "桂皮", "花椒", "茴香"],
    "甜口": ["甜", "糖", "蜜", "甜品", "糕点", "甜味"],
    "孜然": ["孜然", "烤串"],
    "咖喱": ["咖喱", "咖哩"],
    "鲜嫩": ["鲜嫩", "嫩滑", "嫩", "滑嫩"],
    "酥脆": ["酥", "脆", "酥脆", "香酥", "酥皮", "脆皮"],
    "软糯": ["软糯", "软", "糯", "绵软"],
    "筋道": ["筋道", "Q弹", "弹性", "有嚼劲", "弹牙"],
    "肥而不腻": ["肥而不腻", "不腻"],
    "入口即化": ["入口即化", "即化", "融化"],
    "Q弹": ["Q弹", "弹性"],
}

INGREDIENT_KEYWORDS = {
    "羊肉": ["羊肉", "羊", "羊排", "羊腿", "羊蝎子", "羊汤"],
    "牛肉": ["牛肉", "牛", "牛排", "牛腩", "牛腱", "牛杂"],
    "猪肉": ["猪肉", "猪", "排骨", "蹄膀", "肘子", "五花"],
    "鸡肉": ["鸡", "鸡肉", "鸡腿", "鸡翅", "鸡汤", "烧鸡"],
    "鸭肉": ["鸭", "鸭肉", "鸭子", "烤鸭", "板鸭"],
    "鱼肉": ["鱼", "鱼肉", "鱼片", "烤鱼", "鱼汤", "酸菜鱼"],
    "海鲜": ["海鲜", "虾", "蟹", "贝", "鱿鱼", "蛤蜊", "海参"],
    "蔬菜": ["菜心", "青菜", "白菜", "豆芽", "土豆", "萝卜", "西红柿"],
    "豆制品": ["豆腐", "豆皮", "豆干", "千张", "腐竹", "豆花"],
    "面食": ["面", "馍", "饼", "包", "饺", "粉", "馕", "拉面"],
    "菌菇": ["菌", "菇", "木耳", "蘑菇", "香菇", "金针菇"],
    "内脏": ["肠", "肚", "肝", "腰", "百叶", "毛肚", "血"],
}

COOKING_KEYWORDS = {
    "烤": ["烤", "烧烤", "烤制", "炭烤", "电烤", "烤串"],
    "涮": ["涮", "火锅", "涮锅", "汆"],
    "炒": ["炒", "爆炒", "小炒", "煸", "滑炒"],
    "炖": ["炖", "煲", "焖", "煨", "熬", "慢炖"],
    "蒸": ["蒸", "清蒸", "粉蒸", "蒸制"],
    "炸": ["炸", "油炸", "炸制", "煎", "酥炸"],
    "卤": ["卤", "卤制", "卤味", "酱", "酱制"],
    "拌": ["拌", "凉拌", "调拌", "沙拉", "搅拌"],
    "煮": ["煮", "水煮", "白煮", "煮制", "熬煮"],
}

CUISINE_KEYWORDS = {
    "鲁菜": ["鲁", "山东", "菏泽", "济宁", "济南", "胶东"],
    "川菜": ["川", "四川", "成都", "重庆", "麻辣"],
    "粤菜": ["粤", "广东", "广州", "潮汕", "烧腊"],
    "湘菜": ["湘", "湖南", "长沙", "剁椒"],
    "清真": ["清真", "回民", "清真寺"],
    "东北菜": ["东北", "黑龙江", "吉林", "辽宁", "哈尔滨", "铁锅"],
}

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


def extract_opentime(biz_ext, category):
    if isinstance(biz_ext, dict):
        opentime = biz_ext.get("opentime", "")
    elif isinstance(biz_ext, str):
        try:
            ext_data = json.loads(biz_ext)
            opentime = ext_data.get("opentime", "")
        except Exception:
            opentime = ""
    else:
        opentime = ""
    if isinstance(opentime, str) and opentime.strip():
        return opentime.strip().replace("|", " ")
    defaults = {
        "汤类": "06:00-14:00,17:00-21:00",
        "面食": "06:00-14:00,17:00-21:00",
        "小吃": "08:00-21:00",
        "正餐": "10:00-14:00,17:00-22:00",
        "烧烤": "17:00-02:00",
        "甜品": "09:00-21:00",
        "饮品": "09:00-21:00",
        "凉菜": "10:00-21:00",
    }
    return defaults.get(category, "10:00-14:00,17:00-22:00")


def generate_food_tags(name, category, district, score, price, type_code, opentime):
    tags = []

    for taste, keywords in TASTE_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(taste)

    for ingredient, keywords in INGREDIENT_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(ingredient)

    for cooking, keywords in COOKING_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(cooking)

    cuisine_matched = False
    for cuisine, keywords in CUISINE_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(cuisine)
            cuisine_matched = True
    if not cuisine_matched:
        tags.append("鲁菜")

    if category in ("甜品", "饮品"):
        tags.append("下午茶")
    elif "火锅" in name or category == "烧烤":
        tags.append("宵夜")
    elif category in ("汤类", "面食"):
        tags.append("早餐")
    else:
        tags.append("午餐")
        tags.append("晚餐")

    if price < 15:
        tags.append("一人食")
        tags.append("快餐")
    elif price <= 80:
        tags.append("堂食")
        tags.append("外卖")
    else:
        tags.append("聚餐")
        tags.append("宴请")

    if score >= 4.7:
        tags.append("必吃榜")
        tags.append("口碑店")
    elif score >= 4.5:
        tags.append("人气高")
        tags.append("口碑店")
    if any(k in name for k in ("老", "传统", "正宗")):
        tags.append("老字号")
    if district in name:
        tags.append("地方名吃")

    if price <= 10:
        tags.append("人均10元以下")
    elif price <= 30:
        tags.append("人均10-30元")
    elif price <= 60:
        tags.append("人均30-60元")
    elif price <= 100:
        tags.append("人均60-100元")
    else:
        tags.append("人均100元以上")

    if opentime:
        if any(k in opentime for k in ("24小时", "全天")):
            tags.append("24小时营业")

    if "火锅" in name or category == "烧烤":
        tags.append("朋友聚会")
    if price > 80:
        tags.append("商务宴请")
    if "亲子" in name:
        tags.append("亲子")
    if price <= 20:
        tags.append("独自用餐")

    if "秘制" in name:
        tags.append("秘制配方")
    if any(k in name for k in ("限定", "季节")):
        tags.append("季节限定")

    if district != "菏泽市" and district:
        tag = district + "特色"
        if tag not in tags:
            tags.append(tag)

    seen = set()
    unique_tags = []
    for tag in tags:
        if tag not in seen:
            seen.add(tag)
            unique_tags.append(tag)
    return unique_tags


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
        opentime = extract_opentime(biz_ext, category)
        tags = generate_food_tags(name, category, district, score, cost, type_code, opentime)
        candidates.append({
            "name": name, "lng": lng, "lat": lat,
            "price": cost, "score": score, "category": category,
            "address": address, "tags": tags, "opentime": opentime
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
    opentime = food.get("opentime", "-") or "-"
    return (f"{food['id']}|{food['name']}|{food['lng']}|{food['lat']}|"
            f"{food['price']}|{food['score']}|{food['category']}|"
            f"{food['address']}|{opentime}|{tags_str}")


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
