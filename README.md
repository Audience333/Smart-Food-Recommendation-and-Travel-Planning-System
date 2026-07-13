# 成都美食智能推荐系统

## 项目概述

本系统是一个基于C++数据结构和高德地图API的成都美食智能推荐系统，提供景点和美食数据的采集、存储、查询和可视化功能。

## 技术栈

- **后端**: C++ (数据结构与算法)
- **前端**: HTML/CSS/JavaScript
- **地图**: 高德地图 JavaScript API 2.0
- **数据**: JSON格式

## 文件说明

### 核心文件

| 文件 | 说明 |
|------|------|
| food_system.cpp | C++源代码（图、树、哈希表） |
| food_system.exe | 编译后的可执行程序 |
| map.html | 交互式地图页面 |
| scenic.json | 景点数据（12个） |
| food.json | 美食数据（1154家） |

### 文档文件

| 文件 | 说明 |
|------|------|
| README.md | 本说明文档 |
| 数据采集报告.md | 数据采集详细报告 |

## C++ 数据结构

### 1. 图（邻接表实现）

用于路线规划和最短路径计算。

```cpp
class Graph {
    int V;  // 顶点数
    map<int, AdjNode*> adjList;  // 邻接表
    map<int, string> vertexNames;  // 顶点名称

    void addEdge(int from, int to, int weight);
    vector<int> dijkstra(int start, int end, int& totalTime);
};
```

**应用场景**: 景点间步行路线规划

### 2. 二叉搜索树（BST）

用于评论数据的有序存储和检索。

```cpp
class CommentBST {
    BSTNode* root;

    BSTNode* insert(BSTNode* node, Comment comment);
    vector<Comment> getAllSorted();
};
```

**应用场景**: 按评分排序的评论展示

### 3. 哈希表

用于门店数据的快速查找。

```cpp
class FoodStoreHashTable {
    static const int TABLE_SIZE = 1000;
    vector<FoodStore*> table[TABLE_SIZE];

    void insert(FoodStore* store);
    FoodStore* find(int id);
};
```

**应用场景**: 根据门店ID快速查询详情

## 数据结构定义

### 景点结构体

```cpp
struct Scenic {
    int id;
    string name;
    string address;
    double lng, lat;
    string poi_id;
    string type;
    string adname;
};
```

### 美食门店结构体

```cpp
struct FoodStore {
    int id;
    string name;
    string address;
    string district;
    double lng, lat;
    string poi_id;
    string type;
    int cost;
    double rating;
    double meituan_rating;
    int meituan_comments;
    vector<string> meituan_reviews;
    vector<string> user_comments;
};
```

### 评论结构体

```cpp
struct Comment {
    int id;
    int store_id;
    string username;
    string content;
    int rating;
    string timestamp;
};
```

## C++ 程序功能

### 主菜单

```
=== 功能菜单 ===
1. 查看商圈统计
2. 查看评分TOP门店
3. 按价格筛选
4. 查看门店评论
5. 添加评论
6. 查找最短路径
7. 生成评论页面
0. 退出
```

### 功能说明

1. **商圈统计**: 统计各商圈门店数量、人均消费、平均评分
2. **评分TOP**: 显示评分最高的门店排行
3. **价格筛选**: 按价格范围筛选门店
4. **门店评论**: 查看门店的美团评论和用户评论
5. **添加评论**: 用户添加新的评论
6. **最短路径**: 使用Dijkstra算法计算景点间最短步行时间
7. **评论页面**: 生成独立的评论HTML页面

## 地图功能

### 标记样式

- **景点标记**: 紫色圆形 (#667eea)
- **美食标记**: 红色圆形 (#e74c3c)
- **高评分标记**: 金色圆形 (#f5a623, 评分>=4.7)

### 功能列表

1. **图层控制**: 切换显示景点/美食
2. **聚类显示**: 标记自动聚类
3. **热力图**: 美食分布密度
4. **搜索功能**: 搜索景点或美食名称
5. **商圈统计**: 显示各商圈门店信息
6. **信息窗口**: 点击标记查看详情
7. **评论留言**: 用户可提交评论

### 评论功能

地图右下角提供评论面板：
- 选择门店
- 输入昵称
- 选择评分
- 输入评论内容
- 提交评论

## 编译运行

### 编译C++程序

```bash
g++ -o food_system food_system.cpp -std=c++11
```

### 运行程序

```bash
./food_system
```

### 打开地图

直接在浏览器中打开 `map.html`

## 数据来源

- **景点数据**: 高德地图开放平台 API
- **美食数据**: 高德地图开放平台 API
- **评分评论**: 美团 API（模拟数据）

## 商圈覆盖

| 商圈 | 门店数量 | 人均消费 | 平均评分 |
|------|----------|----------|----------|
| 环球中心 | 145家 | 120元 | 4.5 |
| 东郊记忆 | 144家 | 95元 | 4.4 |
| 文殊院 | 138家 | 65元 | 4.3 |
| 玉林 | 130家 | 75元 | 4.5 |
| 九眼桥 | 129家 | 85元 | 4.4 |
| 春熙路 | 127家 | 90元 | 4.5 |
| 建设路 | 116家 | 55元 | 4.3 |
| 宽窄巷子 | 111家 | 70元 | 4.4 |
| 锦里 | 91家 | 80元 | 4.3 |
| 太古里 | 23家 | 150元 | 4.6 |

## 景点列表

| ID | 景点名称 | 区域 |
|----|----------|------|
| 1 | 春熙路步行街 | 锦江区 |
| 2 | 成都太古里 | 锦江区 |
| 3 | 宽窄巷子景区 | 青羊区 |
| 4 | 锦里古街 | 武侯区 |
| 5 | 成都武侯祠博物馆 | 武侯区 |
| 6 | 成都杜甫草堂博物馆 | 青羊区 |
| 7 | 文殊院 | 青羊区 |
| 8 | 人民公园 | 青羊区 |
| 9 | 东郊记忆 | 成华区 |
| 10 | 成都大熊猫繁育研究基地 | 成华区 |
| 11 | 环球中心 | 武侯区 |
| 12 | 金沙遗址博物馆 | 青羊区 |

## 路线数据

| 起点 | 终点 | 步行时间(分钟) |
|------|------|----------------|
| 春熙路 | 太古里 | 2 |
| 春熙路 | 宽窄巷子 | 37 |
| 春熙路 | 人民公园 | 40 |
| 春熙路 | 文殊院 | 39 |
| 宽窄巷子 | 人民公园 | 15 |
| 宽窄巷子 | 文殊院 | 28 |
| 锦里 | 武侯祠 | 18 |
| 锦里 | 杜甫草堂 | 43 |
| 人民公园 | 文殊院 | 35 |
| 建设路 | 东郊记忆 | 13 |

## 注意事项

1. 地图需要联网加载高德地图API
2. 评论功能仅在当前会话有效
3. C++程序需要C++11或更高版本编译器
4. 建议使用Chrome或Firefox浏览器

## 更新日志

### v2.0 (2026-07-13)
- 添加C++数据结构实现
- 移除所有emoji图标
- 添加评论留言功能
- 优化标记样式
- 添加最短路径算法

### v1.0 (2026-07-13)
- 初始版本
- 高德API数据采集
- 美团数据集成
- 交互式地图
