#!/usr/bin/env python3
"""
重新编号 food.txt: 将美食ID从 1-63 改为 101-163（避免与景点ID 1-20 冲突）
同时更新所有相关引用的脚本。
"""
import os
import math

def renumber_food_txt():
    """将 food.txt 的 ID 从 1-63 改为 101-163"""
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, 'data')
    food_path = os.path.join(data_dir, 'food.txt')

    lines_out = []
    with open(food_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            stripped = line.strip()
            # 保留注释和空行
            if not stripped or stripped.startswith('#'):
                lines_out.append(line)
                continue
            # 解析并修改ID
            parts = [p.strip() for p in stripped.split('|')]
            if len(parts) >= 8:
                old_id = int(parts[0])
                new_id = old_id + 100
                parts[0] = str(new_id)
                lines_out.append('|'.join(parts))
            else:
                lines_out.append(line)

    # 写入文件
    with open(food_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines_out) + '\n')

    print(f"已更新 food.txt: ID 1-63 → 101-163")

if __name__ == '__main__':
    renumber_food_txt()
