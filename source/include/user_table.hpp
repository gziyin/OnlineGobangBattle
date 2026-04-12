#pragma once
#include <string>
#include <cstdint>
#include <json/json.h>

#include "db.hpp"
#include "logger.hpp"
#include "util.hpp"

namespace gobang {

/**
 * @brief 用户表数据访问层（DAL）
 *
 * 负责用户数据的 CRUD 操作，不做业务逻辑验证
 * 密码验证由调用方使用 security 模块处理
 *
 * 注意：
 * - 所有接口仅供内部模块调用，不直接暴露给外部
 * - 涉及多表更新的操作用于事务保证原子性
 */
class UserTable {
public:
    /**
     * @brief 初始化，注入 DBPool
     */
    void init(DBPool* pool);

    // ==================== 写操作 ====================

    /**
     * @brief 用户注册：插入新用户
     * @param username 用户名（3-32 位字母数字）
     * @param password_hash PBKDF2 加密后的密码哈希
     * @return 成功返回 user_id (>0)，失败返回 0
     *
     * 失败原因：用户名已存在 (ER_DUP_ENTRY)、数据库错误
     */
    int64_t insert(const std::string& username, const std::string& password_hash);

    /**
     * @brief 更新用户状态
     * @param user_id 用户 ID
     * @param status 状态码（0-离线 1-大厅 2-房间 3-游戏中）
     * @return 成功返回 true，失败返回 false
     */
    bool update_status(int64_t user_id, int status);

    /**
     * @brief 比赛结束后更新双方分数（事务操作，保证原子性）
     * @param winner_id 获胜者 ID
     * @param loser_id 失败者 ID
     * @param winner_delta 获胜者分数变化，**传正数**（通常 +25）
     * @param loser_penalty 失败者扣分，**传正数，内部自动取减**（通常 -15，最低不低于 0）
     * @return 成功返回 true，失败返回 false（回滚）
     *
     * 注意：此接口使用事务，要么双方都更新成功，要么都失败
     *
     * 示例：update_score_match(1001, 1002, 25, 15);
     *   → winner 分数 +25，loser 分数 -15（不会变成负数）
     */
    bool update_score_match(int64_t winner_id, int64_t loser_id,
                            int winner_delta = 25, int loser_penalty = 15);

    // ==================== 读操作 ====================

    /**
     * @brief 根据用户名查询用户（仅供认证模块使用，包含 password_hash）
     * @param username 用户名
     * @param out 输出参数，包含完整用户记录
     * @return 成功返回 true，用户不存在返回 false
     *
     * out 包含字段：id, username, password_hash, score, total_count, win_count, status
     *
     * ⚠️ 注意：此接口返回 password_hash，仅供 security 模块验证密码时使用
     */
    bool select_for_auth(const std::string& username, Json::Value& out);

    /**
     * @brief 根据用户名查询用户（公开版本，不包含 password_hash）
     * @param username 用户名
     * @param out 输出参数，包含公开用户信息
     * @return 成功返回 true，用户不存在返回 false
     *
     * out 包含字段：id, username, score, total_count, win_count, status
     *
     * ✅ 安全：此接口不返回 password_hash，可安全用于用户资料展示
     */
    bool select_by_username(const std::string& username, Json::Value& out);

    /**
     * @brief 根据 ID 查询用户（公开版本，不包含 password_hash）
     * @param user_id 用户 ID
     * @param out 输出参数，包含公开用户信息
     * @return 成功返回 true，用户不存在返回 false
     */
    bool select_by_id(int64_t user_id, Json::Value& out);

private:
    DBPool* _pool = nullptr;
};

// ==================== 实现 ====================

inline void UserTable::init(DBPool* pool) {
    _pool = pool;
    LOG_INFO("UserTable: initialized");
}

inline int64_t UserTable::insert(const std::string& username, const std::string& password_hash) {
    if (!_pool) {
        LOG_ERROR("UserTable: insert called before init");
        return 0;
    }

    auto conn = _pool->get_connection();
    if (!conn) {
        LOG_ERROR("UserTable: failed to get connection for insert");
        return 0;
    }

    const std::string sql =
        "INSERT INTO user (username, password_hash) VALUES (?, ?)";

    auto stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        LOG_ERROR("UserTable: mysql_stmt_init failed: " << mysql_error(conn.get()));
        return 0;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR("UserTable: mysql_stmt_prepare failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    // 绑定参数
    MYSQL_BIND params[2];
    memset(params, 0, sizeof(params));

    // username
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = (char*)username.c_str();
    params[0].buffer_length = username.size();
    params[0].is_null = false;
    params[0].length = nullptr;

    // password_hash
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = (char*)password_hash.c_str();
    params[1].buffer_length = password_hash.size();
    params[1].is_null = false;
    params[1].length = nullptr;

    if (mysql_stmt_bind_param(stmt, params) != 0) {
        LOG_ERROR("UserTable: mysql_stmt_bind_param failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return 0;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        int err = mysql_stmt_errno(stmt);
        if (err == ER_DUP_ENTRY) {
            LOG_WARN("UserTable: insert failed - username already exists: " << username);
        } else {
            LOG_ERROR("UserTable: insert failed: " << mysql_stmt_error(stmt));
        }
        mysql_stmt_close(stmt);
        return 0;
    }

    // 获取新生成的 user_id
    int64_t user_id = mysql_insert_id(conn.get());
    mysql_stmt_close(stmt);

    LOG_INFO("UserTable: user inserted - username=" << username << ", id=" << user_id);
    return user_id;
}

inline bool UserTable::update_status(int64_t user_id, int status) {
    if (!_pool) {
        LOG_ERROR("UserTable: update_status called before init");
        return false;
    }

    auto conn = _pool->get_connection();
    if (!conn) {
        LOG_ERROR("UserTable: failed to get connection for update_status");
        return false;
    }

    const std::string sql =
        "UPDATE user SET status = ? WHERE id = ?";

    auto stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        LOG_ERROR("UserTable: mysql_stmt_init failed");
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR("UserTable: mysql_stmt_prepare failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定参数
    MYSQL_BIND params[2];
    memset(params, 0, sizeof(params));

    // status
    params[0].buffer_type = MYSQL_TYPE_LONG;
    params[0].buffer = &status;
    params[0].is_null = false;

    // user_id
    params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    params[1].buffer = &user_id;
    params[1].is_null = false;

    if (mysql_stmt_bind_param(stmt, params) != 0) {
        LOG_ERROR("UserTable: mysql_stmt_bind_param failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR("UserTable: update_status failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    int64_t affected = mysql_stmt_affected_rows(stmt);
    mysql_stmt_close(stmt);

    if (affected != 1) {
        LOG_WARN("UserTable: update_status - no row updated (user_id=" << user_id << ")");
        return false;
    }

    LOG_DEBUG("UserTable: status updated - user_id=" << user_id << ", status=" << status);
    return true;
}

inline bool UserTable::update_score_match(int64_t winner_id, int64_t loser_id,
                                          int winner_delta, int loser_penalty) {
    if (!_pool) {
        LOG_ERROR("UserTable: update_score_match called before init");
        return false;
    }

    auto conn = _pool->get_connection();
    if (!conn) {
        LOG_ERROR("UserTable: failed to get connection for update_score_match");
        return false;
    }

    // 开启事务
    if (mysql_query(conn.get(), "START TRANSACTION") != 0) {
        LOG_ERROR("UserTable: START TRANSACTION failed: " << mysql_error(conn.get()));
        return false;
    }

    // 更新获胜者分数：score + winner_delta，使用 GREATEST 防止负数
    const std::string winner_sql =
        "UPDATE user SET score = GREATEST(0, score + ?), "
        "win_count = win_count + 1, total_count = total_count + 1 "
        "WHERE id = ?";

    auto stmt_winner = mysql_stmt_init(conn.get());
    if (mysql_stmt_prepare(stmt_winner, winner_sql.c_str(), winner_sql.size()) != 0) {
        LOG_ERROR("UserTable: prepare winner SQL failed: " << mysql_stmt_error(stmt_winner));
        mysql_query(conn.get(), "ROLLBACK");
        mysql_stmt_close(stmt_winner);
        return false;
    }

    // 绑定获胜者参数
    MYSQL_BIND winner_params[2];
    memset(winner_params, 0, sizeof(winner_params));

    winner_params[0].buffer_type = MYSQL_TYPE_LONG;
    winner_params[0].buffer = &winner_delta;
    winner_params[0].is_null = false;

    winner_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    winner_params[1].buffer = &winner_id;
    winner_params[1].is_null = false;

    if (mysql_stmt_bind_param(stmt_winner, winner_params) != 0) {
        LOG_ERROR("UserTable: bind winner params failed: " << mysql_stmt_error(stmt_winner));
        mysql_query(conn.get(), "ROLLBACK");
        mysql_stmt_close(stmt_winner);
        return false;
    }

    if (mysql_stmt_execute(stmt_winner) != 0) {
        LOG_ERROR("UserTable: update winner failed: " << mysql_stmt_error(stmt_winner));
        mysql_query(conn.get(), "ROLLBACK");
        mysql_stmt_close(stmt_winner);
        return false;
    }

    int64_t winner_affected = mysql_stmt_affected_rows(stmt_winner);
    mysql_stmt_close(stmt_winner);

    // 更新失败者分数：score - loser_penalty，使用 GREATEST 防止负数
    const std::string loser_sql =
        "UPDATE user SET score = GREATEST(0, score - ?), "
        "total_count = total_count + 1 "
        "WHERE id = ?";

    auto stmt_loser = mysql_stmt_init(conn.get());
    if (mysql_stmt_prepare(stmt_loser, loser_sql.c_str(), loser_sql.size()) != 0) {
        LOG_ERROR("UserTable: prepare loser SQL failed: " << mysql_stmt_error(stmt_loser));
        mysql_query(conn.get(), "ROLLBACK");
        mysql_stmt_close(stmt_loser);
        return false;
    }

    // 绑定失败者参数
    MYSQL_BIND loser_params[2];
    memset(loser_params, 0, sizeof(loser_params));

    loser_params[0].buffer_type = MYSQL_TYPE_LONG;
    loser_params[0].buffer = &loser_penalty;
    loser_params[0].is_null = false;

    loser_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    loser_params[1].buffer = &loser_id;
    loser_params[1].is_null = false;

    if (mysql_stmt_bind_param(stmt_loser, loser_params) != 0) {
        LOG_ERROR("UserTable: bind loser params failed: " << mysql_stmt_error(stmt_loser));
        mysql_query(conn.get(), "ROLLBACK");
        mysql_stmt_close(stmt_loser);
        return false;
    }

    if (mysql_stmt_execute(stmt_loser) != 0) {
        LOG_ERROR("UserTable: update loser failed: " << mysql_stmt_error(stmt_loser));
        mysql_query(conn.get(), "ROLLBACK");
        mysql_stmt_close(stmt_loser);
        return false;
    }

    int64_t loser_affected = mysql_stmt_affected_rows(stmt_loser);
    mysql_stmt_close(stmt_loser);

    // 检查受影响行数
    if (winner_affected != 1 || loser_affected != 1) {
        LOG_ERROR("UserTable: update_score_match - affected rows mismatch: "
                  << "winner=" << winner_affected << ", loser=" << loser_affected);
        mysql_query(conn.get(), "ROLLBACK");
        return false;
    }

    // 提交事务
    if (mysql_query(conn.get(), "COMMIT") != 0) {
        LOG_ERROR("UserTable: COMMIT failed: " << mysql_error(conn.get()));
        return false;
    }

    LOG_INFO("UserTable: score updated - winner_id=" << winner_id << " (+" << winner_delta
             << "), loser_id=" << loser_id << " (-" << loser_penalty << ")");
    return true;
}

inline bool UserTable::select_for_auth(const std::string& username, Json::Value& out) {
    if (!_pool) {
        LOG_ERROR("UserTable: select_for_auth called before init");
        return false;
    }

    auto conn = _pool->get_connection();
    if (!conn) {
        LOG_ERROR("UserTable: failed to get connection for select_for_auth");
        return false;
    }

    // 查询包含 password_hash 的完整记录
    const std::string sql =
        "SELECT id, username, password_hash, score, total_count, win_count, status "
        "FROM user WHERE username = ?";

    auto stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        LOG_ERROR("UserTable: mysql_stmt_init failed");
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR("UserTable: mysql_stmt_prepare failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定输入参数
    MYSQL_BIND param;
    memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = (char*)username.c_str();
    param.buffer_length = username.size();
    param.is_null = false;

    if (mysql_stmt_bind_param(stmt, &param) != 0) {
        LOG_ERROR("UserTable: bind param failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR("UserTable: execute failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定结果
    MYSQL_BIND result[7];
    memset(result, 0, sizeof(result));

    unsigned long id;
    char username_buf[33];
    unsigned long username_len = 0;
    char password_hash_buf[513];
    unsigned long password_hash_len = 0;
    unsigned long score;
    unsigned long total_count;
    unsigned long win_count;
    int status;

    // id
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &id;
    result[0].is_null = false;

    // username
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = username_buf;
    result[1].buffer_length = sizeof(username_buf) - 1;
    result[1].length = &username_len;
    result[1].is_null = false;

    // password_hash
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = password_hash_buf;
    result[2].buffer_length = sizeof(password_hash_buf) - 1;
    result[2].length = &password_hash_len;
    result[2].is_null = false;

    // score
    result[3].buffer_type = MYSQL_TYPE_LONG;
    result[3].buffer = &score;
    result[3].is_null = false;

    // total_count
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &total_count;
    result[4].is_null = false;

    // win_count
    result[5].buffer_type = MYSQL_TYPE_LONG;
    result[5].buffer = &win_count;
    result[5].is_null = false;

    // status
    result[6].buffer_type = MYSQL_TYPE_TINY;
    result[6].buffer = &status;
    result[6].is_null = false;

    if (mysql_stmt_bind_result(stmt, result) != 0) {
        LOG_ERROR("UserTable: bind result failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false; // 用户不存在
    }

    mysql_stmt_close(stmt);

    // 填充 JSON 输出
    username_buf[username_len] = '\0';
    password_hash_buf[password_hash_len] = '\0';

    out["id"] = (int64_t)id;
    out["username"] = username_buf;
    out["password_hash"] = password_hash_buf;
    out["score"] = (int64_t)score;
    out["total_count"] = (int64_t)total_count;
    out["win_count"] = (int64_t)win_count;
    out["status"] = status;

    LOG_DEBUG("UserTable: select_for_auth - username=" << username);
    return true;
}

inline bool UserTable::select_by_username(const std::string& username, Json::Value& out) {
    if (!_pool) {
        LOG_ERROR("UserTable: select_by_username called before init");
        return false;
    }

    auto conn = _pool->get_connection();
    if (!conn) {
        LOG_ERROR("UserTable: failed to get connection for select_by_username");
        return false;
    }

    // 查询不包含 password_hash 的公开记录
    const std::string sql =
        "SELECT id, username, score, total_count, win_count, status "
        "FROM user WHERE username = ?";

    auto stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        LOG_ERROR("UserTable: mysql_stmt_init failed");
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR("UserTable: mysql_stmt_prepare failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定输入参数
    MYSQL_BIND param;
    memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = (char*)username.c_str();
    param.buffer_length = username.size();
    param.is_null = false;

    if (mysql_stmt_bind_param(stmt, &param) != 0) {
        LOG_ERROR("UserTable: bind param failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR("UserTable: execute failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定结果
    MYSQL_BIND result[6];
    memset(result, 0, sizeof(result));

    unsigned long id;
    char username_buf[33];
    unsigned long username_len = 0;
    unsigned long score;
    unsigned long total_count;
    unsigned long win_count;
    int status;

    // id
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &id;
    result[0].is_null = false;

    // username
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = username_buf;
    result[1].buffer_length = sizeof(username_buf) - 1;
    result[1].length = &username_len;
    result[1].is_null = false;

    // score
    result[2].buffer_type = MYSQL_TYPE_LONG;
    result[2].buffer = &score;
    result[2].is_null = false;

    // total_count
    result[3].buffer_type = MYSQL_TYPE_LONG;
    result[3].buffer = &total_count;
    result[3].is_null = false;

    // win_count
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &win_count;
    result[4].is_null = false;

    // status
    result[5].buffer_type = MYSQL_TYPE_TINY;
    result[5].buffer = &status;
    result[5].is_null = false;

    if (mysql_stmt_bind_result(stmt, result) != 0) {
        LOG_ERROR("UserTable: bind result failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false; // 用户不存在
    }

    mysql_stmt_close(stmt);

    // 填充 JSON 输出
    username_buf[username_len] = '\0';

    out["id"] = (int64_t)id;
    out["username"] = username_buf;
    out["score"] = (int64_t)score;
    out["total_count"] = (int64_t)total_count;
    out["win_count"] = (int64_t)win_count;
    out["status"] = status;

    LOG_DEBUG("UserTable: select_by_username - username=" << username);
    return true;
}

inline bool UserTable::select_by_id(int64_t user_id, Json::Value& out) {
    if (!_pool) {
        LOG_ERROR("UserTable: select_by_id called before init");
        return false;
    }

    auto conn = _pool->get_connection();
    if (!conn) {
        LOG_ERROR("UserTable: failed to get connection for select_by_id");
        return false;
    }

    // 查询不包含 password_hash 的公开记录
    const std::string sql =
        "SELECT id, username, score, total_count, win_count, status "
        "FROM user WHERE id = ?";

    auto stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        LOG_ERROR("UserTable: mysql_stmt_init failed");
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR("UserTable: mysql_stmt_prepare failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定输入参数
    MYSQL_BIND param;
    memset(&param, 0, sizeof(param));
    param.buffer_type = MYSQL_TYPE_LONGLONG;
    param.buffer = &user_id;
    param.is_null = false;

    if (mysql_stmt_bind_param(stmt, &param) != 0) {
        LOG_ERROR("UserTable: bind param failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR("UserTable: execute failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定结果
    MYSQL_BIND result[6];
    memset(result, 0, sizeof(result));

    unsigned long id;
    char username_buf[33];
    unsigned long username_len = 0;
    unsigned long score;
    unsigned long total_count;
    unsigned long win_count;
    int status;

    // id
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &id;
    result[0].is_null = false;

    // username
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = username_buf;
    result[1].buffer_length = sizeof(username_buf) - 1;
    result[1].length = &username_len;
    result[1].is_null = false;

    // score
    result[2].buffer_type = MYSQL_TYPE_LONG;
    result[2].buffer = &score;
    result[2].is_null = false;

    // total_count
    result[3].buffer_type = MYSQL_TYPE_LONG;
    result[3].buffer = &total_count;
    result[3].is_null = false;

    // win_count
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &win_count;
    result[4].is_null = false;

    // status
    result[5].buffer_type = MYSQL_TYPE_TINY;
    result[5].buffer = &status;
    result[5].is_null = false;

    if (mysql_stmt_bind_result(stmt, result) != 0) {
        LOG_ERROR("UserTable: bind result failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false; // 用户不存在
    }

    mysql_stmt_close(stmt);

    // 填充 JSON 输出
    username_buf[username_len] = '\0';

    out["id"] = (int64_t)id;
    out["username"] = username_buf;
    out["score"] = (int64_t)score;
    out["total_count"] = (int64_t)total_count;
    out["win_count"] = (int64_t)win_count;
    out["status"] = status;

    LOG_DEBUG("UserTable: select_by_id - user_id=" << user_id);
    return true;
}

} // namespace gobang
