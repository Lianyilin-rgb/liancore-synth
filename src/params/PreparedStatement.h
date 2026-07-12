// =============================================================================
// LianCore - PreparedStatement 参数化SQL查询工具
// 使用 raw SQLite3 API 实现真正的参数化查询，防止 SQL 注入
// 参考: OWASP SQL Injection Prevention Cheat Sheet
// =============================================================================
#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>
#include <functional>

// 前向声明 sqlite3 类型
struct sqlite3;
struct sqlite3_stmt;

namespace LianCore {

// =============================================================================
// 参数化查询包装器
// 使用 sqlite3_prepare_v2 + sqlite3_bind_* 实现安全参数绑定
// =============================================================================
class PreparedStatement {
public:
    PreparedStatement();
    ~PreparedStatement();

    // 使用 sqlite3* 句柄准备 SQL 语句
    bool prepare(sqlite3* db, const juce::String& sql);

    // 绑定参数 (索引从 1 开始, 符合 SQLite 规范)
    void bindInt(int index, int value);
    void bindInt64(int index, int64_t value);
    void bindFloat(int index, double value);
    void bindText(int index, const juce::String& value);
    void bindNull(int index);

    // 执行非查询语句 (INSERT/UPDATE/DELETE)
    bool execute();

    // 逐步获取结果行 (SELECT)
    bool step();

    // 获取列值
    int getColumnInt(int index) const;
    int64_t getColumnInt64(int index) const;
    double getColumnFloat(int index) const;
    juce::String getColumnText(int index) const;
    bool isColumnNull(int index) const;

    // 获取最后插入的 rowid
    int64_t getLastInsertRowId() const;

    // 重置语句以便重新绑定参数
    void reset();

    // 释放语句
    void finalize();

    // 是否已准备
    bool isPrepared() const { return stmt_ != nullptr; }

    // 获取内部 sqlite3_stmt* (用于高级操作)
    sqlite3_stmt* getHandle() const { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreparedStatement)
};

} // namespace LianCore