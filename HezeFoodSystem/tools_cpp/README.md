# C++ 数据处理管道

## 概述

`pipeline.cpp` 是一个单文件 C++17 命令行工具，整合了原 Python 工具链的全部功能。通过 `popen("curl ...")` 调用高德地图 Web API，使用定向字符串解析提取 JSON 字段，**无需任何第三方库依赖**。

## 编译

```bash
# Windows (MinGW/Cygwin)
g++ -std=c++17 -O2 pipeline.cpp -o pipeline.exe

# Linux/Mac
g++ -std=c++17 -O2 pipeline.cpp -o pipeline
```

## 命令

| 命令 | 功能 | 读取 | 写入 |
|------|------|------|------|
| `pipeline expand` | 高德API搜索POI，生成12维标签，追加到TXT | config/amap_config.txt, data/food.txt, data/spot.txt | data/food.txt, data/spot.txt |
| `pipeline fill` | 逆地理编码补全缺失地址 | data/food.txt, data/spot.txt | data/food.txt, data/spot.txt |
| `pipeline photos` | 获取每个POI的照片URL | data/food.txt, data/spot.txt | data/food.txt, data/spot.txt |
| `pipeline roads` | 计算道路连接距离和时间 | data/food.txt, data/spot.txt | data/road.txt |
| `pipeline json` | TXT → JSON 转换 | data/food.txt, data/spot.txt | web/data/*.json |
| `pipeline all` | 按顺序执行以上全部步骤 | 同上 | 同上 |

## API 调用

所有网络请求通过 `popen("curl ...")` 执行，调用高德地图以下接口：

- **地点搜索** (`expand`): `restapi.amap.com/v3/place/text`
- **逆地理编码** (`fill`): `restapi.amap.com/v3/geocode/regeo`
- **地点详情** (`photos`): `restapi.amap.com/v3/place/detail`
- **驾车路径** (`roads`): `restapi.amap.com/v3/direction/driving`

API Key 从 `config/amap_config.txt` 读取。

## JSON 解析策略

不使用第三方 JSON 库，而是通过简单字符串搜索提取字段：
- `json_str(json, "name")` — 提取 `"name":"value"` 格式的字符串值
- `json_num(json, "distance")` — 提取数字值
- `json_str_array(json, "photos")` — 提取字符串数组

## 12 维标签生成

`expand` 命令为每条美食生成 12 个维度的标签：

1. 口味口感（麻辣、酸辣、清淡...）
2. 食材主料（羊肉、牛肉、猪肉...）
3. 烹饪方式（烤、涮、炒、炖...）
4. 菜系流派（鲁菜、川菜、清真...）
5. 用餐时段（早餐、午餐、晚餐...）
6. 用餐场景（堂食、外卖、聚餐...）
7. 服务设施（包厢、停车、WiFi...）
8. 荣誉标签（老字号、必吃榜、口碑店...）
9. 价格档次（人均10元以下、10-30元...）
10. 门店状态（24小时营业...）
11. 适合人群（亲子、朋友聚会、商务宴请...）
12. 特色标记（秘制配方、季节限定...）
