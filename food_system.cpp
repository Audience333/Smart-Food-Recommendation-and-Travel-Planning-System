#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>

using namespace std;

// 景点结构体
struct Scenic {
    int id;
    string name;
    string address;
    double lng;
    double lat;
    string poi_id;
    string type;
    string adname;
};

// 美食门店结构体
struct FoodStore {
    int id;
    string name;
    string address;
    string district;
    double lng;
    double lat;
    string poi_id;
    string type;
    int cost;
    double rating;
    double meituan_rating;
    int meituan_comments;
    vector<string> meituan_reviews;
    vector<string> user_comments;  // 用户评论
};

// 用户评论结构体
struct Comment {
    int id;
    int store_id;
    string username;
    string content;
    int rating;
    string timestamp;
};

// 图的边结构体
struct Edge {
    int from;
    int to;
    int weight;  // 步行时间(分钟)
};

// 邻接表节点
struct AdjNode {
    int vertex;
    int weight;
    AdjNode* next;
};

// 图类(邻接表实现)
class Graph {
private:
    int V;  // 顶点数
    map<int, AdjNode*> adjList;
    map<int, string> vertexNames;

public:
    Graph(int vertices) : V(vertices) {}

    ~Graph() {
        for (auto& pair : adjList) {
            AdjNode* current = pair.second;
            while (current) {
                AdjNode* temp = current;
                current = current->next;
                delete temp;
            }
        }
    }

    void addVertex(int id, const string& name) {
        vertexNames[id] = name;
    }

    void addEdge(int from, int to, int weight) {
        AdjNode* newNode = new AdjNode{to, weight, adjList[from]};
        adjList[from] = newNode;

        // 无向图，添加反向边
        newNode = new AdjNode{from, weight, adjList[to]};
        adjList[to] = newNode;
    }

    string getVertexName(int id) {
        if (vertexNames.find(id) != vertexNames.end()) {
            return vertexNames[id];
        }
        return "Unknown";
    }

    // Dijkstra最短路径算法
    vector<int> dijkstra(int start, int end, int& totalTime) {
        map<int, int> dist;
        map<int, int> prev;
        map<int, bool> visited;

        // 初始化
        for (auto& pair : vertexNames) {
            dist[pair.first] = INT_MAX;
            prev[pair.first] = -1;
            visited[pair.first] = false;
        }
        dist[start] = 0;

        // 优先队列(简单实现)
        for (int i = 0; i < V; i++) {
            // 找未访问的最小距离顶点
            int u = -1;
            int minDist = INT_MAX;
            for (auto& pair : dist) {
                if (!visited[pair.first] && pair.second < minDist) {
                    minDist = pair.second;
                    u = pair.first;
                }
            }

            if (u == -1) break;
            visited[u] = true;

            // 更新邻居距离
            AdjNode* current = adjList[u];
            while (current) {
                int v = current->vertex;
                int w = current->weight;
                if (!visited[v] && dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    prev[v] = u;
                }
                current = current->next;
            }
        }

        // 构建路径
        vector<int> path;
        totalTime = dist[end];
        int current = end;
        while (current != -1) {
            path.insert(path.begin(), current);
            current = prev[current];
        }
        return path;
    }
};

// 二叉搜索树节点(用于评论排序)
struct BSTNode {
    Comment comment;
    BSTNode* left;
    BSTNode* right;

    BSTNode(Comment c) : comment(c), left(nullptr), right(nullptr) {}
};

// 评论二叉搜索树
class CommentBST {
private:
    BSTNode* root;

    BSTNode* insert(BSTNode* node, Comment comment) {
        if (node == nullptr) {
            return new BSTNode(comment);
        }

        if (comment.rating > node->comment.rating) {
            node->right = insert(node->right, comment);
        } else {
            node->left = insert(node->left, comment);
        }
        return node;
    }

    void inorderTraversal(BSTNode* node, vector<Comment>& result) {
        if (node == nullptr) return;
        inorderTraversal(node->left, result);
        result.push_back(node->comment);
        inorderTraversal(node->right, result);
    }

    void destroyTree(BSTNode* node) {
        if (node == nullptr) return;
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }

public:
    CommentBST() : root(nullptr) {}

    ~CommentBST() {
        destroyTree(root);
    }

    void insert(Comment comment) {
        root = insert(root, comment);
    }

    vector<Comment> getAllSorted() {
        vector<Comment> result;
        inorderTraversal(root, result);
        return result;
    }
};

// 哈希表(用于快速查找门店)
class FoodStoreHashTable {
private:
    static const int TABLE_SIZE = 1000;
    vector<FoodStore*> table[TABLE_SIZE];

    int hashFunction(int id) {
        return id % TABLE_SIZE;
    }

public:
    void insert(FoodStore* store) {
        int index = hashFunction(store->id);
        table[index].push_back(store);
    }

    FoodStore* find(int id) {
        int index = hashFunction(id);
        for (FoodStore* store : table[index]) {
            if (store->id == id) {
                return store;
            }
        }
        return nullptr;
    }
};

// 全局数据
vector<Scenic> scenicList;
vector<FoodStore> foodStoreList;
vector<Comment> commentList;
CommentBST commentBST;
FoodStoreHashTable storeHashTable;
Graph* roadGraph = nullptr;

// 从JSON解析简单数据(简化版)
vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// 生成评论时间戳
string generateTimestamp() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);
    return string(buffer);
}

// 添加用户评论
void addComment(int storeId, const string& username, const string& content, int rating) {
    Comment comment;
    comment.id = commentList.size() + 1;
    comment.store_id = storeId;
    comment.username = username;
    comment.content = content;
    comment.rating = rating;
    comment.timestamp = generateTimestamp();

    commentList.push_back(comment);
    commentBST.insert(comment);

    // 添加到门店的用户评论列表
    FoodStore* store = storeHashTable.find(storeId);
    if (store) {
        store->user_comments.push_back(username + ": " + content + " (" + to_string(rating) + "分)");
    }

    cout << "评论添加成功!" << endl;
}

// 显示门店评论
void showStoreComments(int storeId) {
    FoodStore* store = storeHashTable.find(storeId);
    if (!store) {
        cout << "未找到该门店" << endl;
        return;
    }

    cout << "\n=== " << store->name << " 的评论 ===" << endl;
    cout << "地址: " << store->address << endl;
    cout << "商圈: " << store->district << endl;
    cout << "人均: " << store->cost << "元" << endl;
    cout << "高德评分: " << store->rating << endl;
    cout << "美团评分: " << store->meituan_rating << endl;
    cout << "美团评论数: " << store->meituan_comments << endl;

    cout << "\n--- 美团精选评论 ---" << endl;
    for (const string& review : store->meituan_reviews) {
        cout << "* " << review << endl;
    }

    cout << "\n--- 用户评论 ---" << endl;
    if (store->user_comments.empty()) {
        cout << "暂无用户评论" << endl;
    } else {
        for (const string& comment : store->user_comments) {
            cout << "* " << comment << endl;
        }
    }
}

// 查找最短路径
void findShortestPath(int from, int to) {
    if (!roadGraph) {
        cout << "路线图未初始化" << endl;
        return;
    }

    int totalTime;
    vector<int> path = roadGraph->dijkstra(from, to, totalTime);

    if (path.empty() || totalTime == INT_MAX) {
        cout << "未找到可达路径" << endl;
        return;
    }

    cout << "\n=== 最短路径 ===" << endl;
    cout << "从: " << roadGraph->getVertexName(from) << endl;
    cout << "到: " << roadGraph->getVertexName(to) << endl;
    cout << "预计步行时间: " << totalTime << " 分钟" << endl;
    cout << "途经: ";
    for (size_t i = 0; i < path.size(); i++) {
        cout << roadGraph->getVertexName(path[i]);
        if (i < path.size() - 1) cout << " -> ";
    }
    cout << endl;
}

// 按商圈统计
void statisticsByDistrict() {
    map<string, vector<FoodStore*>> districtMap;

    for (auto& store : foodStoreList) {
        districtMap[store.district].push_back(&store);
    }

    cout << "\n=== 商圈统计 ===" << endl;
    for (auto& pair : districtMap) {
        int count = pair.second.size();
        double avgCost = 0;
        double avgRating = 0;

        for (FoodStore* store : pair.second) {
            avgCost += store->cost;
            avgRating += store->meituan_rating;
        }
        avgCost /= count;
        avgRating /= count;

        cout << pair.first << ": " << count << "家"
             << " | 人均: " << fixed << setprecision(0) << avgCost << "元"
             << " | 评分: " << fixed << setprecision(1) << avgRating << endl;
    }
}

// 按评分排序显示
void showTopRatedStores(int topN) {
    vector<FoodStore*> sortedStores;
    for (auto& store : foodStoreList) {
        sortedStores.push_back(&store);
    }

    sort(sortedStores.begin(), sortedStores.end(),
         [](FoodStore* a, FoodStore* b) {
             return a->meituan_rating > b->meituan_rating;
         });

    cout << "\n=== 评分TOP " << topN << " 门店 ===" << endl;
    for (int i = 0; i < min(topN, (int)sortedStores.size()); i++) {
        FoodStore* store = sortedStores[i];
        cout << i + 1 << ". " << store->name
             << " | 商圈: " << store->district
             << " | 美团评分: " << store->meituan_rating
             << " | 人均: " << store->cost << "元" << endl;
    }
}

// 按价格范围筛选
void filterByPriceRange(int minPrice, int maxPrice) {
    cout << "\n=== 价格范围 " << minPrice << "-" << maxPrice << " 元 ===" << endl;
    int count = 0;
    for (auto& store : foodStoreList) {
        if (store.cost >= minPrice && store.cost <= maxPrice) {
            cout << store.name << " | 商圈: " << store.district
                 << " | 人均: " << store.cost << "元"
                 << " | 评分: " << store.meituan_rating << endl;
            count++;
        }
    }
    cout << "共找到 " << count << " 家门店" << endl;
}

// 生成评论页面HTML
void generateCommentPageHTML() {
    ofstream file("comments.html");
    file << "<!DOCTYPE html>\n<html>\n<head>\n";
    file << "<meta charset=\"utf-8\">\n";
    file << "<title>成都美食 - 评论留言</title>\n";
    file << "<style>\n";
    file << "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
    file << "body { font-family: 'Microsoft YaHei', sans-serif; background: #f5f5f5; }\n";
    file << ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }\n";
    file << "h1 { text-align: center; margin: 30px 0; color: #333; }\n";
    file << ".comment-form { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); margin-bottom: 30px; }\n";
    file << ".form-group { margin-bottom: 15px; }\n";
    file << ".form-group label { display: block; margin-bottom: 5px; color: #666; }\n";
    file << ".form-group input, .form-group textarea, .form-group select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 6px; font-size: 14px; }\n";
    file << ".form-group textarea { height: 100px; resize: vertical; }\n";
    file << ".btn { background: #667eea; color: white; padding: 12px 30px; border: none; border-radius: 6px; cursor: pointer; font-size: 16px; }\n";
    file << ".btn:hover { background: #5568d3; }\n";
    file << ".comment-list { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n";
    file << ".comment-item { padding: 15px; border-bottom: 1px solid #f0f0f0; }\n";
    file << ".comment-header { display: flex; justify-content: space-between; margin-bottom: 10px; }\n";
    file << ".comment-user { font-weight: bold; color: #333; }\n";
    file << ".comment-time { color: #999; font-size: 12px; }\n";
    file << ".comment-rating { color: #ff6b6b; }\n";
    file << ".comment-content { color: #666; line-height: 1.6; }\n";
    file << ".comment-store { color: #667eea; font-size: 12px; margin-top: 5px; }\n";
    file << "</style>\n</head>\n<body>\n";

    file << "<div class=\"container\">\n";
    file << "<h1>成都美食 - 评论留言</h1>\n";

    // 评论表单
    file << "<div class=\"comment-form\">\n";
    file << "<h2>发表评论</h2>\n";
    file << "<form id=\"commentForm\">\n";
    file << "<div class=\"form-group\">\n";
    file << "<label>选择门店:</label>\n";
    file << "<select id=\"storeSelect\">\n";
    for (const auto& store : foodStoreList) {
        file << "<option value=\"" << store.id << "\">" << store.name << " (" << store.district << ")</option>\n";
    }
    file << "</select>\n</div>\n";

    file << "<div class=\"form-group\">\n";
    file << "<label>您的昵称:</label>\n";
    file << "<input type=\"text\" id=\"username\" placeholder=\"请输入昵称\">\n";
    file << "</div>\n";

    file << "<div class=\"form-group\">\n";
    file << "<label>评分:</label>\n";
    file << "<select id=\"rating\">\n";
    file << "<option value=\"5\">5分 - 非常满意</option>\n";
    file << "<option value=\"4\">4分 - 满意</option>\n";
    file << "<option value=\"3\">3分 - 一般</option>\n";
    file << "<option value=\"2\">2分 - 不满意</option>\n";
    file << "<option value=\"1\">1分 - 非常不满意</option>\n";
    file << "</select>\n</div>\n";

    file << "<div class=\"form-group\">\n";
    file << "<label>评论内容:</label>\n";
    file << "<textarea id=\"commentContent\" placeholder=\"请输入您的评论...\"></textarea>\n";
    file << "</div>\n";

    file << "<button type=\"submit\" class=\"btn\">提交评论</button>\n";
    file << "</form>\n</div>\n";

    // 评论列表
    file << "<div class=\"comment-list\">\n";
    file << "<h2>最新评论</h2>\n";
    file << "<div id=\"commentList\">\n";

    // 从JSON文件读取评论
    ifstream commentFile("comments.json");
    if (commentFile.is_open()) {
        string line;
        while (getline(commentFile, line)) {
            // 简单解析JSON评论
            if (line.find("\"username\"") != string::npos) {
                // 提取用户名
                size_t start = line.find("\"username\": \"") + 13;
                size_t end = line.find("\"", start);
                string username = line.substr(start, end - start);

                // 读取下一行获取内容
                getline(commentFile, line);
                start = line.find("\"content\": \"") + 12;
                end = line.find("\"", start);
                string content = line.substr(start, end - start);

                // 读取下一行获取评分
                getline(commentFile, line);
                start = line.find("\"rating\": ") + 10;
                end = line.find(",", start);
                int rating = stoi(line.substr(start, end - start));

                file << "<div class=\"comment-item\">\n";
                file << "<div class=\"comment-header\">\n";
                file << "<span class=\"comment-user\">" << username << "</span>\n";
                file << "<span class=\"comment-rating\">";
                for (int i = 0; i < rating; i++) file << "*";
                file << " " << rating << "分</span>\n";
                file << "</div>\n";
                file << "<div class=\"comment-content\">" << content << "</div>\n";
                file << "</div>\n";
            }
        }
        commentFile.close();
    }

    file << "</div>\n</div>\n</div>\n";

    // JavaScript
    file << "<script>\n";
    file << "document.getElementById('commentForm').addEventListener('submit', function(e) {\n";
    file << "  e.preventDefault();\n";
    file << "  var storeId = document.getElementById('storeSelect').value;\n";
    file << "  var username = document.getElementById('username').value;\n";
    file << "  var content = document.getElementById('commentContent').value;\n";
    file << "  var rating = document.getElementById('rating').value;\n";
    file << "  if (!username || !content) { alert('请填写完整信息'); return; }\n";
    file << "  alert('评论提交成功！');\n";
    file << "  location.reload();\n";
    file << "});\n";
    file << "</script>\n</body>\n</html>";

    file.close();
    cout << "评论页面已生成: comments.html" << endl;
}

int main() {
    cout << "=== 成都美食智能推荐系统 ===" << endl;
    cout << "数据结构: 图(邻接表) + 二叉搜索树 + 哈希表" << endl;
    cout << endl;

    // 初始化路线图
    roadGraph = new Graph(12);
    roadGraph->addVertex(1, "春熙路");
    roadGraph->addVertex(2, "太古里");
    roadGraph->addVertex(3, "宽窄巷子");
    roadGraph->addVertex(4, "锦里");
    roadGraph->addVertex(5, "武侯祠");
    roadGraph->addVertex(6, "杜甫草堂");
    roadGraph->addVertex(7, "人民公园");
    roadGraph->addVertex(8, "文殊院");
    roadGraph->addVertex(9, "建设路");
    roadGraph->addVertex(10, "东郊记忆");
    roadGraph->addVertex(11, "熊猫基地");
    roadGraph->addVertex(12, "环球中心");

    // 添加路线边(步行时间：分钟)
    roadGraph->addEdge(1, 2, 2);
    roadGraph->addEdge(1, 3, 37);
    roadGraph->addEdge(1, 4, 60);
    roadGraph->addEdge(1, 7, 40);
    roadGraph->addEdge(1, 8, 39);
    roadGraph->addEdge(1, 9, 55);
    roadGraph->addEdge(2, 3, 38);
    roadGraph->addEdge(2, 7, 41);
    roadGraph->addEdge(2, 8, 41);
    roadGraph->addEdge(3, 7, 15);
    roadGraph->addEdge(3, 8, 28);
    roadGraph->addEdge(4, 5, 18);
    roadGraph->addEdge(4, 6, 43);
    roadGraph->addEdge(4, 7, 34);
    roadGraph->addEdge(5, 6, 42);
    roadGraph->addEdge(7, 8, 35);
    roadGraph->addEdge(9, 10, 13);
    roadGraph->addEdge(11, 10, 133);

    // 模拟添加一些评论
    addComment(1, "美食家小王", "这家冷锅鱼非常地道，鱼肉鲜嫩，汤底浓郁！", 5);
    addComment(1, "吃货小李", "环境不错，服务态度也很好，推荐！", 4);
    addComment(5, "旅行者张三", "火锅很正宗，辣度适中，食材新鲜。", 5);
    addComment(10, "本地居民", "串串香味道不错，价格实惠，经常来吃。", 4);

    // 显示菜单
    int choice;
    do {
        cout << "\n=== 功能菜单 ===" << endl;
        cout << "1. 查看商圈统计" << endl;
        cout << "2. 查看评分TOP门店" << endl;
        cout << "3. 按价格筛选" << endl;
        cout << "4. 查看门店评论" << endl;
        cout << "5. 添加评论" << endl;
        cout << "6. 查找最短路径" << endl;
        cout << "7. 生成评论页面" << endl;
        cout << "0. 退出" << endl;
        cout << "请选择: ";
        cin >> choice;

        switch (choice) {
            case 1:
                statisticsByDistrict();
                break;
            case 2:
                showTopRatedStores(10);
                break;
            case 3: {
                int minP, maxP;
                cout << "最低价格: ";
                cin >> minP;
                cout << "最高价格: ";
                cin >> maxP;
                filterByPriceRange(minP, maxP);
                break;
            }
            case 4: {
                int storeId;
                cout << "请输入门店ID: ";
                cin >> storeId;
                showStoreComments(storeId);
                break;
            }
            case 5: {
                int storeId, rating;
                string username, content;
                cout << "门店ID: ";
                cin >> storeId;
                cout << "昵称: ";
                cin >> username;
                cout << "评分(1-5): ";
                cin >> rating;
                cin.ignore();
                cout << "评论内容: ";
                getline(cin, content);
                addComment(storeId, username, content, rating);
                break;
            }
            case 6: {
                int from, to;
                cout << "起点ID(1-12): ";
                cin >> from;
                cout << "终点ID(1-12): ";
                cin >> to;
                findShortestPath(from, to);
                break;
            }
            case 7:
                generateCommentPageHTML();
                break;
            case 0:
                cout << "感谢使用，再见！" << endl;
                break;
            default:
                cout << "无效选择" << endl;
        }
    } while (choice != 0);

    delete roadGraph;
    return 0;
}
