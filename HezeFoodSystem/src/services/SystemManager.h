#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include <iostream>
#include <string>
#include <iomanip>

#include "../model/Food.h"
#include "../model/Spot.h"
#include "../model/User.h"
#include "../model/Route.h"
#include "../structure/SeqList.h"
#include "../structure/LinkedList.h"
#include "../structure/Stack.h"
#include "../structure/HashTable.h"
#include "../structure/Graph.h"
#include "../utils/FileManager.h"
#include "../utils/GeoUtil.h"
#include "FoodSearchService.h"
#include "RecommendService.h"
#include "RouteService.h"
#include "../visualization/MapExporter.h"

/**
 * 系统管理器
 *
 * 职责：
 *   1. 统一管理系统初始化和数据加载
 *   2. 提供各服务的统一访问接口
 *   3. 管理用户状态（收藏、历史、偏好）
 *   4. 处理数据持久化（保存/加载）
 *
 * 设计原则：
 *   - main.cpp 只调用 SystemManager 的方法
 *   - 所有业务逻辑封装在此类中
 *   - 各服务通过组合方式集成
 */
class SystemManager {
private:
    // ==================== 数据存储 ====================
    SeqList<Food>           foods_;          // 美食主存储
    SeqList<Spot>           spots_;          // 景点主存储
    User                    currentUser_;    // 当前用户

    // ==================== 业务服务 ====================
    FoodSearchService       searchService_;  // 搜索服务
    RecommendService        recommendService_; // 推荐服务
    RouteService            routeService_;   // 路线服务

    // ==================== 用户状态（链表+栈） ====================
    LinkedList<int>         favorites_;      // 收藏列表（双向链表）
    Stack<int>              history_;        // 浏览历史（栈）

    // ==================== 系统状态 ====================
    bool                    initialized_;    // 是否已初始化
    std::string             dataDir_;        // 数据目录

public:
    SystemManager() : initialized_(false), dataDir_("data/") {}

    // ==================== 初始化 ====================

    /**
     * 初始化系统
     *
     * 流程：
     *   1. 加载美食数据
     *   2. 加载景点数据
     *   3. 构建搜索索引
     *   4. 初始化推荐服务
     *   5. 加载路线数据
     *   6. 加载用户数据
     *
     * @return 是否成功
     */
    bool initialize() {
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║     菏泽美食智能推荐与漫游系统 初始化中...   ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;

        // 1. 加载美食数据
        std::cout << std::endl << "[1/5] 加载美食数据..." << std::endl;
        foods_ = FileManager::loadFoodData(dataDir_ + "food.txt");
        if (foods_.empty()) {
            std::cerr << "错误: 美食数据加载失败" << std::endl;
            return false;
        }

        // 2. 加载景点数据
        std::cout << "[2/5] 加载景点数据..." << std::endl;
        spots_ = FileManager::loadSpotData(dataDir_ + "spot.txt");

        // 3. 构建搜索索引
        std::cout << "[3/5] 构建搜索索引..." << std::endl;
        searchService_.build(foods_);

        // 4. 初始化推荐服务
        std::cout << "[4/5] 初始化推荐服务..." << std::endl;
        recommendService_.init(foods_);

        // 5. 加载路线数据
        std::cout << "[5/5] 加载路线数据..." << std::endl;
        routeService_.load(dataDir_ + "food.txt", dataDir_ + "spot.txt", dataDir_ + "road.txt");

        // 6. 初始化用户
        currentUser_.id = 1;
        currentUser_.username = "默认用户";

        // 7. 加载用户数据
        loadUserData();

        initialized_ = true;

        std::cout << std::endl;
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║     系统初始化完成！                          ║" << std::endl;
        std::cout << "╠══════════════════════════════════════════════╣" << std::endl;
        std::cout << "║  美食数据: " << std::setw(4) << foods_.size() << " 条                          ║" << std::endl;
        std::cout << "║  景点数据: " << std::setw(4) << spots_.size() << " 条                          ║" << std::endl;
        std::cout << "║  收藏数量: " << std::setw(4) << favorites_.size() << " 条                          ║" << std::endl;
        std::cout << "║  历史记录: " << std::setw(4) << history_.size() << " 条                          ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;

        return true;
    }

    /**
     * 保存系统数据
     *
     * 退出时调用：
     *   - 保存用户收藏
     *   - 保存浏览历史
     *   - 保存用户偏好
     */
    void save() {
        std::cout << std::endl << "[SystemManager] 保存数据..." << std::endl;
        saveUserData();
        std::cout << "[SystemManager] 数据保存完成" << std::endl;
    }

    // ==================== 美食浏览 ====================

    /**
     * 显示所有美食列表
     */
    void showAllFoods() {
        std::cout << std::endl;
        std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                    菏泽美食列表                              ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;

        std::cout << std::left
                  << std::setw(5) << "ID"
                  << std::setw(16) << "名称"
                  << std::setw(8) << "分类"
                  << std::setw(10) << "价格"
                  << std::setw(8) << "评分"
                  << "标签" << std::endl;
        std::cout << std::string(75, '-') << std::endl;

        for (int i = 0; i < foods_.size(); i++) {
            const Food& f = foods_[i];
            std::cout << std::left
                      << std::setw(5) << f.id
                      << std::setw(16) << f.name
                      << std::setw(8) << f.category
                      << "¥" << std::setw(9) << std::fixed << std::setprecision(0) << f.price
                      << "★" << std::setw(7) << std::fixed << std::setprecision(1) << f.score
                      << f.tags << std::endl;
        }
        std::cout << std::endl << "共 " << foods_.size() << " 条美食" << std::endl;
    }

    /**
     * 查看美食详情
     *
     * @param foodId 美食 ID
     */
    void viewFoodDetail(int foodId) {
        int idx = findFoodIndex(foodId);
        if (idx == -1) {
            std::cout << "未找到 ID=" << foodId << " 的美食" << std::endl;
            return;
        }

        const Food& f = foods_[idx];

        // 记录浏览历史（使用栈）
        history_.push(foodId);
        currentUser_.addHistory(foodId);

        // 更新偏好
        recommendService_.updatePreferenceByHistory(currentUser_, foodId);

        std::cout << std::endl;
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                 美食详情                      ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
        std::cout << "  ID:     " << f.id << std::endl;
        std::cout << "  名称:   " << f.name << std::endl;
        std::cout << "  分类:   " << f.category << std::endl;
        std::cout << "  价格:   ¥" << std::fixed << std::setprecision(0) << f.price << std::endl;
        std::cout << "  评分:   ★" << std::fixed << std::setprecision(1) << f.score << std::endl;
        std::cout << "  坐标:   (" << std::fixed << std::setprecision(4)
                  << f.longitude << ", " << f.latitude << ")" << std::endl;
        std::cout << "  标签:   " << f.tags << std::endl;
        std::cout << "  收藏:   " << (favorites_.find(foodId) != favorites_.end() ? "已收藏" : "未收藏") << std::endl;
        std::cout << std::endl;
    }

    // ==================== 搜索功能 ====================

    /**
     * 按名称搜索美食
     *
     * @param prefix 名称前缀
     */
    void searchByName(const std::string& prefix) {
        auto results = searchService_.searchByName(prefix);
        FoodSearchService::printResults(results, "名称搜索: " + prefix);
    }

    /**
     * 按标签搜索美食
     *
     * @param tag 标签名称
     */
    void searchByTag(const std::string& tag) {
        auto results = searchService_.searchByTag(tag);
        FoodSearchService::printResults(results, "标签搜索: " + tag);
    }

    /**
     * 按评分搜索美食
     *
     * @param minScore 最低评分
     */
    void searchByScore(double minScore) {
        auto results = searchService_.searchByMinScore(minScore);
        FoodSearchService::printResults(results, "评分筛选: >= " + std::to_string(minScore));
    }

    /**
     * 获取所有标签
     */
    SeqList<std::string> getAllTags() {
        return searchService_.getAllTags();
    }

    // ==================== 推荐功能 ====================

    /**
     * 获取个性化推荐
     *
     * @param topK 推荐数量
     */
    void showRecommendation(int topK = 5) {
        std::cout << std::endl;
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║              个性化推荐                      ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;

        // 显示当前偏好
        RecommendService::printUserPreference(currentUser_);

        // 获取推荐
        auto results = recommendService_.recommendByUser(currentUser_, topK);
        RecommendService::printResults(results, "为您推荐");
    }

    /**
     * 推荐相似美食
     *
     * @param foodId 美食 ID
     * @param topK 推荐数量
     */
    void showSimilarFoods(int foodId, int topK = 5) {
        auto results = recommendService_.recommendSimilar(foodId, topK);
        RecommendService::printResults(results, "相似美食推荐");
    }

    // ==================== 附近搜索 ====================

    /**
     * 搜索附近美食
     *
     * @param locationId 当前位置 ID
     * @param maxLayers 搜索层数
     */
    void searchNearbyFood(int locationId, int maxLayers = 3) {
        auto results = routeService_.findNearbyFood(locationId, maxLayers);
        routeService_.printNearbyResults(results, "附近美食");
    }

    /**
     * 搜索附近景点
     *
     * @param locationId 当前位置 ID
     * @param maxLayers 搜索层数
     */
    void searchNearbySpots(int locationId, int maxLayers = 3) {
        auto results = routeService_.findNearbySpots(locationId, maxLayers);
        routeService_.printNearbyResults(results, "附近景点");
    }

    // ==================== 路线规划 ====================

    /**
     * 规划最短路径
     *
     * @param fromId 起点 ID
     * @param toId 终点 ID
     */
    void findShortestPath(int fromId, int toId) {
        auto result = routeService_.findShortestPath(fromId, toId);
        routeService_.printPathResult(result, "最短路径");

        // 导出路线数据供地图显示
        if (result.found) {
            MapExporter::exportRouteJson(result, routeService_.getCityGraph(),
                                          "最短路径", "web/data/route.json");
            std::cout << "  路线数据已导出到 web/data/route.json" << std::endl;
        }
    }

    /**
     * 规划最快路径
     *
     * @param fromId 起点 ID
     * @param toId 终点 ID
     */
    void findFastestPath(int fromId, int toId) {
        auto result = routeService_.findFastestPath(fromId, toId);
        routeService_.printPathResult(result, "最快路径");

        if (result.found) {
            MapExporter::exportRouteJson(result, routeService_.getCityGraph(),
                                          "最快路径", "web/data/route.json");
            std::cout << "  路线数据已导出到 web/data/route.json" << std::endl;
        }
    }

    /**
     * 美食漫游路线规划
     *
     * @param startId 起点 ID
     * @param foodIds 途经美食 ID 列表
     */
    void planFoodTour(int startId, const SeqList<int>& foodIds) {
        std::cout << std::endl;
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║              美食漫游路线规划                  ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;

        double totalDist = 0;
        double totalTime = 0;
        Dijkstra::PathResult fullPath;
        fullPath.found = true;

        int currentId = startId;
        for (int i = 0; i < foodIds.size(); i++) {
            int nextId = foodIds[i];
            auto result = routeService_.findShortestPath(currentId, nextId);

            if (!result.found) {
                std::cout << "  无法从 " << getNodeName(currentId)
                          << " 到达 " << getNodeName(nextId) << std::endl;
                return;
            }

            routeService_.printPathResult(result,
                "第 " + std::to_string(i + 1) + " 段: " +
                getNodeName(currentId) + " → " + getNodeName(nextId));

            // 合并路径
            for (int j = 0; j < result.path.size() - 1; j++) {
                fullPath.path.push_back(result.path[j]);
            }
            fullPath.totalDistance += result.totalDistance;
            fullPath.totalTime += result.totalTime;

            currentId = nextId;
        }

        // 添加最后一个点
        fullPath.path.push_back(routeService_.findShortestPath(currentId, currentId).path[0]);

        std::cout << std::endl;
        std::cout << "  总距离: " << std::fixed << std::setprecision(0)
                  << fullPath.totalDistance << " 米" << std::endl;
        std::cout << "  总时间: " << std::fixed << std::setprecision(1)
                  << fullPath.totalTime << " 分钟" << std::endl;

        // 导出路线
        MapExporter::exportRouteJson(fullPath, routeService_.getCityGraph(),
                                      "美食漫游路线", "web/data/route.json");
        std::cout << "  路线数据已导出到 web/data/route.json" << std::endl;
    }

    // ==================== 收藏系统（双向链表） ====================

    /**
     * 添加收藏
     *
     * @param foodId 美食 ID
     * @return 是否成功
     */
    bool addFavorite(int foodId) {
        // 检查美食是否存在
        if (findFoodIndex(foodId) == -1) {
            std::cout << "未找到 ID=" << foodId << " 的美食" << std::endl;
            return false;
        }

        // 检查是否已收藏
        if (favorites_.find(foodId) != favorites_.end()) {
            std::cout << "该美食已在收藏列表中" << std::endl;
            return false;
        }

        // 使用双向链表添加收藏
        favorites_.push_back(foodId);
        currentUser_.addFavorite(foodId);

        // 更新偏好
        recommendService_.updatePreferenceByFavorite(currentUser_, foodId);

        std::cout << "收藏成功: " << getFoodName(foodId) << std::endl;
        return true;
    }

    /**
     * 移除收藏
     *
     * @param foodId 美食 ID
     * @return 是否成功
     */
    bool removeFavorite(int foodId) {
        auto it = favorites_.find(foodId);
        if (it == favorites_.end()) {
            std::cout << "该美食不在收藏列表中" << std::endl;
            return false;
        }

        favorites_.erase(it);
        currentUser_.removeFavorite(foodId);

        std::cout << "已取消收藏: " << getFoodName(foodId) << std::endl;
        return true;
    }

    /**
     * 显示收藏列表（正序，体现链表遍历）
     */
    void showFavorites() {
        std::cout << std::endl;
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║              我的收藏                        ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;

        if (favorites_.empty()) {
            std::cout << "  收藏列表为空" << std::endl;
            return;
        }

        std::cout << "  共 " << favorites_.size() << " 条收藏:" << std::endl;
        std::cout << "  " << std::string(60, '-') << std::endl;

        int idx = 1;
        // 使用链表迭代器正序遍历
        for (auto it = favorites_.begin(); it != favorites_.end(); ++it) {
            int foodId = *it;
            const Food* f = getFoodById(foodId);
            if (f) {
                std::cout << "  " << std::left << std::setw(4) << idx++
                          << std::setw(16) << f->name
                          << std::setw(8) << f->category
                          << "¥" << std::setw(7) << std::fixed << std::setprecision(0) << f->price
                          << "★" << std::fixed << std::setprecision(1) << f->score
                          << std::endl;
            }
        }
        std::cout << std::endl;
    }

    /**
     * 反向显示收藏列表（体现双向链表特性）
     */
    void showFavoritesReverse() {
        std::cout << std::endl;
        std::cout << "=== 收藏列表（反序） ===" << std::endl;

        if (favorites_.empty()) {
            std::cout << "  收藏列表为空" << std::endl;
            return;
        }

        // 使用链表迭代器反向遍历
        auto it = favorites_.end();
        --it; // 移动到最后一个元素

        int idx = favorites_.size();
        while (true) {
            int foodId = *it;
            const Food* f = getFoodById(foodId);
            if (f) {
                std::cout << "  " << std::left << std::setw(4) << idx--
                          << std::setw(16) << f->name
                          << f->category << std::endl;
            }

            if (it == favorites_.begin()) break;
            --it;
        }
        std::cout << std::endl;
    }

    // ==================== 浏览历史（栈） ====================

    /**
     * 显示浏览历史（体现栈结构）
     */
    void showHistory() {
        std::cout << std::endl;
        std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
        std::cout << "║              浏览历史                        ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════╝" << std::endl;

        if (history_.empty()) {
            std::cout << "  浏览历史为空" << std::endl;
            return;
        }

        std::cout << "  共 " << history_.size() << " 条记录（最近浏览在前）:" << std::endl;
        std::cout << "  " << std::string(60, '-') << std::endl;

        // 创建临时栈来遍历（不破坏原栈）
        Stack<int> tempStack;
        Stack<int> displayStack;

        // 将历史记录转移到临时栈（反转顺序）
        while (!history_.empty()) {
            int foodId = history_.top();
            history_.pop();
            displayStack.push(foodId);
            tempStack.push(foodId);
        }

        // 恢复原栈
        while (!tempStack.empty()) {
            history_.push(tempStack.top());
            tempStack.pop();
        }

        // 显示历史（栈顶 = 最近浏览）
        int idx = 1;
        while (!displayStack.empty()) {
            int foodId = displayStack.top();
            displayStack.pop();

            const Food* f = getFoodById(foodId);
            if (f) {
                std::cout << "  " << std::left << std::setw(4) << idx++
                          << std::setw(16) << f->name
                          << f->category << std::endl;
            }
        }
        std::cout << std::endl;
    }

    // ==================== 地图导出 ====================

    /**
     * 导出所有地图数据
     */
    void exportMapData() {
        std::cout << std::endl << "[SystemManager] 导出地图数据..." << std::endl;

        // 导出美食数据
        MapExporter::exportFoodJson(foods_, "web/data/food.json");

        // 导出景点数据
        MapExporter::exportSpotJson(spots_, "web/data/spot.json");

        // 如果没有路线数据，生成默认路线
        auto result = routeService_.findShortestPath(1, 101);
        if (result.found) {
            MapExporter::exportRouteJson(result, routeService_.getCityGraph(),
                                          "默认路线", "web/data/route.json");
        }

        std::cout << "[SystemManager] 地图数据导出完成" << std::endl;
        std::cout << "  请用浏览器打开 web/index.html 查看地图" << std::endl;
    }

    // ==================== Getter ====================

    const SeqList<Food>& getFoods() const { return foods_; }
    const SeqList<Spot>& getSpots() const { return spots_; }
    const User& getCurrentUser() const { return currentUser_; }
    bool isInitialized() const { return initialized_; }

    /**
     * 获取节点名称
     */
    std::string getNodeName(int id) const {
        return routeService_.getNodeName(id);
    }

    /**
     * 获取美食名称
     */
    std::string getFoodName(int id) const {
        int idx = findFoodIndex(id);
        if (idx != -1) return foods_[idx].name;
        return "未知";
    }

    /**
     * 获取所有美食 ID
     */
    SeqList<int> getAllFoodIds() const {
        SeqList<int> ids;
        for (int i = 0; i < foods_.size(); i++) {
            ids.push_back(foods_[i].id);
        }
        return ids;
    }

    /**
     * 获取所有景点 ID
     */
    SeqList<int> getAllSpotIds() const {
        SeqList<int> ids;
        for (int i = 0; i < spots_.size(); i++) {
            ids.push_back(spots_[i].id);
        }
        return ids;
    }

private:
    /**
     * 查找美食在列表中的下标
     */
    int findFoodIndex(int foodId) const {
        for (int i = 0; i < foods_.size(); i++) {
            if (foods_[i].id == foodId) return i;
        }
        return -1;
    }

    /**
     * 获取美食对象指针
     */
    const Food* getFoodById(int foodId) const {
        int idx = findFoodIndex(foodId);
        if (idx != -1) return &foods_[idx];
        return nullptr;
    }

    // ==================== 数据持久化 ====================

    /**
     * 保存用户数据
     */
    void saveUserData() {
        // 保存收藏
        std::ofstream favFile(dataDir_ + "favorite.txt");
        if (favFile.is_open()) {
            favFile << "# 用户收藏数据" << std::endl;
            favFile << "# 格式: food_id" << std::endl;
            for (auto it = favorites_.begin(); it != favorites_.end(); ++it) {
                favFile << *it << std::endl;
            }
            favFile.close();
            std::cout << "  收藏数据已保存: " << favorites_.size() << " 条" << std::endl;
        }

        // 保存历史
        std::ofstream histFile(dataDir_ + "history.txt");
        if (histFile.is_open()) {
            histFile << "# 用户浏览历史" << std::endl;
            histFile << "# 格式: food_id（最新在前）" << std::endl;

            // 临时栈用于保存
            Stack<int> tempStack;
            while (!history_.empty()) {
                histFile << history_.top() << std::endl;
                tempStack.push(history_.top());
                history_.pop();
            }
            // 恢复栈
            while (!tempStack.empty()) {
                history_.push(tempStack.top());
                tempStack.pop();
            }

            histFile.close();
            std::cout << "  历史数据已保存: " << history_.size() << " 条" << std::endl;
        }

        // 保存用户偏好
        std::ofstream userFile(dataDir_ + "user.txt");
        if (userFile.is_open()) {
            userFile << "# 用户偏好数据" << std::endl;
            userFile << "username=" << currentUser_.username << std::endl;
            userFile << "preference=";
            for (int i = 0; i < PreferenceVector::DIM; i++) {
                if (i > 0) userFile << ",";
                userFile << std::fixed << std::setprecision(4) << currentUser_.preference[i];
            }
            userFile << std::endl;
            userFile.close();
            std::cout << "  用户偏好已保存" << std::endl;
        }
    }

    /**
     * 加载用户数据
     */
    void loadUserData() {
        // 加载收藏
        std::ifstream favFile(dataDir_ + "favorite.txt");
        if (favFile.is_open()) {
            std::string line;
            while (std::getline(favFile, line)) {
                if (line.empty() || line[0] == '#') continue;
                try {
                    int foodId = std::stoi(FileManager::trim(line));
                    if (findFoodIndex(foodId) != -1) {
                        favorites_.push_back(foodId);
                        currentUser_.addFavorite(foodId);
                    }
                } catch (...) {}
            }
            favFile.close();
            std::cout << "  收藏数据已加载: " << favorites_.size() << " 条" << std::endl;
        }

        // 加载历史
        std::ifstream histFile(dataDir_ + "history.txt");
        if (histFile.is_open()) {
            std::string line;
            Stack<int> tempStack;
            while (std::getline(histFile, line)) {
                if (line.empty() || line[0] == '#') continue;
                try {
                    int foodId = std::stoi(FileManager::trim(line));
                    tempStack.push(foodId);
                } catch (...) {}
            }
            histFile.close();

            // 反转顺序（文件中最新在前，栈需要最新在顶）
            while (!tempStack.empty()) {
                history_.push(tempStack.top());
                currentUser_.addHistory(tempStack.top());
                tempStack.pop();
            }
            std::cout << "  历史数据已加载: " << history_.size() << " 条" << std::endl;
        }

        // 加载用户偏好
        std::ifstream userFile(dataDir_ + "user.txt");
        if (userFile.is_open()) {
            std::string line;
            while (std::getline(userFile, line)) {
                if (line.empty() || line[0] == '#') continue;

                size_t pos = line.find('=');
                if (pos == std::string::npos) continue;

                std::string key = FileManager::trim(line.substr(0, pos));
                std::string value = FileManager::trim(line.substr(pos + 1));

                if (key == "username") {
                    currentUser_.username = value;
                } else if (key == "preference") {
                    // 解析偏好向量
                    SeqList<std::string> parts = FileManager::split(value, ',');
                    for (int i = 0; i < parts.size() && i < PreferenceVector::DIM; i++) {
                        try {
                            currentUser_.preference[i] = std::stod(FileManager::trim(parts[i]));
                        } catch (...) {}
                    }
                }
            }
            userFile.close();
            std::cout << "  用户偏好已加载" << std::endl;
        }
    }
};

#endif // SYSTEMMANAGER_H
