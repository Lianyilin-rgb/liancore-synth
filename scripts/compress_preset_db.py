"""
LianCore V3 - 预设数据库压缩脚本
目标: 将 376MB 压缩到 200MB 以下

策略:
1. VACUUM + REINDEX (回收碎片空间)
2. 压缩 JSON合成图数据 (移除默认值、缩进)
3. 分析并拆分大表
"""

import sqlite3
import json
import os
import shutil
import time


def analyze_db(db_path):
    """分析数据库状态"""
    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    # 基本信息
    sz = os.path.getsize(db_path)
    c.execute("SELECT COUNT(*) FROM presets")
    count = c.fetchone()[0]
    c.execute("PRAGMA page_count")
    pages = c.fetchone()[0]
    c.execute("PRAGMA page_size")
    page_size = c.fetchone()[0]
    c.execute("PRAGMA freelist_count")
    free_pages = c.fetchone()[0]

    print(f"=== 数据库分析 ===")
    print(f"文件大小: {sz / 1024 / 1024:.1f} MB")
    print(f"预设数量: {count}")
    print(f"页面数: {pages} (页大小: {page_size} bytes)")
    print(f"理论大小: {pages * page_size / 1024 / 1024:.1f} MB")
    print(f"空闲页面: {free_pages} ({free_pages * page_size / 1024:.1f} KB)")

    # 分析 JSON 数据大小
    c.execute("SELECT SUM(length(json_data)) FROM presets")
    total_json = c.fetchone()[0] or 0
    print(f"JSON数据总量: {total_json / 1024 / 1024:.1f} MB")

    # 平均每条预设大小
    c.execute("SELECT AVG(length(json_data)) FROM presets")
    avg_json = c.fetchone()[0] or 0
    print(f"平均每条JSON: {avg_json / 1024:.1f} KB")

    # 按类别统计
    c.execute("SELECT category, COUNT(*), SUM(length(json_data)) FROM presets GROUP BY category ORDER BY COUNT(*) DESC")
    print(f"\n=== 类别分布 ===")
    for row in c.fetchall():
        print(f"  {row[0]:<15} {row[1]:>5} 预设  {row[2] / 1024 / 1024:.1f} MB")

    conn.close()
    return sz, count


def compress_json_data(json_str):
    """压缩 JSON 数据: 移除缩进和默认值字段"""
    try:
        data = json.loads(json_str) if isinstance(json_str, str) else json_str
    except (json.JSONDecodeError, TypeError):
        return json_str

    # 移除默认值字段 (减少冗余)
    if isinstance(data, dict):
        # 移除 author 字段如果为默认值
        if data.get("author") == "unknown":
            data.pop("author", None)
        # 移除空 tags
        if data.get("tags") == [] or data.get("tags") == "":
            data.pop("tags", None)
        # 移除空 description
        if data.get("description") == "":
            data.pop("description", None)

    # 紧凑 JSON (无缩进, 无多余空格)
    return json.dumps(data, separators=(",", ":"), ensure_ascii=False)


def compress_db(db_path, backup=True):
    """压缩数据库"""
    if backup:
        backup_path = db_path + ".backup"
        shutil.copy2(db_path, backup_path)
        print(f"\n备份已创建: {backup_path}")

    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    # Step 1: 压缩 JSON 数据
    print(f"\n=== 压缩 JSON 数据 ===")
    c.execute("SELECT id, json_data FROM presets")
    rows = c.fetchall()
    total_before = 0
    total_after = 0
    updates = []

    for row_id, json_str in rows:
        if json_str is None:
            continue
        total_before += len(json_str)
        compressed = compress_json_data(json_str)
        total_after += len(compressed)
        if compressed != json_str:
            updates.append((compressed, row_id))

    print(f"  JSON总量: {total_before / 1024 / 1024:.1f} MB -> {total_after / 1024 / 1024:.1f} MB")
    print(f"  压缩率: {(1 - total_after / total_before) * 100:.1f}%")
    print(f"  更新 {len(updates)}/{len(rows)} 条记录")

    if updates:
        c.executemany("UPDATE presets SET json_data = ? WHERE id = ?", updates)
        conn.commit()
        print(f"  已更新 {len(updates)} 条记录")

    # Step 2: VACUUM (回收空间)
    print(f"\n=== VACUUM ===")
    before_vacuum = os.path.getsize(db_path)
    conn.execute("VACUUM")
    after_vacuum = os.path.getsize(db_path)
    print(f"  大小: {before_vacuum / 1024 / 1024:.1f} MB -> {after_vacuum / 1024 / 1024:.1f} MB")
    print(f"  回收: {(before_vacuum - after_vacuum) / 1024 / 1024:.1f} MB")

    # Step 3: REINDEX
    print(f"\n=== REINDEX ===")
    conn.execute("REINDEX")
    conn.commit()

    # Step 4: 优化
    print(f"\n=== 优化设置 ===")
    c.execute("PRAGMA journal_mode=WAL")
    c.execute("PRAGMA synchronous=NORMAL")
    c.execute("PRAGMA cache_size=-65536")  # 64MB cache
    conn.commit()

    conn.close()

    final_size = os.path.getsize(db_path)
    print(f"\n=== 最终结果 ===")
    print(f"  最终大小: {final_size / 1024 / 1024:.1f} MB")
    return final_size


def main():
    db_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), "data", "preset_library.db")

    if not os.path.exists(db_path):
        # 尝试备用路径
        alt_paths = [
            os.path.join(os.getcwd(), "data", "preset_library.db"),
            "data/preset_library.db",
        ]
        for p in alt_paths:
            if os.path.exists(p):
                db_path = p
                break
        else:
            print(f"错误: 找不到预设数据库文件")
            print(f"尝试路径: {db_path}, {alt_paths}")
            return

    print(f"数据库路径: {db_path}")
    start_time = time.time()

    # 分析
    original_size, count = analyze_db(db_path)

    if original_size < 200 * 1024 * 1024:
        print(f"\n数据库已小于 200MB ({original_size / 1024 / 1024:.1f} MB)，无需压缩")
        return

    # 压缩
    final_size = compress_db(db_path)

    # 验证
    elapsed = time.time() - start_time
    print(f"\n操作耗时: {elapsed:.1f} 秒")
    print(f"压缩率: {(1 - final_size / original_size) * 100:.1f}%")

    if final_size < 200 * 1024 * 1024:
        print(f"目标达成: {final_size / 1024 / 1024:.1f} MB < 200 MB")
    else:
        print(f"目标未达成: {final_size / 1024 / 1024:.1f} MB")
        print(f"建议: 考虑拆分数据库为出厂预设 + 扩展预设")


if __name__ == "__main__":
    main()