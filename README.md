# 菏泽美食推荐与城市漫游规划系统

> C++17 数据结构课程设计项目

## 项目简介

以菏泽美食和景点数据为基础，综合运用 **10 种手写数据结构**，实现美食推荐、路线规划、地图可视化等功能。

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                    src/main.cpp                          │
│                  用户交互层（菜单系统）                    │
└─────────────────────────┬───────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────┐
│               src/services/SystemManager.h               │
│                    系统管理器                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                │
│  │foods_    │ │favorites_│ │history_  │                │
│  │SeqList   │ │LinkedList│ │Stack     │                │
│  └──────────┘ └──────────┘ └──────────┘                │
└────┬──────────────────┬──────────────────┬──────────────┘
     ▼                  ▼                  ▼
┌──────────┐     ┌──────────┐     ┌──────────┐
│搜索服务   │     │推荐服务   │     │路线服务   │
│Trie      │     │Heap      │     │Graph     │
│InvertedIdx│    │Similarity│     │Dijkstra  │
│BST       │     │Feature   │     │BFS       │
└──────────┘     └──────────┘     └──────────┘
     │                  │                  │
     └──────────────────┼──────────────────┘
                        ▼
              src/structure/（10种数据结构）
              src/algorithm/（4种算法）
                        │
                        ▼
              src/visualization/MapExporter.h
                        │
                        ▼
              web/（高德地图可视化）
```

## 项目结构

```
HezeFoodSystem/
│
├── src/                        # 源代码
│   ├── main.cpp                # 主程序入口（菜单系统）
│   │
│   ├── model/                  # 数据模型
│   │   ├── Food.h              # 美食结构体
│   │   ├── Spot.h              # 景点结构体
│   │   ├── User.h              # 用户结构体 + 偏好向量
│   │   └── Route.h             # 路线结果结构体
│   │
│   ├── structure/              # 数据结构（全部手写）
│   │   ├── SeqList.h           # 顺序表
│   │   ├── LinkedList.h        # 双向链表
│   │   ├── Stack.h             # 栈
│   │   ├── Queue.h             # 队列
│   │   ├── HashTable.h         # 哈希表
│   │   ├── Heap.h              # 堆
│   │   ├── Graph.h             # 图（邻接表）
│   │   ├── BST.h               # 二叉搜索树
│   │   ├── Trie.h              # Trie 树
│   │   └── InvertedIndex.h     # 倒排索引
│   │
│   ├── algorithm/              # 算法实现
│   │   ├── Dijkstra.h          # 最短路径
│   │   ├── BFS.h               # 广度优先搜索
│   │   ├── Similarity.h        # 余弦相似度
│   │   └── FoodFeature.h       # 美食特征提取
│   │
│   ├── services/               # 业务服务
│   │   ├── SystemManager.h     # 系统管理器
│   │   ├── FoodSearchService.h # 搜索服务
│   │   ├── RecommendService.h  # 推荐服务
│   │   └── RouteService.h      # 路线服务
│   │
│   ├── visualization/          # 可视化
│   │   └── MapExporter.h       # 数据导出（C++ → JSON）
│   │
│   └── utils/                  # 工具类
│       ├── FileManager.h       # 文件管理
│       └── GeoUtil.h           # 地理计算（Haversine）
│
├── data/                       # 数据文件
│   ├── food.txt                # 美食数据（63条）
│   ├── spot.txt                # 景点数据（10条）
│   ├── road.txt                # 道路连接数据
│   ├── favorite.txt            # 用户收藏
│   ├── history.txt             # 浏览历史
│   └── user.txt                # 用户偏好
│
├── config/                     # 配置文件
│   └── amap_config.txt         # 高德 API 配置
│
├── web/                        # 地图可视化
│   ├── index.html              # 主页面
│   ├── style.css               # 样式
│   ├── map.js                  # 地图逻辑
│   └── data/                   # JSON 数据（运行时生成）
│
└── docs/                       # 文档
```

## 数据结构用途

| 数据结构 | 文件 | 系统用途 |
|---------|------|---------|
| SeqList | structure/SeqList.h | 美食/景点主存储、搜索结果 |
| LinkedList | structure/LinkedList.h | 用户收藏列表（O(1)增删） |
| Stack | structure/Stack.h | 浏览历史（LIFO） |
| Queue | structure/Queue.h | BFS 遍历队列 |
| HashTable | structure/HashTable.h | ID 索引、快速查找 |
| Heap | structure/Heap.h | Top-K 推荐、Dijkstra 优先队列 |
| Graph | structure/Graph.h | 城市路线网络（邻接表） |
| BST | structure/BST.h | 评分范围查询 |
| Trie | structure/Trie.h | 名称前缀搜索 |
| InvertedIndex | structure/InvertedIndex.h | 标签倒排索引 |

## 编译运行

```bash
cd HezeFoodSystem

# 编译主程序
g++ -std=c++17 -o main src/main.cpp

# 运行
./main

# 查看地图（需要先导出数据）
cd web
python -m http.server 8080
# 浏览器访问 http://localhost:8080
```

## 功能菜单

```
╔══════════════════════════════════════════════════════════════╗
║           菏泽美食智能推荐与漫游系统                          ║
╠══════════════════════════════════════════════════════════════╣
║  1. 浏览全部美食           6. 城市漫游路线规划                ║
║  2. 搜索美食               7. 查看地图（导出数据）            ║
║  3. 标签查询               8. 收藏管理                       ║
║  4. 查看个性推荐           9. 查看浏览历史                   ║
║  5. 附近美食/景点搜索      10. 完整演示流程                   ║
║  0. 退出系统                                                ║
╚══════════════════════════════════════════════════════════════╝
```

## 文档

- [系统需求说明书](docs/01-系统需求说明书.md)
- [技术架构设计](docs/02-技术架构设计.md)
- [数据库设计](docs/04-数据库设计.md)
