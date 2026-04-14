#include "security.hpp"
#include <gtest/gtest.h>
#include <thread>

// ==================== PBKDF2 测试 ====================

TEST(SecurityTest, PBKDF2_DifferentPasswords) {
    std::string hash1 = gobang::security::pbkdf2_hash("password123");
    std::string hash2 = gobang::security::pbkdf2_hash("password456");

    ASSERT_FALSE(hash1.empty());   // 断言 1: 哈希不为空
    ASSERT_NE(hash1, hash2);       // 断言 2: 不同密码生成不同哈希
}

TEST(SecurityTest, PBKDF2_SamePasswordDifferentSalt) {
    std::string hash1 = gobang::security::pbkdf2_hash("password123");
    std::string hash2 = gobang::security::pbkdf2_hash("password123");

    ASSERT_NE(hash1, hash2);       // 断言 1: 相同密码因 salt 不同生成不同哈希
}

TEST(SecurityTest, PBKDF2_Verify_CorrectPassword) {
    std::string password = "my_secure_password";
    std::string hash = gobang::security::pbkdf2_hash(password);

    ASSERT_TRUE(gobang::security::pbkdf2_verify(password, hash));  // 断言 1: 正确密码通过
}

TEST(SecurityTest, PBKDF2_Verify_WrongPassword) {
    std::string password = "my_secure_password";
    std::string hash = gobang::security::pbkdf2_hash(password);

    ASSERT_FALSE(gobang::security::pbkdf2_verify("wrong_password", hash));  // 断言 1: 错误密码拒绝
}

// ==================== JWT 测试 ====================

TEST(SecurityTest, JWT_Generate_ValidFormat) {
    std::string token = gobang::security::jwt_generate(12345);

    ASSERT_FALSE(token.empty());                    // 断言 1: token 不为空
    // JWT 格式：header.payload.signature，三段用 "." 分隔
    size_t first = token.find('.');
    ASSERT_NE(first, std::string::npos);           // 断言 2: 至少有一个点
    ASSERT_NE(token.find('.', first + 1), std::string::npos);  // 断言 3: 有第二个点
}

TEST(SecurityTest, JWT_Verify_ValidToken) {
    int64_t user_id = 12345;
    std::string token = gobang::security::jwt_generate(user_id, 3600);

    int64_t verified_id = gobang::security::jwt_verify(token);
    ASSERT_NE(verified_id, 0);        // 断言 1: 验证成功返回非 0
    ASSERT_EQ(verified_id, user_id);  // 断言 2: 返回正确的 user_id
}

TEST(SecurityTest, JWT_Verify_ExpiredToken) {
    int64_t user_id = 12345;
    // 生成 1 秒后过期的 token
    std::string token = gobang::security::jwt_generate(user_id, 1);

    // 等待过期
    std::this_thread::sleep_for(std::chrono::seconds(2));

    int64_t verified_id = gobang::security::jwt_verify(token);
    ASSERT_EQ(verified_id, 0);  // 断言 1: 过期 token 返回 0
}

TEST(SecurityTest, JWT_Verify_ForgedToken) {
    std::string fake_token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                             "eyJ1c2VyX2lkIjoiMTIzNDUiLCJpYXQiOjE3MTMwMDAwMDAsImV4cCI6MTcxMzA0NjAwMH0."
                             "fake_signature";

    int64_t verified_id = gobang::security::jwt_verify(fake_token);
    ASSERT_EQ(verified_id, 0);  // 断言 1: 伪造 token 返回 0
}

// ==================== 主函数 ====================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
