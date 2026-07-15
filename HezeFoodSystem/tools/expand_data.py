"""
==========================================================================
高德地图POI数据扩展脚本 (expand_data.py)
==========================================================================
【脚本用途】
  按菏泽市各区县分别搜索美食和景点POI，与已有数据去重后追加到 data/food.txt
  和 data/spot.txt 数据文件中，实现数据集的自动扩充。

【在数据流水线中的位置】
  预处理阶段 -> 本脚本(扩展数据) -> recalc_roads.py(重新计算道路连接) ->
  gen_web_json.py(生成前端JSON) -> Web端加载展示

【何时运行】
  - 当需要扩充美食或景点数据时手动运行
  - 数据量不足时（如每个区县的美食/景点数少于预期）
  - 作为数据流水线的一部分，在所有数据脚本中优先运行

【运行方式】
  cd HezeFoodSystem
  python tools/expand_data.py

【前置条件】
  - config/amap_config.txt 中配置了有效的高德API Key
  - 已存在 data/food.txt 和 data/spot.txt（可以为空或只有注释行）
  - 网络连接正常

【输出】
  - 向 data/food.txt 追加新增的美食POI数据
  - 向 data/spot.txt 追加新增的景点POI数据

【下一步】
  python tools/recalc_roads.py && python tools/gen_web_json.py
==========================================================================
"""
import json
import math
import os
import time
import urllib.request
import urllib.parse

# ==================== 文件路径常量 ====================
# 高德API配置文件路径，包含 AMAP_KEY
CONFIG_PATH = "config/amap_config.txt"
# 美食数据文件（TXT格式，以 | 分隔字段）
FOOD_TXT = "data/food.txt"
# 景点数据文件（TXT格式，以 | 分隔字段）
SPOT_TXT = "data/spot.txt"

# ==================== 搜索范围配置 ====================
# 菏泽市下辖的9个区县，脚本将逐一为每个区县搜索POI
DISTRICTS = ["牡丹区", "单县", "曹县", "郓城", "巨野", "东明", "定陶", "成武", "鄄城"]

# 美食搜索关键词列表：涵盖主流餐饮类型，提高搜索覆盖面
FOOD_KEYWORDS = ["火锅", "烧烤", "面馆", "川菜", "鲁菜", "小吃", "快餐", "甜品",
                 "饮品", "海鲜", "自助餐", "农家菜", "特色菜", "汤", "糕点", "羊肉汤"]

# 景点搜索关键词列表：涵盖各类旅游目的地类型
SPOT_KEYWORDS = ["公园", "景区", "博物馆", "文化", "寺庙", "展览", "纪念馆"]

# ==================== 标签自动生成配置 ====================
# 以下五个字典用于根据POI名称中的关键词自动推断和生成标签

# 口味标签关键词映射：根据菜品名称中的特征词推断口味类型
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

# 食材标签关键词映射：根据菜品名称推断主要食材
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

# 烹饪方式标签关键词映射：根据菜品名称推断烹饪方法
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

# 菜系标签关键词映射：根据菜品名称中的地名或特征词推断菜系
CUISINE_KEYWORDS = {
    "鲁菜": ["鲁", "山东", "菏泽", "济宁", "济南", "胶东"],
    "川菜": ["川", "四川", "成都", "重庆", "麻辣"],
    "粤菜": ["粤", "广东", "广州", "潮汕", "烧腊"],
    "湘菜": ["湘", "湖南", "长沙", "剁椒"],
    "清真": ["清真", "回民", "清真寺"],
    "东北菜": ["东北", "黑龙江", "吉林", "辽宁", "哈尔滨", "铁锅"],
}

# ==================== 数量限制 ====================
# 每个区县最多获取的美食POI数量（按评分降序后取前25个）
MAX_FOODS_PER_DISTRICT = 25
# 每个区县最多获取的景点POI数量（按评分降序后取前10个）
MAX_SPOTS_PER_DISTRICT = 10


def load_config():
    """
    加载高德API配置文件

    解析 config/amap_config.txt 中的键值对配置，跳过空行和注释行（#开头）
    返回配置字典，其中包含 AMAP_KEY 等必要配置项

    返回:
        dict: 配置键值对字典，如 {"AMAP_KEY": "your_key_here", ...}
    """
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
    """
    向高德地图Web API发送HTTP请求并返回JSON解析结果

    内置重试机制，失败后等待2秒重试，最多重试 retries 次
    设置10秒超时，防止网络故障导致脚本挂起

    参数:
        url (str): 完整的API请求URL
        retries (int): 最大重试次数（不包括首次请求），默认2次

    返回:
        dict | None: 成功返回API响应的JSON字典，全部失败返回None
    """
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
    """
    使用Haversine公式计算地球表面两点间的球面距离

    Haversine公式假设地球为完美球体（半径R=6371000米），计算大圆距离
    相比简单的欧氏距离，更适用于地理坐标的距离计算

    参数:
        lng1 (float): 第一个点的经度
        lat1 (float): 第一个点的纬度
        lng2 (float): 第二个点的经度
        lat2 (float): 第二个点的纬度

    返回:
        float: 两点间的球面距离（单位：米）
    """
    R = 6371000                                 # 地球平均半径（米）
    dlat = math.radians(lat2 - lat1)            # 纬度差（弧度）
    dlng = math.radians(lng2 - lng1)            # 经度差（弧度）
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlng/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))


def load_existing_names(filepath):
    """
    从TXT数据文件中加载所有已有POI的名称集合，用于去重判断

    解析以 | 为分隔符的数据行，提取第二个字段（名称）加入集合

    参数:
        filepath (str): 数据文件的路径（如 data/food.txt 或 data/spot.txt）

    返回:
        set: 已有POI名称的集合
    """
    names = set()
    if os.path.exists(filepath):
        with open(filepath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = [p.strip() for p in line.split("|")]
                if len(parts) >= 2:
                    names.add(parts[1])         # 第2个字段为POI名称
    return names


def load_existing_food_locations(filepath):
    """
    从美食数据文件中加载所有已有美食的名称和坐标，用于地理位置去重

    解析以 | 为分隔符的数据行，提取名称(第2字段)、经度(第3字段)、纬度(第4字段)
    与 load_existing_names 不同，本函数还记录坐标，用于距离+名称相似度的双重去重

    参数:
        filepath (str): 美食数据文件路径

    返回:
        list: [(name, lng, lat), ...] 元组列表
    """
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
    """
    从景点数据文件中加载所有已有景点的名称和坐标

    景点数据格式与美食不同：名称在第2字段，经度在第5字段，纬度在第6字段

    参数:
        filepath (str): 景点数据文件路径

    返回:
        list: [(name, lng, lat), ...] 元组列表
    """
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
    """
    计算两个POI名称的Jaccard相似度（基于字符集合）

    Jaccard相似度 = 两个名称共有字符数 / 两个名称全部不同字符数
    取值范围 [0, 1]，越大表示名称越相似

    参数:
        a (str): 第一个POI名称
        b (str): 第二个POI名称

    返回:
        float: Jaccard相似度（0~1之间）
    """
    sa = set(a)
    sb = set(b)
    if not sa or not sb:
        return 0
    return len(sa & sb) / len(sa | sb)


def is_duplicate(name, lng, lat, existing_names, existing_locations):
    """
    判断一个新POI是否与已有数据重复

    去重策略：
    1. 先检查名称是否完全匹配（精确去重）
    2. 再检查200米范围内是否有名称相似度超过70%的POI（模糊去重）
       这种双重策略可以有效过滤掉同一店铺使用不同名称或同一POI在不同关键词
       搜索中重复出现的情况

    参数:
        name (str): 待检查的POI名称
        lng (float): 待检查的POI经度
        lat (float): 待检查的POI纬度
        existing_names (set): 已有POI名称集合
        existing_locations (list): 已有POI坐标列表 [(name, lng, lat), ...]

    返回:
        bool: True表示重复（应该跳过），False表示不重复
    """
    if name in existing_names:
        return True
    for ename, elng, elat in existing_locations:
        dist = haversine(lng, lat, elng, elat)
        if dist < 200 and name_similarity(name, ename) > 0.7:
            return True
    return False


def get_max_id(filepath, id_index=0):
    """
    获取数据文件中的最大POI ID，用于为新数据分配ID

    遍历文件中所有有效数据行，提取指定位置的ID字段并记录最大值
    新POI的ID将从 max_id + 1 开始递增分配

    参数:
        filepath (str): 数据文件路径
        id_index (int): ID字段在行中的索引位置，默认为0（第一个字段）

    返回:
        int: 当前最大ID值（若无有效数据则返回0）
    """
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
    """
    通过多个关键词搜索高德POI，汇总去重后返回

    【高德API: 关键字搜索 (place/text)】
      - 端点: https://restapi.amap.com/v3/place/text
      - 参数:
          key         - 高德API Key
          keywords    - 搜索关键词
          city        - 搜索城市（如"牡丹区"）
          citylimit   - 是否限制在城市范围内（"true"）
          types       - POI类型代码（如 "050000|051000" 为餐饮类）
          offset      - 每页返回数量
          page        - 页码
          output      - 输出格式（"json"）
      - 响应: {"status": "1", "pois": [{"id":..., "name":..., "location":...}, ...]}

    策略：遍历每个关键词，对每个关键词分页获取结果，
    用POI的id字段去重（同一POI可能被不同关键词搜到）

    参数:
        key (str): 高德API Key
        keywords (list): 搜索关键词列表
        types (str): POI类型代码字符串
        city (str): 搜索城市名称
        page_size (int): 每页返回的POI数量，默认20
        max_pages (int): 每个关键词最多翻页数，默认2

    返回:
        list: 去重后的所有POI数据列表
    """
    all_pois = []
    seen = set()                                # 用于记录已获取的POI ID，避免同一POI重复添加
    for kw in keywords:
        for page in range(1, max_pages + 1):
            # 构造请求参数
            params = {
                "key": key, "keywords": kw, "city": city,
                "citylimit": "true", "types": types,
                "offset": str(page_size), "page": str(page), "output": "json"
            }
            url = "https://restapi.amap.com/v3/place/text?" + urllib.parse.urlencode(params)
            data = amap_request(url)
            if not data or data.get("status") != "1":
                break                           # API调用失败时终止当前关键词
            pois = data.get("pois", [])
            if not pois:
                break                           # 无更多结果时终止翻页
            for poi in pois:
                pid = poi.get("id", "") or poi.get("name", "")
                if pid not in seen:
                    seen.add(pid)
                    all_pois.append(poi)
            if len(pois) < page_size:
                break                           # 最后一页，不再翻页
            time.sleep(0.3)                     # 控制请求频率，避免触发API限流
    return all_pois


def infer_food_category(name, type_code):
    """
    根据POI名称和类型代码推断美食分类

    分类优先级策略（从上到下匹配，越具体的规则越靠前）：
    1. 名称中包含特定关键词 -> 直接返回对应分类
    2. 高德API类型代码以"050"开头 -> 正餐
    3. 名称中包含"火锅" -> 正餐
    4. 无法匹配 -> 默认归为"小吃"

    参数:
        name (str): POI名称
        type_code (str): 高德API返回的POI类型代码

    返回:
        str: 分类名称（汤类/面食/正餐/饮品/甜品/烧烤/凉菜/小吃）
    """
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
        return "正餐"                            # 高德分类码050开头为餐饮大类
    if "火锅" in name:
        return "正餐"
    return "小吃"


def extract_opentime(biz_ext, category):
    """
    从高德API返回的biz_ext中提取营业时间

    biz_ext可能是dict或JSON字符串，本函数兼容两种格式
    如果API未返回营业时间，则根据美食分类使用默认营业时间

    参数:
        biz_ext (dict|str): 高德API返回的扩展信息
        category (str): 美食分类，用于匹配默认营业时间

    返回:
        str: 营业时间字符串（如 "06:00-14:00,17:00-21:00"）
    """
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
        return opentime.strip().replace("|", " ")   # 去除管道符避免破坏数据格式
    # 各分类的默认营业时间
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
    """
    为美食POI自动生成丰富的标签集合

    标签生成策略（按执行顺序）：
    1. 口味标签：匹配 TASTE_KEYWORDS 字典
    2. 食材标签：匹配 INGREDIENT_KEYWORDS 字典
    3. 烹饪方式标签：匹配 COOKING_KEYWORDS 字典
    4. 菜系标签：匹配 CUISINE_KEYWORDS 字典（默认归为"鲁菜"）
    5. 用餐时段标签：基于分类（下午茶/宵夜/早餐/午餐晚餐）
    6. 价格档位标签：基于人均价格（一人食/堂食外卖/聚餐宴请）
    7. 评分等级标签：基于评分（必吃榜/口碑店/人气高）
    8. 特色标签：老字号、地方名吃、24小时营业等
    9. 场景标签：朋友聚会、商务宴请、独自用餐等
    10. 价格区间标签：分段（10元以下/10-30/30-60/60-100/100以上）
    11. 分类默认标签：基于美食分类添加推荐标签
    12. 口味补充：若口味标签不足3个，从候选池中补充
    13. 地域标签：添加区县相关的定位标签

    参数:
        name (str): POI名称
        category (str): 美食分类
        district (str): 所在区县
        score (float): 评分（1~5）
        price (float): 人均价格（元）
        type_code (str): 高德类型代码
        opentime (str): 营业时间

    返回:
        list: 去重后的标签字符串列表
    """
    tags = []

    # 第1步：根据名称匹配口味标签
    for taste, keywords in TASTE_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(taste)

    # 第2步：根据名称匹配食材标签
    for ingredient, keywords in INGREDIENT_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(ingredient)

    # 第3步：根据名称匹配烹饪方式标签
    for cooking, keywords in COOKING_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(cooking)

    # 第4步：匹配菜系标签（默认归为"鲁菜"）
    cuisine_matched = False
    for cuisine, keywords in CUISINE_KEYWORDS.items():
        if any(kw in name for kw in keywords):
            tags.append(cuisine)
            cuisine_matched = True
    if not cuisine_matched:
        tags.append("鲁菜")

    # 第5步：基于分类添加用餐时段标签
    if category in ("甜品", "饮品"):
        tags.append("下午茶")
    elif "火锅" in name or category == "烧烤":
        tags.append("宵夜")
    elif category in ("汤类", "面食"):
        tags.append("早餐")
    else:
        tags.append("午餐")
        tags.append("晚餐")

    # 第6步：基于人均价格添加消费场景标签
    if price < 15:
        tags.append("一人食")
        tags.append("快餐")
    elif price <= 80:
        tags.append("堂食")
        tags.append("外卖")
    else:
        tags.append("聚餐")
        tags.append("宴请")

    # 第7步：基于评分添加等级标签
    if score >= 4.7:
        tags.append("必吃榜")
        tags.append("口碑店")
    elif score >= 4.5:
        tags.append("人气高")
        tags.append("口碑店")

    # 第8步：特色标签
    if any(k in name for k in ("老", "传统", "正宗")):
        tags.append("老字号")
    if district in name:
        tags.append("地方名吃")

    # 第9步：价格区间标签（细粒度分段）
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

    # 第10步：24小时营业标签
    if opentime:
        if any(k in opentime for k in ("24小时", "全天")):
            tags.append("24小时营业")

    # 第11步：场景标签
    if "火锅" in name or category == "烧烤":
        tags.append("朋友聚会")
    if price > 80:
        tags.append("商务宴请")
    if "亲子" in name:
        tags.append("亲子")
    if price <= 20:
        tags.append("独自用餐")

    # 第12步：特殊标签
    if "秘制" in name:
        tags.append("秘制配方")
    if any(k in name for k in ("限定", "季节")):
        tags.append("季节限定")

    # 第13步：区县特色标签
    if district != "菏泽市" and district:
        tag = district + "特色"
        if tag not in tags:
            tags.append(tag)

    # 第14步：基于分类添加默认标签组
    category_defaults = {
        "汤类": ["鲜香", "原味", "清淡", "暖胃", "滋补"],
        "面食": ["筋道", "快餐", "主食"],
        "小吃": ["现点现做", "地方特色"],
        "正餐": ["菜品丰富", "环境好"],
        "烧烤": ["孜然", "宵夜", "朋友聚会"],
        "甜品": ["甜口", "下午茶"],
        "饮品": ["清爽", "下午茶"],
        "凉菜": ["开胃"],
    }
    for tag in category_defaults.get(category, []):
        tags.append(tag)

    # 第15步：基于价格再追加标签
    if price <= 10:
        for t in ["平价实惠", "快餐"]:
            tags.append(t)
    elif price <= 30:
        for t in ["家常", "堂食"]:
            tags.append(t)
    elif price <= 60:
        for t in ["品质餐厅", "朋友聚会"]:
            tags.append(t)
    elif price <= 100:
        for t in ["环境好", "商务宴请"]:
            tags.append(t)
    else:
        for t in ["高端", "宴请", "包厢"]:
            tags.append(t)

    # 第16步：口味标签不足3个时从候选池中补充
    taste_keys = set(TASTE_KEYWORDS.keys())
    existing_taste_count = sum(1 for t in tags if t in taste_keys)
    if existing_taste_count < 3:
        flavor_pool = ["鲜香", "原味", "五香", "酱香", "清淡"]
        seed = sum(ord(c) for c in name)
        available = [f for f in flavor_pool if f not in tags]
        available.sort(key=lambda f: (hash(f + name) % 10000))
        count = 2 + (seed % 2)
        count = min(count, len(available))
        for tag in available[:count]:
            tags.append(tag)

    # 第17步：添加区域定位标签
    if district:
        tags.append(district + "美食")
        tags.append(district + "人气")

    # 第18步：通用标签（提升搜索和推荐匹配度）
    if "菏泽" not in name:
        tags.append("菏泽")
    tags.append("美食推荐")
    tags.append("人气美食")

    # 第19步：去重（保持原始添加顺序）
    seen = set()
    unique_tags = []
    for tag in tags:
        if tag not in seen:
            seen.add(tag)
            unique_tags.append(tag)
    return unique_tags


def infer_spot_type(type_name):
    """
    根据高德API返回的POI类型名称推断景点分类

    分类规则：
    - 包含"公园""风景""山""湿地""河""湖" -> 自然景观
    - 包含"博物""古迹""纪念""历史""红色""庙" -> 历史文化
    - 包含"游乐""乐园""主题" -> 主题公园
    - 无法匹配 -> 默认"自然景观"

    参数:
        type_name (str): 高德API返回的POI类型名称（如 "风景名胜"）

    返回:
        str: 景点分类名称
    """
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
    """
    解析高德坐标字符串 "经度,纬度" 为浮点数元组

    参数:
        loc_str (str): 坐标字符串，如 "116.397428,39.90923"

    返回:
        tuple: (经度, 纬度)，解析失败返回 (0, 0)
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


def safe_str(value, default="-"):
    """
    安全地将值转换为字符串，处理列表类型（如标签列表）且替换管道符

    参数:
        value: 需要转换的值，可能是 str/list/None/其他
        default (str): 当值为None时返回的默认值

    返回:
        str: 转换后的字符串（管道符已被替换为空格）
    """
    if isinstance(value, list):
        return ", ".join(str(v) for v in value)
    if isinstance(value, str):
        return value
    if value is None:
        return default
    return str(value)


def safe_float_from_biz(biz_ext, key, default):
    """
    安全地从高德API的biz_ext中提取浮点数值

    biz_ext可能是dict或JSON字符串，本函数兼容两种格式
    加上取值范围验证（0~999），防止异常数据

    参数:
        biz_ext (dict|str): 高德API的扩展信息
        key (str): 要提取的字段名（如 "rating", "cost"）
        default (float): 默认值

    返回:
        float: 提取到的浮点数值
    """
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
    """
    将高德API返回的美食POI列表转换为系统内部格式

    处理流程：
    1. 解析每条POI的坐标、名称
    2. 去重检查（名称+位置双重去重）
    3. 推断分类、提取评分/价格/地址/营业时间
    4. 自动生成标签
    5. 按评分降序排序，取前 MAX_FOODS_PER_DISTRICT 条
    6. 分配递增ID并更新去重集合

    参数:
        pois (list): 高德API返回的POI原始数据
        start_id (int): 起始分配ID
        existing_names (set): 已有POI名称集合（用于去重，会就地修改）
        existing_locs (list): 已有POI坐标列表（用于去重，会就地修改）
        district (str): 所在区县名称

    返回:
        list: 转换后的美食数据字典列表，每个字典包含 id/name/lng/lat/price/score/
              category/address/tags/opentime 字段
    """
    candidates = []
    for poi in pois:
        lng, lat = parse_location(poi.get("location", ""))
        if lng == 0 or lat == 0:
            continue                            # 跳过坐标无效的POI
        name = poi.get("name", "")
        if is_duplicate(name, lng, lat, existing_names, existing_locs):
            continue                            # 跳过重复的POI
        type_code = poi.get("typecode", "")
        category = infer_food_category(name, type_code)
        biz_ext = poi.get("biz_ext", {})
        score = safe_float_from_biz(biz_ext, "rating", 4.0)
        if score < 1.0 or score > 5.0:
            score = 4.0                         # 评分异常时使用默认值
        cost = safe_float_from_biz(biz_ext, "cost", 30.0)
        address = safe_str(poi.get("address", "-")).replace("|", " ")
        opentime = extract_opentime(biz_ext, category)
        tags = generate_food_tags(name, category, district, score, cost, type_code, opentime)
        candidates.append({
            "name": name, "lng": lng, "lat": lat,
            "price": cost, "score": score, "category": category,
            "address": address, "tags": tags, "opentime": opentime
        })

    # 按评分降序排序，优先保留高评分POI
    candidates.sort(key=lambda x: x["score"], reverse=True)
    selected = candidates[:MAX_FOODS_PER_DISTRICT]

    foods = []
    for item in selected:
        item["id"] = start_id
        foods.append(item)
        # 将新POI加入去重集合，防止后续区县的POI与之重复
        existing_names.add(item["name"])
        existing_locs.append((item["name"], item["lng"], item["lat"]))
        start_id += 1

    return foods


def convert_new_spots(pois, start_id, existing_names, existing_locs):
    """
    将高德API返回的景点POI列表转换为系统内部格式

    与 convert_new_foods 流程类似，但景点字段不同：
    包含 description、ticketInfo、openingTime、recommendDuration、bestSeason 等

    参数:
        pois (list): 高德API返回的景点POI原始数据
        start_id (int): 起始分配ID
        existing_names (set): 已有POI名称集合（用于去重，会就地修改）
        existing_locs (list): 已有POI坐标列表（用于去重，会就地修改）

    返回:
        list: 转换后的景点数据字典列表
    """
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
            "description": name,               # 默认用名称作为描述
            "address": address,
            "type": infer_spot_type(type_name),
            "ticketInfo": "免费",               # 默认免费，实际票价需手动修正
            "openingTime": "08:00-18:00",       # 默认营业时间
            "recommendDuration": "1-2小时",      # 默认推荐游玩时长
            "bestSeason": "全年",               # 默认最佳季节
            "score": score,
            "tags": []                          # 初始化空标签列表
        })

    # 按评分降序排序，取前 MAX_SPOTS_PER_DISTRICT 个
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
    """
    将美食数据字典格式化为TXT数据行（管道符分隔）

    格式: ID|名称|经度|纬度|价格|评分|分类|地址|营业时间|标签(逗号分隔)

    参数:
        food (dict): 美食数据字典

    返回:
        str: 格式化后的数据行字符串
    """
    tags_str = ",".join(food["tags"])
    opentime = food.get("opentime", "-") or "-"
    return (f"{food['id']}|{food['name']}|{food['lng']}|{food['lat']}|"
            f"{food['price']}|{food['score']}|{food['category']}|"
            f"{food['address']}|{opentime}|{tags_str}")


def format_spot_line(spot):
    """
    将景点数据字典格式化为TXT数据行（管道符分隔）

    格式: ID|名称|描述|地址|经度|纬度|类型|票价|营业时间|推荐时长|最佳季节|评分|标签(|分割)

    参数:
        spot (dict): 景点数据字典

    返回:
        str: 格式化后的数据行字符串
    """
    tags_str = "|".join(spot["tags"]) if spot["tags"] else "新增"
    return (f"{spot['id']}|{spot['name']}|{spot['description']}|{spot['address']}|"
            f"{spot['lng']}|{spot['lat']}|{spot['type']}|{spot['ticketInfo']}|"
            f"{spot['openingTime']}|{spot['recommendDuration']}|{spot['bestSeason']}|"
            f"{spot['score']}|{tags_str}")


def main():
    """
    主函数：执行完整的数据扩展流程

    流程：
    1. 加载高德API配置
    2. 读取已有美食和景点数据（名称集合 + 坐标列表）
    3. 获取当前最大ID（为新数据分配ID时递增）
    4. 遍历每个区县：
       a. 搜索该区县的美食POI（使用 FOOD_KEYWORDS）
       b. 搜索该区县的景点POI（使用 SPOT_KEYWORDS）
       c. 转换并去重后添加到汇总列表
    5. 将全部新增数据追加写入 data/food.txt 和 data/spot.txt
    6. 输出统计摘要和下一步提示
    """
    print("=" * 60)
    print("  高德地图POI数据扩展")
    print("=" * 60)

    # 加载API Key
    config = load_config()
    key = config.get("AMAP_KEY") or config.get("API_KEY")
    if not key:
        print("ERROR: API Key not found in config/amap_config.txt")
        return

    print(f"API Key: {key[:8]}...")

    # 加载已有数据（用于去重和ID分配）
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

    # 逐区县搜索和转换
    for district in DISTRICTS:
        print(f"[District: {district}]")

        # 搜索该区县的美食POI
        print(f"  搜索美食 ({len(FOOD_KEYWORDS)} 关键词)...")
        food_pois = fetch_poi_by_keywords(key, FOOD_KEYWORDS,
            "050000|051000|052000",               # 高德餐饮类型代码
            district, page_size=20, max_pages=2)
        new_foods = convert_new_foods(food_pois, max_food_id + len(all_new_foods) + 1,
                                       existing_food_names, existing_food_locs, district)
        all_new_foods.extend(new_foods)
        print(f"  新增美食: {len(new_foods)}")

        # 搜索该区县的景点POI
        print(f"  搜索景点 ({len(SPOT_KEYWORDS)} 关键词)...")
        spot_pois = fetch_poi_by_keywords(key, SPOT_KEYWORDS,
            "110000|110100|110200",               # 高德旅游类型代码
            district, page_size=20, max_pages=2)
        new_spots = convert_new_spots(spot_pois, max_spot_id + len(all_new_spots) + 1,
                                       existing_spot_names, existing_spot_locs)
        all_new_spots.extend(new_spots)
        print(f"  新增景点: {len(new_spots)}")
        print()

    # 输出统计摘要
    print("=" * 60)
    print(f"总计新增美食: {len(all_new_foods)}")
    print(f"总计新增景点: {len(all_new_spots)}")

    # 追加写入美食数据文件
    if all_new_foods:
        with open(FOOD_TXT, "a", encoding="utf-8") as f:
            f.write(f"\n# ========== 高德API扩展数据 ({time.strftime('%Y-%m-%d')}) ==========\n")
            for food in all_new_foods:
                f.write(format_food_line(food) + "\n")
        print(f"已追加到 {FOOD_TXT}")

    # 追加写入景点数据文件
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
