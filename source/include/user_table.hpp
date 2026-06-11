#pragma once
#include <string>
#include <cstdint>
#include <json/json.h>

#include "db.hpp"
#include "logger.hpp"
#include "util.hpp"

// MySQL 错误码（ER_DUP_ENTRY = 1062）
#ifndef ER_DUP_ENTRY
#define ER_DUP_ENTRY 1062
#endif

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

} // namespace gobang
