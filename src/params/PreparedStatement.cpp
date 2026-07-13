// =============================================================================
// LianCore - PreparedStatement 实现
// 基于 sqlite3_prepare_v2 / sqlite3_bind_* / sqlite3_step
// =============================================================================
#include "PreparedStatement.h"

// 使用原生 SQLite3 API (本地 amalgamation)
#include "sqlite3.h"

namespace LianCore {

PreparedStatement::PreparedStatement() = default;

PreparedStatement::~PreparedStatement() {
    finalize();
}

bool PreparedStatement::prepare(sqlite3* db, const juce::String& sql) {
    if (!db) return false;

    // 先释放旧语句
    finalize();

    db_ = db;
    const char* tail = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.toRawUTF8(), -1, &stmt_, &tail);
    return rc == SQLITE_OK && stmt_ != nullptr;
}

void PreparedStatement::bindInt(int index, int value) {
    if (stmt_) sqlite3_bind_int(stmt_, index, value);
}

void PreparedStatement::bindInt64(int index, int64_t value) {
    if (stmt_) sqlite3_bind_int64(stmt_, index, value);
}

void PreparedStatement::bindFloat(int index, double value) {
    if (stmt_) sqlite3_bind_double(stmt_, index, value);
}

void PreparedStatement::bindText(int index, const juce::String& value) {
    if (!stmt_) return;
    // SQLITE_TRANSIENT: SQLite 会复制字符串，调用者不需要保持其生命周期
    std::string utf8 = value.toStdString();
    sqlite3_bind_text(stmt_, index, utf8.c_str(), (int)utf8.size(), SQLITE_TRANSIENT);
}

void PreparedStatement::bindNull(int index) {
    if (stmt_) sqlite3_bind_null(stmt_, index);
}

bool PreparedStatement::execute() {
    if (!stmt_) return false;
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_DONE || rc == SQLITE_ROW;
}

bool PreparedStatement::step() {
    if (!stmt_) return false;
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_ROW;
}

int PreparedStatement::getColumnInt(int index) const {
    if (!stmt_) return 0;
    return sqlite3_column_int(stmt_, index);
}

int64_t PreparedStatement::getColumnInt64(int index) const {
    if (!stmt_) return 0;
    return sqlite3_column_int64(stmt_, index);
}

double PreparedStatement::getColumnFloat(int index) const {
    if (!stmt_) return 0.0;
    return sqlite3_column_double(stmt_, index);
}

juce::String PreparedStatement::getColumnText(int index) const {
    if (!stmt_) return {};
    const char* text = (const char*)sqlite3_column_text(stmt_, index);
    return text ? juce::String::fromUTF8(text) : juce::String();
}

bool PreparedStatement::isColumnNull(int index) const {
    if (!stmt_) return true;
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

int64_t PreparedStatement::getLastInsertRowId() const {
    if (!db_) return -1;
    return sqlite3_last_insert_rowid(db_);
}

void PreparedStatement::reset() {
    if (stmt_) sqlite3_reset(stmt_);
}

void PreparedStatement::finalize() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
        db_ = nullptr;
    }
}

} // namespace LianCore