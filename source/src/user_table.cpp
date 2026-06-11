#include "user_table.hpp"

#include <cstring>

namespace gobang {

void UserTable::init(DBPool* pool) {
    _pool = pool;
    LOG_INFO("UserTable: initialized");
}

int64_t UserTable::insert(const std::string& username, const std::string& password_hash) {
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

    bool is_null = false;

    // username
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = (char*)username.c_str();
    params[0].buffer_length = username.size();
    params[0].is_null = &is_null;
    params[0].length = nullptr;

    // password_hash
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = (char*)password_hash.c_str();
    params[1].buffer_length = password_hash.size();
    params[1].is_null = &is_null;
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

bool UserTable::update_status(int64_t user_id, int status) {
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

    bool is_null = false;

    // status
    params[0].buffer_type = MYSQL_TYPE_LONG;
    params[0].buffer = &status;
    params[0].is_null = &is_null;

    // user_id
    params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    params[1].buffer = &user_id;
    params[1].is_null = &is_null;

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

bool UserTable::update_score_match(int64_t winner_id, int64_t loser_id,
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

    bool is_null = false;

    winner_params[0].buffer_type = MYSQL_TYPE_LONG;
    winner_params[0].buffer = &winner_delta;
    winner_params[0].is_null = &is_null;

    winner_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    winner_params[1].buffer = &winner_id;
    winner_params[1].is_null = &is_null;

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
    loser_params[0].is_null = &is_null;

    loser_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    loser_params[1].buffer = &loser_id;
    loser_params[1].is_null = &is_null;

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

bool UserTable::select_for_auth(const std::string& username, Json::Value& out) {
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
    bool is_null = false;
    param.is_null = &is_null;

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

    unsigned int id;
    char username_buf[33];
    unsigned long username_len = 0;
    char password_hash_buf[513];
    unsigned long password_hash_len = 0;
    unsigned int score;
    unsigned int total_count;
    unsigned int win_count;
    int status;

    bool is_null_false = false;

    // id
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &id;
    result[0].is_null = &is_null_false;

    // username
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = username_buf;
    result[1].buffer_length = sizeof(username_buf) - 1;
    result[1].length = &username_len;
    result[1].is_null = &is_null_false;

    // password_hash
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = password_hash_buf;
    result[2].buffer_length = sizeof(password_hash_buf) - 1;
    result[2].length = &password_hash_len;
    result[2].is_null = &is_null_false;

    // score
    result[3].buffer_type = MYSQL_TYPE_LONG;
    result[3].buffer = &score;
    result[3].is_null = &is_null_false;

    // total_count
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &total_count;
    result[4].is_null = &is_null_false;

    // win_count
    result[5].buffer_type = MYSQL_TYPE_LONG;
    result[5].buffer = &win_count;
    result[5].is_null = &is_null_false;

    // status
    result[6].buffer_type = MYSQL_TYPE_TINY;
    result[6].buffer = &status;
    result[6].is_null = &is_null_false;

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

    out["id"] = (Json::Int64)id;
    out["username"] = username_buf;
    out["password_hash"] = password_hash_buf;
    out["score"] = (Json::UInt64)score;
    out["total_count"] = (Json::UInt64)total_count;
    out["win_count"] = (Json::UInt64)win_count;
    out["status"] = status;

    LOG_DEBUG("UserTable: select_for_auth - username=" << username);
    return true;
}

bool UserTable::select_by_username(const std::string& username, Json::Value& out) {
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

    // 绑定输入参数（select_by_username）
    MYSQL_BIND param_username;
    memset(&param_username, 0, sizeof(param_username));
    param_username.buffer_type = MYSQL_TYPE_STRING;
    param_username.buffer = (char*)username.c_str();
    param_username.buffer_length = username.size();
    bool is_null_username = false;
    param_username.is_null = &is_null_username;

    if (mysql_stmt_bind_param(stmt, &param_username) != 0) {
        LOG_ERROR("UserTable: bind param failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR("UserTable: execute failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定结果（select_by_username）
    MYSQL_BIND result[6];
    memset(result, 0, sizeof(result));

    unsigned int id;
    char username_buf[33];
    unsigned long username_len = 0;
    unsigned int score;
    unsigned int total_count;
    unsigned int win_count;
    int status;

    bool is_null_false = false;

    // id
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &id;
    result[0].is_null = &is_null_false;

    // username
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = username_buf;
    result[1].buffer_length = sizeof(username_buf) - 1;
    result[1].length = &username_len;
    result[1].is_null = &is_null_false;

    // score
    result[2].buffer_type = MYSQL_TYPE_LONG;
    result[2].buffer = &score;
    result[2].is_null = &is_null_false;

    // total_count
    result[3].buffer_type = MYSQL_TYPE_LONG;
    result[3].buffer = &total_count;
    result[3].is_null = &is_null_false;

    // win_count
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &win_count;
    result[4].is_null = &is_null_false;

    // status
    result[5].buffer_type = MYSQL_TYPE_TINY;
    result[5].buffer = &status;
    result[5].is_null = &is_null_false;

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

    out["id"] = (Json::Int64)id;
    out["username"] = username_buf;
    out["score"] = (Json::UInt64)score;
    out["total_count"] = (Json::UInt64)total_count;
    out["win_count"] = (Json::UInt64)win_count;
    out["status"] = status;

    LOG_DEBUG("UserTable: select_by_username - username=" << username);
    return true;
}

bool UserTable::select_by_id(int64_t user_id, Json::Value& out) {
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

    // 绑定输入参数（select_by_id）
    MYSQL_BIND param_id;
    memset(&param_id, 0, sizeof(param_id));
    param_id.buffer_type = MYSQL_TYPE_LONGLONG;
    param_id.buffer = &user_id;
    bool is_null_id = false;
    param_id.is_null = &is_null_id;

    if (mysql_stmt_bind_param(stmt, &param_id) != 0) {
        LOG_ERROR("UserTable: bind param failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR("UserTable: execute failed: " << mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    // 绑定结果（select_by_id）
    MYSQL_BIND result[6];
    memset(result, 0, sizeof(result));

    unsigned int id;
    char username_buf[33];
    unsigned long username_len = 0;
    unsigned int score;
    unsigned int total_count;
    unsigned int win_count;
    int status;

    bool is_null_false = false;

    // id
    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &id;
    result[0].is_null = &is_null_false;

    // username
    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = username_buf;
    result[1].buffer_length = sizeof(username_buf) - 1;
    result[1].length = &username_len;
    result[1].is_null = &is_null_false;

    // score
    result[2].buffer_type = MYSQL_TYPE_LONG;
    result[2].buffer = &score;
    result[2].is_null = &is_null_false;

    // total_count
    result[3].buffer_type = MYSQL_TYPE_LONG;
    result[3].buffer = &total_count;
    result[3].is_null = &is_null_false;

    // win_count
    result[4].buffer_type = MYSQL_TYPE_LONG;
    result[4].buffer = &win_count;
    result[4].is_null = &is_null_false;

    // status
    result[5].buffer_type = MYSQL_TYPE_TINY;
    result[5].buffer = &status;
    result[5].is_null = &is_null_false;

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

    out["id"] = (Json::Int64)id;
    out["username"] = username_buf;
    out["score"] = (Json::UInt64)score;
    out["total_count"] = (Json::UInt64)total_count;
    out["win_count"] = (Json::UInt64)win_count;
    out["status"] = status;

    LOG_DEBUG("UserTable: select_by_id - user_id=" << user_id);
    return true;
}

} // namespace gobang
