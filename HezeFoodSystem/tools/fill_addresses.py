"""
高德逆地理编码地址补全脚本
对缺失地址的POI调用逆地理编码API补全地址
"""
import json
import os
import sys
import time
import urllib.request
import urllib.parse

CONFIG_PATH = "config/amap_config.txt"
FOOD_TXT = "data/food.txt"
SPOT_TXT = "data/spot.txt"


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


def reverse_geocode(key, lng, lat):
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
                return data["regeocode"].get("formattedAddress", "")
    except Exception as e:
        print("  Geocode failed for {},{}: {}".format(lng, lat, e))
    return None


def parse_food_line(line):
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
    parts = [p.strip() for p in line.split("|")]
    if len(parts) < 6:
        return None
    d = {"raw": line, "parts": parts}
    try:
        d["id"] = int(parts[0])
        d["name"] = parts[1]
        d["lng"] = float(parts[4])
        d["lat"] = float(parts[5])
    except (ValueError, IndexError):
        return None
    return d


def get_address_idx_food(parts):
    """Determine which index holds the address in food line parts."""
    if len(parts) < 9:
        return None
    part8 = parts[7]
    is_address = ("区" in part8 or "路" in part8 or "街" in part8 or
                  "市" in part8 or "县" in part8 or part8 in ("-", ""))
    if is_address:
        return 7
    return None


def get_address_parts_idx(parts, addr_idx):
    """Check if address at given index is missing (empty or '-')."""
    if addr_idx is not None and addr_idx < len(parts):
        return parts[addr_idx] in ("", "-", None)
    return False


def fill_addresses(filepath, parse_fn, get_addr_idx_fn, key, label):
    lines = []
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    addr_cache = {}
    updated = 0
    new_lines = []

    for line in lines:
        original_line = line
        line_stripped = line.strip()
        if not line_stripped or line_stripped.startswith("#"):
            new_lines.append(line)
            continue

        info = parse_fn(line_stripped)
        if not info:
            new_lines.append(line)
            continue

        addr_idx = get_addr_idx_fn(info["parts"])
        if addr_idx is None:
            new_lines.append(line)
            continue

        if addr_idx >= len(info["parts"]):
            new_lines.append(line)
            continue

        current_addr = info["parts"][addr_idx] if addr_idx < len(info["parts"]) else ""
        if current_addr not in ("", "-", None):
            new_lines.append(line)
            continue

        cache_key = "{:.5f},{:.5f}".format(info["lng"], info["lat"])
        if cache_key in addr_cache:
            addr = addr_cache[cache_key]
        else:
            print("  Geocoding {} (id={})...".format(info["name"], info["id"]))
            addr = reverse_geocode(key, info["lng"], info["lat"])
            time.sleep(0.1)
            if addr:
                addr_cache[cache_key] = addr
            else:
                addr_cache[cache_key] = "无法获取"
                addr = "无法获取"

        parts = list(info["parts"])
        if addr_idx < len(parts):
            parts[addr_idx] = addr.replace("|", " ")
        new_lines.append("|".join(parts) + "\n")
        updated += 1
        if updated % 10 == 0:
            print("  Updated {} {} entries...".format(updated, label))

    if updated > 0:
        with open(filepath, "w", encoding="utf-8") as f:
            f.writelines(new_lines)

    return updated


def main():
    print("=" * 50)
    print("  高德逆地理编码地址补全")
    print("=" * 50)

    config = load_config()
    key = config.get("AMAP_KEY") or config.get("API_KEY")
    if not key:
        print("ERROR: API Key not found")
        return

    print("API Key: {}...".format(key[:8]))

    for fpath, pfn, gfn, label in [
        (FOOD_TXT, parse_food_line, get_address_idx_food, "foods"),
        (SPOT_TXT, parse_spot_line, lambda p: 3 if len(p) >= 4 else None, "spots")
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
