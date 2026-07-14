# 菏泽美食推荐与城市漫游规划系统

> C++17 数据结构课程设计 + 高德地图 Web 可视化 + Python 数据管道

## 项目简介

以菏泽市 **288** 条真实美食门店和 **110** 个景点数据为基础，综合运用 **10 种手写数据结构** 实现 C++ 后端，通过 Python 数据管道对接高德地图 API 获取真实 POI 数据，前端提供完整的地图交互、搜索筛选、路线规划、收藏管理、用户画像等实用功能。

## 技术栈

| 层次 | 技术 | 用途 |
|------|------|------|
| C++ 后端 | C++17 + 手写数据结构 | 核心算法：搜索/推荐/路径规划 |
| Python 数据管道 | Python 3 + urllib | 高德API数据采集/补全/转换 |
| Web 前端 | Vanilla JS + 高德地图 JS API 2.0 | 地图可视化、人机交互 |
| 数据存储 | TXT (管道分隔) + JSON | 轻量级文件存储 |

## 项目结构

```
HezeFoodSystem/
├── src/                          # C++17 后端
│   ├── main.cpp                  # 控制台菜单系统入口
│   ├── model/                    # 数据模型 (Food.h, Spot.h, User.h, Route.h)
│   ├── structure/                # 10 种手写数据结构
│   │   ├── SeqList.h             #   顺序表
│   │   ├── LinkedList.h          #   双向链表
│   │   ├── Stack.h               #   栈
│   │   ├── Queue.h               #   队列
│   │   ├── HashTable.h           #   哈希表
│   │   ├── Heap.h                #   堆
│   │   ├── Graph.h               #   图（邻接表）
│   │   ├── BST.h                 #   二叉搜索树
│   │   ├── Trie.h                #   Trie 树
│   │   └── InvertedIndex.h       #   倒排索引
│   ├── algorithm/                # 4 种算法 (Dijkstra, BFS, Similarity, FoodFeature)
│   ├── services/                 # 业务服务 (SystemManager, Search, Recommend, Route)
│   ├── visualization/            # MapExporter (C++ → JSON 导出)
│   └── utils/                    # FileManager, GeoUtil (Haversine)
│
├── data/                         # 源数据文件 (TXT)
│   ├── food.txt                  # 美食数据 (288条, 管道分隔)
│   ├── spot.txt                  # 景点数据 (110条)
│   ├── road.txt                  # 道路连接 (2,782条)
│   ├── favorite.txt              # 用户收藏
│   ├── history.txt               # 浏览历史
│   └── user.txt                  # 用户偏好向量
│
├── config/                       # 配置
│   └── amap_config.txt           # 高德 API Key
│
├── tools/                        # Python 数据管道脚本
│   ├── expand_data.py            # 高德 POI 搜索 + 12维标签生成
│   ├── fetch_amap_data.py        # 原始高德 POI 采集
│   ├── fill_addresses.py         # 逆地理编码地址补全
│   ├── fetch_photos.py           # POI 详情照片获取
│   ├── gen_web_json.py           # TXT → JSON 转换
│   ├── recalc_roads.py           # 道路连接重算 (+ 驾车API)
│   └── renumber_foods.py         # ID 重新编号
│
├── web/                          # Web 前端
│   ├── index.html                # 主页面 (17个功能面板)
│   ├── map.js                    # 地图交互逻辑 (~1800行)
│   ├── style.css                 # 样式表 (~600行)
│   └── data/                     # 前端 JSON 数据
│       ├── food.json             # 美食 (288条)
│       ├── spot.json             # 景点 (110条)
│       └── route.json            # 示例路线
│
├── docs/                         # 项目文档
│   ├── 01-系统需求说明书.md
│   ├── 02-技术架构设计.md
│   ├── 03-项目目录结构.md
│   ├── 04-数据库设计.md
│   ├── 05-标签体系设计.md
│   ├── 06-开发计划.md
│   └── superpowers/              # 迭代设计文档和规格
│       ├── specs/                # 5 份设计规格
│       └── plans/                # 5 份实施计划
│
└── README.md
```

## 数据管道

```
高德地图 API (restapi.amap.com)
    │
    ├── expand_data.py    → food.txt / spot.txt (POI搜索 + 标签生成)
    ├── fill_addresses.py → food.txt / spot.txt (地址补全)
    ├── fetch_photos.py   → food.txt / spot.txt (照片URL获取)
    ├── recalc_roads.py   → road.txt     (道路距离计算)
    │
    └── gen_web_json.py   → web/data/*.json (前端数据)
```

## Web 前端功能

### 图层控制
- 美食标记 / 景点标记 开关切换
- 仅显示营业中 筛选
- 美食分类标签筛选（点击跳转地图到对应区域）

### 搜索
- 实时搜索美食/景点名称、地址、标签
- 搜索结果列表，点击定位到地图标记
- 支持全部/美食/景点类型切换

### 数据统计
- 美食数量 / 景点数量 实时显示

### 路线规划
- 通行方式选择：驾车 / 步行
- 起点终点搜索选择（输入框实时过滤 398 个 POI）
- 途经点添加/删除/排序（最多5个）
- 排序策略：时间优先 / 距离优先 / 过路费少
- 调用高德驾车/步行API获取真实路径距离/时间/过路费
- 路线详情：分段距离/时间/过路费展示

### 美食分类
- 8 大分类标签筛选（汤类/面食/小吃/正餐/烧烤/甜品/饮品/凉菜）
- 多选筛选，实时更新地图标记

### 用户画像
- 口味偏好标签云（自动从收藏分析）
- 价格区间分布条形图
- 偏好品类占比
- 编辑模式：+/- 调整权重 / 从系统中选择标签添加 / 删除
- 保存 / 取消 / 恢复默认

### 一日游推荐
- 智能算法：匹配用户偏好品类 → 筛选高分美食 → 找最近景点
- 生成3个候选方案（含评分/距离/预计时间）
- 换一批随机刷新
- 采纳一键填入路线规划并自动执行

### 排行榜
- 人气榜（评分降序）、性价比榜（评分/价格）、收藏榜
- 前3名奖牌展示，前10名完整信息
- 正序/倒序切换
- 点击跳转地图定位

### 收藏夹
- 收藏/取消收藏（info窗口顶部按钮 + 收藏面板管理）
- localStorage 持久化
- 收藏标记金色边框

### 操作历史（标题栏）
- 撤销 / 重做 按钮
- 历史记录下拉面板（筛选/搜索/收藏/路线）
- 点击历史项跳转到对应状态

### 详情浮窗
- 评分星级 / 价格 / 分类 / 地址 / 营业时间
- 高德真实门店照片（最多3张）
- 12 维度标签展示
- 点击地图空白处关闭浮窗

## C++ 控制台功能

```
╔══════════════════════════════════════════════════════════╗
║                                                        ║
║  1. 浏览全部美食        6. 城市漫游路线规划              ║
║  2. 搜索美食            7. 查看地图（导出数据）          ║
║  3. 标签查询            8. 收藏管理                     ║
║  4. 查看个性推荐        9. 查看浏览历史                  ║
║  5. 附近美食/景点搜索   10. 完整演示流程                  ║
║  0. 退出系统                                            ║
║                                                        ║
╚══════════════════════════════════════════════════════════╝
```

## 10 种手写数据结构用途

| 数据结构 | 系统用途 |
|---------|---------|
| SeqList | 美食/景点主存储、搜索结果 |
| LinkedList | 用户收藏列表（O(1) 增删） |
| Stack | 浏览历史（LIFO） |
| Queue | BFS 遍历队列 |
| HashTable | POI ID 索引、快速查找 |
| Heap | Top-K 推荐、Dijkstra 优先队列 |
| Graph | 城市路线网络（邻接表） |
| BST | 评分范围查询 |
| Trie | 名称前缀搜索 |
| InvertedIndex | 标签倒排索引 |

## 快速开始

### 前端（推荐）

```bash
cd HezeFoodSystem/web
python -m http.server 8080
# 浏览器访问 http://localhost:8080
```

### 数据管道（更新数据时使用）

```bash
cd HezeFoodSystem

# 获取最新高德 POI 数据
python tools/expand_data.py

# 补全缺失地址
python tools/fill_addresses.py

# 获取门店照片
python tools/fetch_photos.py

# 重新计算道路连接
python tools/recalc_roads.py

# 生成前端 JSON
python tools/gen_web_json.py
```

### C++ 后端

```bash
cd HezeFoodSystem
g++ -std=c++17 -o main src/main.cpp
./main
```

## 数据概览

| 指标 | 数值 |
|------|------|
| 美食门店 | 288 条（63 条原有 + 225 条高德API） |
| 景点 | 110 个（20 个原有 + 90 个高德API） |
| 道路连接 | 2,782 条 |
| 照片覆盖率 | 96.7%（385/398） |
| 地址覆盖率 | 100% |
| 美食标签 | 12 维度，平均 16 条/门店 |
| 覆盖区县 | 9 个（牡丹区/单县/曹县/郓城/巨野/东明/定陶/成武/鄄城） |

## 文档

- [系统需求说明书](docs/01-系统需求说明书.md)
- [技术架构设计](docs/02-技术架构设计.md)
- [数据库设计](docs/04-数据库设计.md)
- [标签体系设计](docs/05-标签体系设计.md)
- 设计规格与实施计划：[docs/superpowers/](docs/superpowers/)
