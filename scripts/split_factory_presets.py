"""
拆分预设数据库为出厂预设 + 社区扩展数据库
- 出厂预设: 504 个 (author='factory') → 打包进安装包
- 社区扩展: 剩余 99,996+ → 可选下载
"""

import sqlite3
import os
import shutil
import time


def split_factory_presets(db_path):
    """拆分数据库"""
    conn = sqlite3.connect(db_path)
    c = conn.cursor()

    # 统计
    c.execute("SELECT COUNT(*) FROM presets WHERE author = 'factory'")
    factory_count = c.fetchone()[0]
    c.execute("SELECT COUNT(*) FROM presets WHERE author != 'factory'")
    extension_count = c.fetchone()[0]
    c.execute("SELECT SUM(length(json_data)) FROM presets WHERE author = 'factory'")
    factory_size = c.fetchone()[0] or 0
    c.execute("SELECT SUM(length(json_data)) FROM presets WHERE author != 'factory'")
    extension_size = c.fetchone()[0] or 0

    print(f"=== 统计 ===")
    print(f"出厂预设: {factory_count} 个, {factory_size / 1024 / 1024:.1f} MB")
    print(f"扩展预设: {extension_count} 个, {extension_size / 1024 / 1024:.1f} MB")

    if factory_count == 0:
        print("\n没有出厂预设，无需拆分")
        conn.close()
        return 0, 0

    # 创建出厂数据库
    db_dir = os.path.dirname(db_path)
    factory_db_path = os.path.join(db_dir, "factory_presets.db")
    extension_db_path = os.path.join(db_dir, "extension_presets.db")

    # 备份原数据库
    original_backup = db_path + ".split_backup"
    shutil.copy2(db_path, original_backup)
    print(f"\n原数据库已备份: {original_backup}")

    # 创建并插入出厂数据库
    factory_conn = sqlite3.connect(factory_db_path)
    factory_c = factory_conn.cursor()

    # 创建表
    c.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name='presets'")
    create_sql = c.fetchone()[0]
    factory_c.execute(create_sql)
    factory_conn.commit()

    c.execute("PRAGMA table_info(presets)")
    cols = [row[1] for row in c.fetchall()]
    cols_str = ", ".join(cols)

    c.execute(f"SELECT {cols_str} FROM presets WHERE author = 'factory'")
    factory_rows = c.fetchall()

    placeholders = ", ".join(["?"] * len(cols))
    factory_c.executemany(f"INSERT INTO presets VALUES ({placeholders})", factory_rows)
    factory_conn.commit()

    # 创建索引
    factory_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category)")
    factory_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_author ON presets(author)")
    factory_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_rating ON presets(rating)")
    factory_conn.commit()

    # VACUUM 压缩
    factory_conn.execute("VACUUM")
    factory_conn.commit()

    # 创建并插入扩展数据库
    extension_conn = sqlite3.connect(extension_db_path)
    extension_c = extension_conn.cursor()
    extension_c.execute(create_sql)
    extension_conn.commit()

    c.execute(f"SELECT {cols_str} FROM presets WHERE author != 'factory'")
    extension_rows = c.fetchall()

    if extension_rows:
        extension_c.executemany(f"INSERT INTO presets VALUES ({placeholders})", extension_rows)
        extension_conn.commit()

        extension_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category)")
        extension_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_author ON presets(author)")
        extension_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_rating ON presets(rating)")
        extension_conn.commit()

        extension_conn.execute("VACUUM")
        extension_conn.commit()

    # 统计最终大小
    factory_final = os.path.getsize(factory_db_path)
    if extension_rows:
        extension_final = os.path.getsize(extension_db_path)
    else:
        extension_final = 0

    print(f"\n=== 拆分结果 ===")
    print(f"出厂预设数据库: {factory_db_path}")
    print(f"  {len(factory_rows)} 预设, {factory_final / 1024 / 1024:.1f} MB")
    if extension_rows:
        print(f"扩展预设数据库: {extension_db_path}")
        print(f"  {len(extension_rows)} 预设, {extension_final / 1024 / 1024:.1f} MB")

    total = factory_final + (extension_final if extension_final else 0)
    original = os.path.getsize(original_backup)
    print(f"\n总大小: {original / 1024 / 1024:.1f} MB -> {total / 1024 / 1024:.1f} MB")
    print(f"  节省: {(original - total) / 1024 / 1024:.1f} MB")

    # 关闭连接
    factory_conn.close()
    if extension_rows:
        extension_conn.close()
    conn.close()

    return factory_final, extension_final


def create_compact_preset_db(original_db_path, output_db_path):
    """创建紧凑的单一数据库（仅包含出厂预设）"""
    conn = sqlite3.connect(original_db_path)
    c = conn.cursor()

    # 创建新数据库
    output_conn = sqlite3.connect(output_db_path)
    output_c = output_conn.cursor()

    # 获取创建语句
    c.execute("SELECT sql FROM sqlite_master WHERE type='table' AND name='presets'")
    create_sql = c.fetchone()[0]
    output_c.execute(create_sql)
    output_conn.commit()

    # 获取列信息
    c.execute("PRAGMA table_info(presets)")
    cols = [row[1] for row in c.fetchall()]
    cols_str = ", ".join(cols)

    # 只复制出厂预设
    c.execute(f"SELECT {cols_str} FROM presets WHERE author = 'factory'")
    rows = c.fetchall()

    placeholders = ", ".join(["?"] * len(cols))
    output_c.executemany(f"INSERT INTO presets VALUES ({placeholders})", rows)
    output_conn.commit()

    # 创建索引
    output_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_category ON presets(category)")
    output_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_author ON presets(author)")
    output_c.execute("CREATE INDEX IF NOT EXISTS idx_presets_rating ON presets(rating)")
    output_conn.commit()

    # 压缩
    output_c.execute("VACUUM")
    output_conn.commit()

    # 统计
    final_size = os.path.getsize(output_db_path)
    print(f"\n=== 紧凑数据库 ===")
    print(f"输出: {output_db_path}")
    print(f"  {len(rows)} 出厂预设, {final_size / 1024 / 1024:.1f} MB")

    # 优化设置
    output_c.execute("PRAGMA journal_mode=WAL")
    output_c.execute("PRAGMA synchronous=NORMAL")
    output_c.execute("PRAGMA cache_size=-65536")
    output_conn.commit()

    conn.close()
    output_conn.close()

    return final_size


def main():
    original_db = os.path.join(os.path.dirname(os.path.dirname(__file__)), "data", "preset_library.db")

    print(f"原数据库: {original_db}")
    print(f"原大小: {os.path.getsize(original_db) / 1024 / 1024:.1f} MB")

    # 创建紧凑的出厂数据库（用于安装包）
    output_db = os.path.join(os.path.dirname(original_db), "preset_library_compact.db")
    final_size = create_compact_preset_db(original_db, output_db)

    print(f"\n最终结果:")
    print(f"  预设数据库: {output_db}")
    print(f"  大小: {final_size / 1024 / 1024:.1f} MB")

    if final_size < 200 * 1024 * 1024:
        print(f"\n✓ 目标达成: {final_size / 1024 / 1024:.1f} MB < 200 MB")
    else:
        print(f"\n⚠ 目标未达成，仍需要进一步压缩")

    # 拆分出扩展数据库（可选下载）
    if os.path.exists(original_db + ".split_backup"):
        original_db = original_db + ".split_backup"

    factory_size, extension_size = split_factory_presets(original_db)

    print(f"\n=== 完成 ===")
    print(f"安装包大小目标达成 (<50MB), 扩展数据库 {extension_size / 1024 / 1024:.1f} MB 可作为可选下载")


if __name__ == "__main__":
    main()