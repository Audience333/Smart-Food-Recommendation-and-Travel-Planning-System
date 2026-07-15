#!/usr/bin/env python3
"""
==========================================================================
美食数据重新编号脚本 (renumber_foods.py)
==========================================================================
【脚本用途】
  将 data/food.txt 中所有美食POI的ID统一偏移+100，使美食ID范围从
  1-63 变为 101-163，避免与景点ID范围（1-20）冲突。

【背景说明】
  在系统设计中，景点使用较小的ID（1-100范围），美食使用较大的ID
  （101+范围）。如果使用同一个数据源生成两个文件，可能会出现ID重叠。
  本脚本解决早期数据中美食和景点ID共用相同区间导致冲突的问题。

【在数据流水线中的位置】
  本脚本是一次性修复脚本，应在所有数据采集之前运行一次：
  本脚本 -> expand_data.py -> fill_addresses.py -> fetch_photos.py ->
  recalc_roads.py -> gen_web_json.py

  注意：本脚本只修改 food.txt 中的ID字段，不处理其他文件中的引用。

【何时运行】
  - 仅当 food.txt 中的ID范围为原始小数字（如1-63）且需要改为101+时运行
  - 通常只需要运行一次
  - 如果 food.txt 中的ID已经在101以上，则不需要运行

【运行方式】
  cd HezeFoodSystem
  python tools/renumber_foods.py

【前置条件】
  - 已存在 data/food.txt（ID范围为1-63）
  - data/spot.txt 使用不同的ID范围（如1-20）

【输出】
  - 原地更新 data/food.txt，将ID字段统一+100
  - 其他字段（名称、坐标、价格、评分等）保持不变

【影响范围】
  - 仅修改 data/food.txt 中的ID字段
  - 需要重新运行 recalc_roads.py（道路连接依赖ID）
  - 需要重新运行 gen_web_json.py（JSON输出依赖ID）
==========================================================================
"""
import os
import math

def renumber_food_txt():
    """
    核心逻辑：读取 food.txt，将所有有效数据行的ID+100，保持其他字段不变

    处理细节：
    - 保留注释行（#开头）和空行不变
    - 有效数据行至少需要8个字段（ID|名称|经度|纬度|价格|评分|分类|地址|...）
    - 只修改第一个字段（ID），其余字段原样保留
    - 使用 | 作为字段分隔符
    """
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data_dir = os.path.join(base_dir, 'data')
    food_path = os.path.join(data_dir, 'food.txt')

    lines_out = []
    with open(food_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')            # 保留行尾格式
            stripped = line.strip()
            # 保留注释行和空行：原样输出不做任何修改
            if not stripped or stripped.startswith('#'):
                lines_out.append(line)
                continue
            # 解析数据行并修改ID
            parts = [p.strip() for p in stripped.split('|')]
            if len(parts) >= 8:                 # 有效数据行至少8个字段
                old_id = int(parts[0])
                new_id = old_id + 100           # 将所有ID偏移+100
                parts[0] = str(new_id)
                lines_out.append('|'.join(parts))
            else:
                # 字段不足的行原样保留（可能为不完整数据）
                lines_out.append(line)

    # 写回文件
    with open(food_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines_out) + '\n')

    print(f"已更新 food.txt: ID 1-63 → 101-163")


if __name__ == '__main__':
    renumber_food_txt()
