#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

#include "../model/Food.h"
#include "../model/Spot.h"
#include "../structure/SeqList.h"

/**
 * 文件管理工具类
 *
 * 职责：
 *   - 从 TXT 文件加载美食/景点数据
 *   - 将数据保存回文件
 *   - 字符串分割工具函数
 *
 * 文件格式（以 '|' 分隔）：
 *   food.txt: id|name|longitude|latitude|price|score|category|tags
 *   spot.txt: id|name|description|address|lng|lat|type|ticketInfo|openingTime|recommendDuration|bestSeason|score|tags
 */
class FileManager {
public:
    // ==================== 字符串工具 ====================

    // 按分隔符拆分字符串
    static SeqList<std::string> split(const std::string& str, char delimiter) {
        SeqList<std::string> result;
        std::istringstream iss(str);
        std::string token;
        while (std::getline(iss, token, delimiter)) {
            result.push_back(token);
        }
        return result;
    }

    // 去除首尾空白
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    // ==================== 美食数据加载 ====================

    /**
     * 从 food.txt 加载美食数据
     * 文件格式: id|name|longitude|latitude|price|score|category|tags
     * 示例: 1|单县羊肉汤|115.94|35.06|35|4.8|汤类|早餐,地方特色
     *
     * 时间复杂度: O(n)，n 为文件行数
     * 空间复杂度: O(n)
     */
    static SeqList<Food> loadFoodData(const std::string& filePath) {
        SeqList<Food> foods;
        std::ifstream file(filePath);

        if (!file.is_open()) {
            std::cerr << "[FileManager] 错误: 无法打开文件 " << filePath << std::endl;
            return foods;
        }

        std::string line;
        int lineNum = 0;
        int successCount = 0;

        while (std::getline(file, line)) {
            lineNum++;
            line = trim(line);

            // 跳过空行和注释行（以 # 开头）
            if (line.empty() || line[0] == '#') continue;

            SeqList<std::string> fields = split(line, '|');

            // 校验字段数
            if (fields.size() < 8) {
                std::cerr << "[FileManager] 警告: food.txt 第 " << lineNum
                          << " 行字段不足(需要8个，实际" << fields.size()
                          << ")，跳过: " << line << std::endl;
                continue;
            }

            try {
                Food food;
                food.id        = std::stoi(trim(fields[0]));
                food.name      = trim(fields[1]);
                food.longitude = std::stod(trim(fields[2]));
                food.latitude  = std::stod(trim(fields[3]));
                food.price     = std::stod(trim(fields[4]));
                food.score     = std::stod(trim(fields[5]));
                food.category  = trim(fields[6]);
                food.tags      = trim(fields[7]);

                foods.push_back(food);
                successCount++;
            } catch (const std::exception& e) {
                std::cerr << "[FileManager] 错误: food.txt 第 " << lineNum
                          << " 行解析失败: " << e.what() << std::endl;
            }
        }

        file.close();
        std::cout << "[FileManager] 加载美食数据完成: 成功 " << successCount
                  << " 条，跳过 " << (lineNum - successCount) << " 条" << std::endl;
        return foods;
    }

    // ==================== 景点数据加载 ====================

    /**
     * 从 spot.txt 加载景点数据
     * 文件格式: id|name|description|address|lng|lat|type|ticketInfo|openingTime|recommendDuration|bestSeason|score|tags
     */
    static SeqList<Spot> loadSpotData(const std::string& filePath) {
        SeqList<Spot> spots;
        std::ifstream file(filePath);

        if (!file.is_open()) {
            std::cerr << "[FileManager] 错误: 无法打开文件 " << filePath << std::endl;
            return spots;
        }

        std::string line;
        int lineNum = 0;
        int successCount = 0;

        while (std::getline(file, line)) {
            lineNum++;
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            SeqList<std::string> fields = split(line, '|');
            if (fields.size() < 13) {
                std::cerr << "[FileManager] 警告: spot.txt 第 " << lineNum
                          << " 行字段不足，跳过" << std::endl;
                continue;
            }

            try {
                Spot spot;
                spot.id                 = std::stoi(trim(fields[0]));
                spot.name               = trim(fields[1]);
                spot.description        = trim(fields[2]);
                spot.address            = trim(fields[3]);
                spot.longitude          = std::stod(trim(fields[4]));
                spot.latitude           = std::stod(trim(fields[5]));
                spot.type               = trim(fields[6]);
                spot.ticketInfo         = trim(fields[7]);
                spot.openingTime        = trim(fields[8]);
                spot.recommendDuration  = trim(fields[9]);
                spot.bestSeason         = trim(fields[10]);
                spot.score              = std::stod(trim(fields[11]));
                spot.tagsStr            = trim(fields[12]);
                spots.push_back(spot);
                successCount++;
            } catch (const std::exception& e) {
                std::cerr << "[FileManager] 错误: spot.txt 第 " << lineNum
                          << " 行解析失败: " << e.what() << std::endl;
            }
        }

        file.close();
        std::cout << "[FileManager] 加载景点数据完成: 成功 " << successCount << " 条" << std::endl;
        return spots;
    }

    // ==================== 数据保存 ====================

    /**
     * 保存美食数据到文件
     * 时间复杂度: O(n)
     */
    static bool saveFoodData(const std::string& filePath, const SeqList<Food>& foods) {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "[FileManager] 错误: 无法写入文件 " << filePath << std::endl;
            return false;
        }

        file << "# 菏泽美食数据" << std::endl;
        file << "# 格式: id|name|longitude|latitude|price|score|category|tags" << std::endl;

        for (int i = 0; i < foods.size(); i++) {
            const Food& f = foods[i];
            file << f.id << "|"
                 << f.name << "|"
                 << std::fixed << std::setprecision(4) << f.longitude << "|"
                 << std::fixed << std::setprecision(4) << f.latitude << "|"
                 << std::fixed << std::setprecision(0) << f.price << "|"
                 << std::fixed << std::setprecision(1) << f.score << "|"
                 << f.category << "|"
                 << f.tags << std::endl;
        }

        file.close();
        std::cout << "[FileManager] 保存美食数据完成: " << foods.size() << " 条" << std::endl;
        return true;
    }

    /**
     * 保存景点数据到文件
     */
    static bool saveSpotData(const std::string& filePath, const SeqList<Spot>& spots) {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "[FileManager] 错误: 无法写入文件 " << filePath << std::endl;
            return false;
        }

        file << "# 菏泽景点数据" << std::endl;
        file << "# 格式: id|name|description|address|lng|lat|type|ticketInfo|openingTime|recommendDuration|bestSeason|score|tags" << std::endl;

        for (int i = 0; i < spots.size(); i++) {
            const Spot& s = spots[i];
            file << s.id << "|"
                 << s.name << "|"
                 << s.description << "|"
                 << s.address << "|"
                 << std::fixed << std::setprecision(4) << s.longitude << "|"
                 << std::fixed << std::setprecision(4) << s.latitude << "|"
                 << s.type << "|"
                 << s.ticketInfo << "|"
                 << s.openingTime << "|"
                 << s.recommendDuration << "|"
                 << s.bestSeason << "|"
                 << std::fixed << std::setprecision(1) << s.score << "|"
                 << s.tagsStr << std::endl;
        }

        file.close();
        std::cout << "[FileManager] 保存景点数据完成: " << spots.size() << " 条" << std::endl;
        return true;
    }
};

#endif // FILEMANAGER_H
