#pragma once
#include <string>
#include <json/json.h>

#include "db.hpp"
#include "user_table.hpp"
#include "security.hpp"
#include "logger.hpp"

namespace gobang {
namespace auth {

/**
 * @brief 用户注册请求处理
 * @param user_table 用户表操作对象
 * @param username 用户名
 * @param password 原始密码
 * @return JSON 响应 {success: bool, message: string, user_id: number}
 *
 * 流程：
 * 1. 参数校验（用户名长度 3-32，密码长度 6-128）
 * 2. 检查用户名是否已存在
 * 3. PBKDF2 加密密码
 * 4. 插入用户表
 * 5. 返回结果
 */
inline Json::Value handle_register(UserTable& user_table,
                                   const std::string& username,
                                   const std::string& password) {
    Json::Value response;

    // 1. 参数校验
    if (username.length() < 3 || username.length() > 32) {
        response["success"] = false;
        response["message"] = "用户名长度必须在 3-32 字符之间";
        return response;
    }
    if (password.length() < 6 || password.length() > 128) {
        response["success"] = false;
        response["message"] = "密码长度必须在 6-128 字符之间";
        return response;
    }

    // 2. 检查用户名是否已存在
    Json::Value existing_user;
    if (user_table.select_by_username(username, existing_user)) {
        response["success"] = false;
        response["message"] = "用户名已存在";
        return response;
    }

    // 3. PBKDF2 加密密码
    std::string password_hash = security::pbkdf2_hash(password);
    if (password_hash.empty()) {
        response["success"] = false;
        response["message"] = "密码加密失败";
        return response;
    }

    // 4. 插入用户表（TOCTOU 防护：insert 失败后再次检查用户名是否存在）
    int64_t user_id = user_table.insert(username, password_hash);
    if (user_id == 0) {
        // 并发情况下：先检查后插入可能存在竞态，insert 因 UNIQUE 约束失败
        // 再次检查用户名是否存在，给用户准确的错误信息
        Json::Value existing_user2;
        if (user_table.select_by_username(username, existing_user2)) {
            response["success"] = false;
            response["message"] = "用户名已存在";
        } else {
            response["success"] = false;
            response["message"] = "用户注册失败";
        }
        return response;
    }

    // 5. 返回成功
    response["success"] = true;
    response["message"] = "注册成功";
    response["user_id"] = static_cast<Json::Int64>(user_id);
    LOG_INFO("Auth: 用户注册成功，username=" << username << ", user_id=" << user_id);
    return response;
}

/**
 * @brief 用户登录请求处理
 * @param user_table 用户表操作对象
 * @param username 用户名
 * @param password 原始密码
 * @return JSON 响应 {success: bool, message: string, token: string, user: {...}}
 *
 * 流程：
 * 1. 参数校验
 * 2. 查询用户（使用 select_for_auth，包含 password_hash）
 * 3. PBKDF2 验证密码
 * 4. 生成 JWT Token
 * 5. 返回 token 和用户信息（不包含 password_hash）
 */
inline Json::Value handle_login(UserTable& user_table,
                                const std::string& username,
                                const std::string& password) {
    Json::Value response;

    // 1. 参数校验
    if (username.empty() || password.empty()) {
        response["success"] = false;
        response["message"] = "用户名和密码不能为空";
        return response;
    }

    // 2. 查询用户（使用 select_for_auth 获取 password_hash）
    Json::Value user;
    if (!user_table.select_for_auth(username, user)) {
        response["success"] = false;
        response["message"] = "用户名或密码错误";
        return response;
    }

    // 3. PBKDF2 验证密码
    std::string stored_hash = user["password_hash"].asString();
    if (!security::pbkdf2_verify(password, stored_hash)) {
        response["success"] = false;
        response["message"] = "用户名或密码错误";
        LOG_WARN("Auth: 登录密码错误，username=" << username);
        return response;
    }

    // 4. 生成 JWT Token
    int64_t user_id = user["id"].asInt64();
    std::string token = security::jwt_generate(user_id);
    if (token.empty()) {
        response["success"] = false;
        response["message"] = "Token 生成失败";
        return response;
    }

    // 5. 返回结果（移除 password_hash）
    response["success"] = true;
    response["message"] = "登录成功";
    response["token"] = token;

    Json::Value user_info;
    user_info["id"] = static_cast<Json::Int64>(user_id);
    user_info["username"] = user["username"];
    user_info["score"] = user["score"];
    user_info["total_count"] = user["total_count"];
    user_info["win_count"] = user["win_count"];
    user_info["status"] = user["status"];
    response["user"] = user_info;

    LOG_INFO("Auth: 用户登录成功，username=" << username << ", user_id=" << user_id);
    return response;
}

} // namespace auth
} // namespace gobang
