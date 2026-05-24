#include "security.hpp"
#include "util.hpp"
#include <gtest/gtest.h>
#include <thread>

TEST(SecurityTest, PBKDF2_DifferentPasswords) {
    std::string hash1 = gobang::security::pbkdf2_hash("password123");
    std::string hash2 = gobang::security::pbkdf2_hash("password456");

    ASSERT_FALSE(hash1.empty());
    ASSERT_NE(hash1, hash2);
}

TEST(SecurityTest, PBKDF2_SamePasswordDifferentSalt) {
    std::string hash1 = gobang::security::pbkdf2_hash("password123");
    std::string hash2 = gobang::security::pbkdf2_hash("password123");

    ASSERT_NE(hash1, hash2);
}

TEST(SecurityTest, PBKDF2_Verify_CorrectPassword) {
    std::string password = "my_secure_password";
    std::string hash = gobang::security::pbkdf2_hash(password);

    ASSERT_TRUE(gobang::security::pbkdf2_verify(password, hash));
}

TEST(SecurityTest, PBKDF2_Verify_WrongPassword) {
    std::string password = "my_secure_password";
    std::string hash = gobang::security::pbkdf2_hash(password);

    ASSERT_FALSE(gobang::security::pbkdf2_verify("wrong_password", hash));
}

TEST(SecurityTest, JWT_Generate_ValidFormat) {
    std::string token = gobang::security::jwt_generate(12345);

    ASSERT_FALSE(token.empty());
    size_t first = token.find('.');
    ASSERT_NE(first, std::string::npos);
    ASSERT_NE(token.find('.', first + 1), std::string::npos);
}

TEST(SecurityTest, JWT_Verify_ValidToken) {
    int64_t user_id = 12345;
    std::string token = gobang::security::jwt_generate(user_id, 3600);

    int64_t verified_id = gobang::security::jwt_verify(token);
    ASSERT_NE(verified_id, 0);
    ASSERT_EQ(verified_id, user_id);
}

TEST(SecurityTest, JWT_Verify_ExpiredToken) {
    int64_t user_id = 12345;
    std::string token = gobang::security::jwt_generate(user_id, 1);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    int64_t verified_id = gobang::security::jwt_verify(token);
    ASSERT_EQ(verified_id, 0);
}

TEST(SecurityTest, JWT_Verify_ForgedToken) {
    std::string fake_token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                             "eyJ1c2VyX2lkIjoiMTIzNDUiLCJpYXQiOjE3MTMwMDAwMDAsImV4cCI6MTcxMzA0NjAwMH0."
                             "fake_signature";

    int64_t verified_id = gobang::security::jwt_verify(fake_token);
    ASSERT_EQ(verified_id, 0);
}

TEST(SecurityTest, ConfigRejectsWeakJwtSecret) {
    gobang::util::Config cfg;
    cfg.db_password = "x";
    cfg.jwt_secret = "CHANGE_ME";

    EXPECT_THROW(gobang::util::validate_config(cfg), std::runtime_error);
}

TEST(SecurityTest, ConfigAcceptsStrongJwtSecret) {
    gobang::util::Config cfg;
    cfg.db_password = "x";
    cfg.jwt_secret = "A_strong_secret_for_m3_governance_2026!";

    EXPECT_NO_THROW(gobang::util::validate_config(cfg));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
