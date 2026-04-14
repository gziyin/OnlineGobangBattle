#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <jwt-cpp/jwt.h>

#include "util.hpp"
#include "logger.hpp"

namespace gobang {
namespace security {

// PBKDF2 参数配置
const int PBKDF2_ITERATIONS = 10000;
const int SALT_LENGTH = 16;
const int KEY_LENGTH = 32;

/**
 * @brief Base64 编码
 */
static inline std::string base64_encode(const unsigned char* data, size_t len) {
    BIO* bio = nullptr;
    BIO* b64 = nullptr;
    BUF_MEM* bufferPtr = nullptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_write(bio, data, static_cast<int>(len));
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return result;
}

/**
 * @brief Base64 解码
 */
static inline std::vector<unsigned char> base64_decode(const std::string& encoded) {
    BIO* bio = nullptr;
    BIO* b64 = nullptr;
    size_t len = encoded.length();

    std::vector<unsigned char> buffer(len);

    bio = BIO_new_mem_buf(encoded.data(), static_cast<int>(len));
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    int decoded_len = BIO_read(bio, buffer.data(), static_cast<int>(len));
    buffer.resize(decoded_len > 0 ? static_cast<size_t>(decoded_len) : 0);

    BIO_free_all(bio);
    return buffer;
}

/**
 * @brief PBKDF2 密码加密
 * @param password 原始密码
 * @return 加密后的哈希字符串（格式：salt:hash，base64 编码），失败返回空字符串
 */
static inline std::string pbkdf2_hash(const std::string& password) {
    // 1. 生成随机盐
    unsigned char salt[SALT_LENGTH];
    if (RAND_bytes(salt, SALT_LENGTH) != 1) {
        LOG_ERROR("Security: RAND_bytes failed");
        return "";
    }

    // 2. PBKDF2 密钥派生
    unsigned char hash[KEY_LENGTH];
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            salt, SALT_LENGTH,
            PBKDF2_ITERATIONS,
            EVP_sha256(),
            KEY_LENGTH,
            hash) != 1) {
        LOG_ERROR("Security: PKCS5_PBKDF2_HMAC failed");
        return "";
    }

    // 3. 编码输出：salt:hash
    std::string salt_b64 = base64_encode(salt, SALT_LENGTH);
    std::string hash_b64 = base64_encode(hash, KEY_LENGTH);
    return salt_b64 + ":" + hash_b64;
}

/**
 * @brief PBKDF2 密码验证
 * @param password 原始密码
 * @param hash_str 加密后的哈希（格式：salt:hash）
 * @return 验证通过返回 true，失败返回 false
 */
static inline bool pbkdf2_verify(const std::string& password, const std::string& hash_str) {
    // 1. 解析 salt:hash
    auto pos = hash_str.find(':');
    if (pos == std::string::npos) {
        LOG_WARN("Security: invalid hash format (missing ':')");
        return false;
    }

    std::string salt_b64 = hash_str.substr(0, pos);
    std::string hash_b64 = hash_str.substr(pos + 1);

    // 2. Base64 解码 salt
    std::vector<unsigned char> salt = base64_decode(salt_b64);
    if (salt.size() != SALT_LENGTH) {
        LOG_WARN("Security: invalid salt length");
        return false;
    }

    // 3. Base64 解码存储的 hash
    std::vector<unsigned char> stored_hash = base64_decode(hash_b64);
    if (stored_hash.size() != KEY_LENGTH) {
        LOG_WARN("Security: invalid hash length");
        return false;
    }

    // 4. 重新计算哈希
    unsigned char new_hash[KEY_LENGTH];
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(), static_cast<int>(password.size()),
            salt.data(), static_cast<int>(salt.size()),
            PBKDF2_ITERATIONS,
            EVP_sha256(),
            KEY_LENGTH,
            new_hash) != 1) {
        LOG_ERROR("Security: PKCS5_PBKDF2_HMAC verify failed");
        return false;
    }

    // 5. 常数时间比较（防止时序攻击）
    if (CRYPTO_memcmp(new_hash, stored_hash.data(), KEY_LENGTH) != 0) {
        LOG_DEBUG("Security: password verification failed");
        return false;
    }

    return true;
}

/**
 * @brief JWT Token 生成
 * @param user_id 用户 ID
 * @param expire_sec 过期时间（秒），默认 86400（24 小时）
 * @return JWT 字符串，失败返回空字符串
 */
static inline std::string jwt_generate(int64_t user_id, int64_t expire_sec = 86400) {
    try {
        auto now = std::chrono::system_clock::now();
        auto exp = now + std::chrono::seconds(expire_sec);

        // 从 Config 读取 JWT 配置
        static gobang::util::Config cfg = gobang::util::load_config("config/server.conf");
        std::string secret = cfg.jwt_secret;
        std::string issuer = cfg.jwt_issuer;

        auto token = jwt::create()
            .set_issuer(issuer)
            .set_type("JWS")
            .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
            .set_issued_at(now)
            .set_expires_at(exp)
            .sign(jwt::algorithm::hs256{secret});

        LOG_DEBUG("Security: JWT generated for user_id=" << user_id);
        return token;
    } catch (const std::exception& e) {
        LOG_ERROR("Security: JWT generate error - " << e.what());
        return "";
    }
}

/**
 * @brief JWT Token 验证
 * @param token JWT 字符串
 * @return 验证成功返回 user_id，失败返回 0
 */
static inline int64_t jwt_verify(const std::string& token) {
    try {
        // 从 Config 读取 JWT 配置
        static gobang::util::Config cfg = gobang::util::load_config("config/server.conf");
        std::string secret = cfg.jwt_secret;
        std::string issuer = cfg.jwt_issuer;

        // jwt-cpp v5 使用 verifier 验证
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer(issuer);

        // 解码 token
        auto decoded = jwt::decode(token);

        // 验证签名和 claims
        verifier.verify(decoded);

        // 获取 user_id
        auto user_id_claim = decoded.get_payload_claim("user_id");
        if (user_id_claim.is_string()) {
            int64_t user_id = std::stoll(user_id_claim.as_string());
            LOG_DEBUG("Security: JWT verified, user_id=" << user_id);
            return user_id;
        }

        LOG_WARN("Security: JWT claim 'user_id' not found or invalid type");
        return 0;

    } catch (const jwt::error::token_verification_exception& e) {
        // 过期、签名错误、issuer 不对
        LOG_DEBUG("Security: JWT verify failed - " << e.what());
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Security: JWT verify error - " << e.what());
        return 0;
    } catch (...) {
        LOG_ERROR("Security: JWT verify unknown error");
        return 0;
    }
}

} // namespace security
} // namespace gobang
