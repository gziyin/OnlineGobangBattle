#pragma once
#include <string>
#include <algorithm>
#include <json/json.h>

#include "security.hpp"
#include "logger.hpp"

namespace gobang {
namespace auth {

/**
 * @brief 从 HTTP Authorization Header 中提取并验证 JWT
 * @param auth_header HTTP Header 中的 Authorization 值
 * @return 验证成功返回 user_id，失败返回 0
 *
 * Header 格式：Authorization: Bearer <jwt_token>
 */
inline int64_t extract_user_id_from_token(const std::string& auth_header) {
    // Header 格式：Bearer <token>
    const std::string bearer_prefix = "Bearer ";

    if (auth_header.length() < bearer_prefix.length()) {
        return 0;
    }

    // 检查前缀（大小写不敏感）
    // 注：RFC 6750 规定 Bearer 应大小写敏感，此处为宽松实现，兼容小写
    std::string prefix = auth_header.substr(0, bearer_prefix.length());
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);

    if (prefix != "bearer ") {
        return 0;
    }

    // 提取 token
    std::string token = auth_header.substr(bearer_prefix.length());

    // 验证 token
    return security::jwt_verify(token);
}

/**
 * @brief 检查请求是否已认证
 * @param auth_header HTTP Authorization Header
 * @return 已认证返回 true，未认证返回 false
 */
inline bool is_authenticated(const std::string& auth_header) {
    return extract_user_id_from_token(auth_header) > 0;
}

} // namespace auth
} // namespace gobang
