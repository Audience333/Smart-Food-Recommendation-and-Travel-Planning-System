/**
 * 菏泽美食智能推荐与漫游规划系统
 *
 * C++17 数据结构课程设计
 *
 * 系统架构：
 *   - main.cpp 只负责：初始化、菜单、调用服务
 *   - SystemManager 负责：数据管理、服务协调、用户状态
 *   - 各 Service 负责：具体业务逻辑
 *
 * 数据结构清单：
 *   - SeqList    顺序表（美食/景点主存储）
 *   - LinkedList 双向链表（收藏列表）
 *   - Stack      栈（浏览历史）
 *   - Queue      队列（BFS 遍历）
 *   - HashTable  哈希表（ID索引）
 *   - Heap       堆（Top-K 推荐）
 *   - Graph      图（城市路线网络）
 *   - Trie       Trie树（名称搜索）
 *   - BST        二叉搜索树（评分索引）
 *   - InvertedIndex 倒排索引（标签搜索）
 */

#include <iostream>
#include <string>
#include <limits>

#include "services/SystemManager.h"

using namespace std;

// ==================== 全局系统管理器 ====================
SystemManager g_system;

// ==================== 输入工具 ====================

/**
 * 清除输入缓冲区
 */
void clearInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

/**
 * 读取整数
 */
int readInt(const string& prompt) {
    int value;
    cout << prompt;
    cin >> value;
    clearInput();
    return value;
}

/**
 * 读取字符串
 */
string readLine(const string& prompt) {
    string value;
    cout << prompt;
    getline(cin, value);
    return value;
}

/**
 * 暂停等待用户确认
 */
void pause() {
    cout << endl << "按 Enter 键继续...";
    cin.get();
}

// ==================== 菜单显示 ====================

void showMainMenu() {
    cout << endl;
    cout << "╔══════════════════════════════════════════════════════════════╗" << endl;
    cout << "║           菏泽美食智能推荐与漫游系统                          ║" << endl;
    cout << "╠══════════════════════════════════════════════════════════════╣" << endl;
    cout << "║  1. 浏览全部美食                                             ║" << endl;
    cout << "║  2. 搜索美食（名称/标签/评分）                                ║" << endl;
    cout << "║  3. 标签查询                                                 ║" << endl;
    cout << "║  4. 查看个性推荐                                             ║" << endl;
    cout << "║  5. 附近美食/景点搜索                                         ║" << endl;
    cout << "║  6. 城市漫游路线规划                                          ║" << endl;
    cout << "║  7. 查看地图（导出数据）                                       ║" << endl;
    cout << "║  8. 收藏管理                                                 ║" << endl;
    cout << "║  9. 查看浏览历史                                             ║" << endl;
    cout << "║  10. 完整演示流程                                             ║" << endl;
    cout << "║  0. 退出系统                                                 ║" << endl;
    cout << "╚══════════════════════════════════════════════════════════════╝" << endl;
}

// ==================== 功能实现 ====================

/**
 * 1. 浏览全部美食
 */
void handleBrowseFoods() {
    g_system.showAllFoods();

    int foodId = readInt("输入美食 ID 查看详情（0 返回）: ");
    if (foodId > 0) {
        g_system.viewFoodDetail(foodId);
    }
}

/**
 * 2. 搜索美食
 */
void handleSearchFood() {
    cout << endl;
    cout << "=== 搜索美食 ===" << endl;
    cout << "  1. 按名称搜索" << endl;
    cout << "  2. 按标签搜索" << endl;
    cout << "  3. 按评分搜索" << endl;

    int choice = readInt("请选择: ");

    switch (choice) {
        case 1: {
            string prefix = readLine("请输入名称前缀: ");
            g_system.searchByName(prefix);
            break;
        }
        case 2: {
            string tag = readLine("请输入标签: ");
            g_system.searchByTag(tag);
            break;
        }
        case 3: {
            double minScore = 0;
            cout << "请输入最低评分 (1-5): ";
            cin >> minScore;
            clearInput();
            g_system.searchByScore(minScore);
            break;
        }
        default:
            cout << "无效选择" << endl;
    }
}

/**
 * 3. 标签查询
 */
void handleTagQuery() {
    cout << endl;
    cout << "=== 可用标签 ===" << endl;

    auto tags = g_system.getAllTags();
    for (int i = 0; i < tags.size(); i++) {
        cout << "  " << tags[i];
        if ((i + 1) % 5 == 0) cout << endl;
    }
    cout << endl;

    string tag = readLine("请输入标签进行搜索: ");
    if (!tag.empty()) {
        g_system.searchByTag(tag);
    }
}

/**
 * 4. 查看个性推荐
 */
void handleRecommendation() {
    g_system.showRecommendation(5);
}

/**
 * 5. 附近搜索
 */
void handleNearbySearch() {
    cout << endl;
    cout << "=== 附近搜索 ===" << endl;
    cout << "  1. 搜索附近美食" << endl;
    cout << "  2. 搜索附近景点" << endl;

    int choice = readInt("请选择: ");

    int locationId = readInt("请输入当前位置 ID: ");
    int layers = readInt("请输入搜索层数 (1-5): ");

    switch (choice) {
        case 1:
            g_system.searchNearbyFood(locationId, layers);
            break;
        case 2:
            g_system.searchNearbySpots(locationId, layers);
            break;
        default:
            cout << "无效选择" << endl;
    }
}

/**
 * 6. 路线规划
 */
void handleRoutePlanning() {
    cout << endl;
    cout << "=== 路线规划 ===" << endl;
    cout << "  1. 最短路径" << endl;
    cout << "  2. 最快路径" << endl;
    cout << "  3. 美食漫游路线" << endl;

    int choice = readInt("请选择: ");

    switch (choice) {
        case 1: {
            int fromId = readInt("请输入起点 ID: ");
            int toId = readInt("请输入终点 ID: ");
            g_system.findShortestPath(fromId, toId);
            break;
        }
        case 2: {
            int fromId = readInt("请输入起点 ID: ");
            int toId = readInt("请输入终点 ID: ");
            g_system.findFastestPath(fromId, toId);
            break;
        }
        case 3: {
            int startId = readInt("请输入起点 ID: ");
            cout << "请输入途经美食 ID（输入 0 结束）:" << endl;

            SeqList<int> foodIds;
            while (true) {
                int id = readInt("  美食 ID: ");
                if (id == 0) break;
                foodIds.push_back(id);
            }

            if (foodIds.size() > 0) {
                g_system.planFoodTour(startId, foodIds);
            } else {
                cout << "未输入途经点" << endl;
            }
            break;
        }
        default:
            cout << "无效选择" << endl;
    }
}

/**
 * 7. 查看地图
 */
void handleShowMap() {
    g_system.exportMapData();

    cout << endl;
    cout << "========================================" << endl;
    cout << "  地图数据已导出！" << endl;
    cout << "  " << endl;
    cout << "  查看方式:" << endl;
    cout << "  1. 用浏览器打开 web/index.html" << endl;
    cout << "  2. 或启动本地服务器:" << endl;
    cout << "     cd web && python -m http.server 8080" << endl;
    cout << "     然后访问 http://localhost:8080" << endl;
    cout << "========================================" << endl;
}

/**
 * 8. 收藏管理
 */
void handleFavorites() {
    cout << endl;
    cout << "=== 收藏管理 ===" << endl;
    cout << "  1. 查看收藏列表" << endl;
    cout << "  2. 添加收藏" << endl;
    cout << "  3. 取消收藏" << endl;
    cout << "  4. 反向显示收藏（链表特性演示）" << endl;

    int choice = readInt("请选择: ");

    switch (choice) {
        case 1:
            g_system.showFavorites();
            break;
        case 2: {
            g_system.showAllFoods();
            int foodId = readInt("请输入要收藏的美食 ID: ");
            g_system.addFavorite(foodId);
            break;
        }
        case 3: {
            g_system.showFavorites();
            int foodId = readInt("请输入要取消收藏的美食 ID: ");
            g_system.removeFavorite(foodId);
            break;
        }
        case 4:
            g_system.showFavoritesReverse();
            break;
        default:
            cout << "无效选择" << endl;
    }
}

/**
 * 9. 查看历史
 */
void handleHistory() {
    g_system.showHistory();
}

/**
 * 10. 完整演示流程
 */
void handleDemo() {
    cout << endl;
    cout << "╔══════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    完整演示流程                              ║" << endl;
    cout << "╚══════════════════════════════════════════════════════════════╝" << endl;

    // 步骤 1：显示用户当前偏好
    cout << endl << "【步骤 1】当前用户偏好" << endl;
    RecommendService::printUserPreference(g_system.getCurrentUser());

    // 步骤 2：获取个性化推荐
    cout << endl << "【步骤 2】获取个性化推荐 Top 5" << endl;
    g_system.showRecommendation(5);

    // 步骤 3：浏览一个美食
    cout << "【步骤 3】浏览美食详情" << endl;
    g_system.viewFoodDetail(1);  // 菏泽烧牛肉

    // 步骤 4：收藏该美食
    cout << "【步骤 4】收藏美食" << endl;
    g_system.addFavorite(1);

    // 步骤 5：查看收藏列表
    cout << "【步骤 5】查看收藏列表" << endl;
    g_system.showFavorites();

    // 步骤 6：查看浏览历史
    cout << "【步骤 6】查看浏览历史" << endl;
    g_system.showHistory();

    // 步骤 7：规划路线
    cout << "【步骤 7】规划路线：曹州牡丹园 → 菏泽烧牛肉" << endl;
    g_system.findShortestPath(1, 101);

    // 步骤 8：导出地图数据
    cout << "【步骤 8】导出地图数据" << endl;
    g_system.exportMapData();

    // 步骤 9：显示更新后的偏好
    cout << "【步骤 9】更新后的用户偏好" << endl;
    RecommendService::printUserPreference(g_system.getCurrentUser());

    // 步骤 10：再次获取推荐（偏好已更新）
    cout << endl << "【步骤 10】更新后的推荐结果" << endl;
    g_system.showRecommendation(5);

    cout << endl;
    cout << "╔══════════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    演示完成！                                ║" << endl;
    cout << "║  系统已记录您的浏览和收藏行为，并更新了推荐偏好              ║" << endl;
    cout << "╚══════════════════════════════════════════════════════════════╝" << endl;
}

// ==================== 主函数 ====================

int main() {
    // 初始化系统
    if (!g_system.initialize()) {
        cerr << "系统初始化失败！" << endl;
        return 1;
    }

    // 主菜单循环
    int choice = -1;
    while (choice != 0) {
        showMainMenu();
        choice = readInt("请选择功能: ");

        switch (choice) {
            case 1:  handleBrowseFoods();      break;
            case 2:  handleSearchFood();        break;
            case 3:  handleTagQuery();          break;
            case 4:  handleRecommendation();    break;
            case 5:  handleNearbySearch();      break;
            case 6:  handleRoutePlanning();     break;
            case 7:  handleShowMap();           break;
            case 8:  handleFavorites();         break;
            case 9:  handleHistory();           break;
            case 10: handleDemo();              break;
            case 0:
                g_system.save();
                cout << endl;
                cout << "╔══════════════════════════════════════════════╗" << endl;
                cout << "║     感谢使用菏泽美食推荐系统，再见！         ║" << endl;
                cout << "╚══════════════════════════════════════════════╝" << endl;
                break;
            default:
                cout << "无效选择，请重试" << endl;
        }
    }

    return 0;
}
