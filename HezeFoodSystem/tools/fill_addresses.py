"""
==========================================================================
高德逆地理编码地址补全脚本 (fill_addresses.py)
==========================================================================
【脚本用途】
  检查 data/food.txt 和 data/spot.txt 中的POI地址字段，对缺失或无效的
  地址（空字符串或"-"），通过高德地图逆地理编码API根据经纬度坐标反查
  实际地址并补全。

【在数据流水线中的位置】
  expand_data.py(扩展数据) -> 本脚本(补全地址) -> recalc_roads.py ->
  gen_web_json.py(生成前端JSON) -> Web端展示

  本脚本在数据扩展后运行，确保新增POI都有可用的地址信息。

【何时运行】
  - 运行 expand_data.py 之后，数据新增了没有地址的POI
  - 发现前端显示某些POI地址为"-"或空白
  - 手动向数据文件中添加了缺少地址的POI

【运行方式】
  cd HezeFoodSystem
  python tools/fill_addresses.py

【前置条件】
  - config/amap_config.txt 中配置了有效的高德API Key
  - 已存在 data/food.txt 和 data/spot.txt
  - 网络连接正常

【输出】
  - 原地更新 data/food.txt 和 data/spot.txt，补全缺失的地址字段

【下一步】
  python tools/gen_web_json.py
==========================================================================
"""
import json
import os
import sys
import time
import urllib.request
import urllib.parse

# ==================== 文件路径常量 ====================
# 高德API配置文件路径
CONFIG_PATH = "config/amap_config.txt"
# 美食数据文件
FOOD_TXT = "data/food.txt"
# 景点数据文件
SPOT_TXT = "data/spot.txt"


def load_config():
    """
    加载高德API配置文件

    解析 config/amap_config.txt 中键值对格式的配置行，
    跳过空行和#开头的注释行

    返回:
        dict: 配置键值对字典
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


def reverse_geocode(key, lng, lat):
    """
    调用高德地图逆地理编码API，根据经纬度获取格式化地址

    【高德API: 逆地理编码 (geocode/regeo)】
      - 端点: https://restapi.amap.com/v3/geocode/regeo
      - 参数:
          key         - 高德API Key
          location    - 经纬度坐标，格式 "经度,纬度"
          extensions  - 返回结果控制，默认"base"即可
          output      - 输出格式，"json"
      - 响应: {
          "status": "1",
          "regeocode": {
            "formatted_address": "山东省菏泽市牡丹区xxx路xxx号"
          }
        }
      - 说明: 逆地理编码将经纬度坐标转换为结构化地址信息

    参数:
        key (str): 高德API Key
        lng (float): 经度
        lat (float): 纬度

    返回:
        str | None: 成功返回格式化地址字符串，失败返回None
    """
    params = {
        "key": key,
        "location": "{},{}".format(lng, lat),
        "extensions": "base",
        "output": "json"
    }
    url = "https://restapi.amap.com/v3/geocode/regeo?" + urllib.parse.urlencode(params)
    try:
        req = urllib.request.Request(url)
        req.add_header("User-Agent", "Mozilla/5.0")
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            if data.get("status") == "1" and data.get("regeocode"):
                return data["regeocode"].get("formatted_address", "")
    except Exception as e:
        print("  Geocode failed for {},{}: {}".format(lng, lat, e))
    return None


def parse_food_line(line):
    """
    解析美食数据行

    美食数据格式: ID|名称|经度|纬度|价格|评分|分类|地址|...

    参数:
        line (str): 美食数据行字符串

    返回:
        dict | None: 解析成功返回包含 raw/parts/id/name/lng/lat 的字典，
                     解析失败（字段不足或解析异常）返回None
    """
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 8:
        return None
    d = {"raw": line, "parts": parts}
    try:
        d["id"] = int(parts[0])
        d["name"] = parts[1]
        d["lng"] = float(parts[2])
        d["lat"] = float(parts[3])
    except (ValueError, IndexError):
        return None
    return d


def parse_spot_line(line):
    """
    解析景点数据行

    景点数据格式: ID|名称|描述|地址|经度|纬度|类型|票价|营业时间|推荐时长|最佳季节|评分|标签...

    参数:
        line (str): 景点数据行字符串

    返回:
        dict | None: 解析成功返回包含 raw/parts/id/name/lng/lat 的字典，
                     解析失败返回None
    """
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 6:
        return None
    d = {"raw": line, "parts": parts}
    try:
        d["id"] = int(parts[0])
        d["name"] = parts[1]
        d["lng"] = float(parts[4])              # 景点的经度在第5个字段
        d["lat"] = float(parts[5])              # 纬度在第6个字段
    except (ValueError, IndexError):
        return None
    return d


def get_address_idx_food(parts):
    """
    判断美食数据行中地址字段的索引位置

    启发式判断策略：
    检查第8个字段（parts[7]）是否包含地址特征词（区/路/街/市/县）
    如果包含，则认为该字段是地址字段，返回索引7
    这种方法允许数据行有可选的字段（如营业时间），灵活适配不同格式

    参数:
        parts (list): 美食数据行的字段列表

    返回:
        int | None: 地址字段的索引位置，无法判断返回None
    """
    if len(parts) < 9:
        return None
    part8 = parts[7]
    # 判断该字段内容是否具有地址特征
    is_address = ("区" in part8 or "路" in part8 or "街" in part8 or
                  "市" in part8 or "县" in part8 or part8 in ("-", ""))
    if is_address:
        return 7
    return None


def get_address_parts_idx(parts, addr_idx):
    """
    检查指定索引位置的地址字段是否缺失（空字符串或"-"）

    参数:
        parts (list): 数据行的字段列表
        addr_idx (int): 地址字段索引

    返回:
        bool: True表示地址缺失需要补全，False表示已有有效地址
    """
    if addr_idx is not None and addr_idx < len(parts):
        return parts[addr_idx] in ("", "-", None)
    return False


def fill_addresses(filepath, parse_fn, get_addr_idx_fn, key, label):
    """
    核心地址补全逻辑：读取文件、查找缺失地址、调用API、回写文件

    处理流程：
    1. 逐行读取数据文件
    2. 解析每个有效数据行，定位地址字段
    3. 若地址缺失（空或"-"），调用逆地理编码API获取地址
    4. 使用缓存机制：相同坐标的POI共享同一次API查询结果
    5. 每补全10条输出一次进度
    6. 处理完成后整体回写文件

    参数:
        filepath (str): 数据文件路径
        parse_fn (callable): 解析数据行的函数（food用 parse_food_line）
        get_addr_idx_fn (callable): 获取地址字段索引的函数
        key (str): 高德API Key
        label (str): 数据类型标签（"foods" 或 "spots"），用于日志输出

    返回:
        int: 实际补全的地址条目数量
    """
    lines = []
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    addr_cache = {}                             # 坐标->地址缓存，避免重复API调用
    updated = 0
    new_lines = []

    for line in lines:
        original_line = line
        line_stripped = line.strip()
        # 保留空行和注释行不变
        if not line_stripped or line_stripped.startswith("#"):
            new_lines.append(line)
            continue

        info = parse_fn(line_stripped)
        if not info:                            # 解析失败的行原样保留
            new_lines.append(line)
            continue

        addr_idx = get_addr_idx_fn(info["parts"])
        if addr_idx is None:                    # 无法确定地址位置的行原样保留
            new_lines.append(line)
            continue

        if addr_idx >= len(info["parts"]):
            new_lines.append(line)
            continue

        current_addr = info["parts"][addr_idx] if addr_idx < len(info["parts"]) else ""
        if current_addr not in ("", "-", None):
            # 已有有效地址，无需补全
            new_lines.append(line)
            continue

        # 使用坐标作为缓存键，避免对同一位置重复调用API
        cache_key = "{:.5f},{:.5f}".format(info["lng"], info["lat"])
        if cache_key in addr_cache:
            addr = addr_cache[cache_key]
        else:
            print("  Geocoding {} (id={})...".format(info["name"], info["id"]))
            addr = reverse_geocode(key, info["lng"], info["lat"])
            time.sleep(0.1)                     # 控制请求频率
            if addr:
                addr_cache[cache_key] = addr
            else:
                addr_cache[cache_key] = "无法获取"
                addr = "无法获取"

        # 更新地址字段并重新构建数据行
        parts = list(info["parts"])
        if addr_idx < len(parts):
            parts[addr_idx] = addr.replace("|", " ")   # 替换管道符避免破坏格式
        new_lines.append("|".join(parts) + "\n")
        updated += 1
        if updated % 10 == 0:
            print("  Updated {} {} entries...".format(updated, label))

    # 如果有修改，整体写回文件
    if updated > 0:
        with open(filepath, "w", encoding="utf-8") as f:
            f.writelines(new_lines)

    return updated


def main():
    """
    主函数：执行地址补全的完整流程

    流程：
    1. 加载高德API配置
    2. 统计各数据文件中缺失地址的条目数量
    3. 对美食文件调用 fill_addresses 补全地址
    4. 对景点文件调用 fill_addresses 补全地址
    5. 输出补全结果统计和下一步提示

    注意：
    - 景点文件中地址字段固定位于parts[3]（第4个字段）
    - 美食文件中地址字段位置不固定（取决于是否有营业时间字段）
    """
    print("=" * 50)
    print("  高德逆地理编码地址补全")
    print("=" * 50)

    config = load_config()
    key = config.get("AMAP_KEY") or config.get("API_KEY")
    if not key:
        print("ERROR: API Key not found")
        return

    print("API Key: {}...".format(key[:8]))

    # 第一阶段：统计各文件中的地址缺失情况
    for fpath, pfn, gfn, label in [
        (FOOD_TXT, parse_food_line, get_address_idx_food, "foods"),
        (SPOT_TXT, parse_spot_line, lambda p: 3 if len(p) >= 4 else None, "spots")
        # 景点地址在第4个字段（索引3）
    ]:
        missing = 0
        with open(fpath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                info = pfn(line)
                if not info: continue
                addr_idx = gfn(info["parts"])
                if addr_idx is not None and addr_idx < len(info["parts"]):
                    addr_val = info["parts"][addr_idx]
                    if addr_val in ("", "-", None):
                        missing += 1
        print("{} missing address: {} entries".format(label, missing))

    print()

    # 第二阶段：实际补全地址
    for fpath, pfn, gfn, label in [
        (FOOD_TXT, parse_food_line, get_address_idx_food, "foods"),
        (SPOT_TXT, parse_spot_line, lambda p: 3 if len(p) >= 4 else None, "spots")
    ]:
        print("[{}] Filling addresses...".format(label))
        n = fill_addresses(fpath, pfn, gfn, key, label)
        print("  Done: updated {} entries".format(n))

    print()
    print("Done! Next: python tools/gen_web_json.py")


if __name__ == "__main__":
    main()
