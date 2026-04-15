#include "auth_handler.hpp"
#include "auth_middleware.hpp"
#include "db.hpp"
#include "user_table.hpp"
#include "util.hpp"
#include "config.h"
#include <gtest/gtest.h>

using namespace gobang;

class AuthApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化 DBPool
        gobang::util::Config cfg = gobang::util::load_config(GOBANG_CONFIG_PATH);
        pool_.init(cfg);
        user_table_.init(&pool_);

        // 清空 user 表
        auto conn = pool_.get_connection();
        int ret = mysql_query(conn.get(), "TRUNCATE TABLE user");
        ASSERT_EQ(ret, 0) << "TRUNCATE TABLE failed: " << mysql_error(conn.get());
    }

    DBPool pool_;
    UserTable user_table_;
};

TEST_F(AuthApiTest, Register_Success) {
    Json::Value result = gobang::auth::handle_register(
        user_table_, "testuser", "password123");

    ASSERT_TRUE(result["success"].asBool());              // 断言 1
    ASSERT_EQ(result["message"].asString(), "注册成功");   // 断言 2
    ASSERT_GT(result["user_id"].asInt64(), 0);           // 断言 3
}

TEST_F(AuthApiTest, Register_DuplicateUsername) {
    // 第一次注册
    gobang::auth::handle_register(user_table_, "testuser", "password123");

    // 重复注册
    Json::Value result = gobang::auth::handle_register(
        user_table_, "testuser", "password456");

    ASSERT_FALSE(result["success"].asBool());             // 断言 1
    ASSERT_EQ(result["message"].asString(), "用户名已存在");  // 断言 2
}

TEST_F(AuthApiTest, Register_ParameterValidation) {
    // 用户名太短
    Json::Value r1 = gobang::auth::handle_register(user_table_, "ab", "password123");
    ASSERT_FALSE(r1["success"].asBool());  // 断言 1

    // 用户名太长
    Json::Value r2 = gobang::auth::handle_register(
        user_table_, std::string(33, 'a'), "password123");
    ASSERT_FALSE(r2["success"].asBool());  // 断言 2

    // 密码太短
    Json::Value r3 = gobang::auth::handle_register(user_table_, "testuser", "12345");
    ASSERT_FALSE(r3["success"].asBool());  // 断言 3

    // 密码太长
    Json::Value r4 = gobang::auth::handle_register(
        user_table_, "testuser", std::string(129, 'a'));
    ASSERT_FALSE(r4["success"].asBool());  // 断言 4
}

TEST_F(AuthApiTest, Login_Success) {
    // 先注册
    gobang::auth::handle_register(user_table_, "testuser", "password123");

    // 登录
    Json::Value result = gobang::auth::handle_login(
        user_table_, "testuser", "password123");

    ASSERT_TRUE(result["success"].asBool());              // 断言 1
    ASSERT_FALSE(result["token"].asString().empty());    // 断言 2
    ASSERT_EQ(result["user"]["username"].asString(), "testuser");  // 断言 3
    ASSERT_FALSE(result["user"].isMember("password_hash"));  // 断言 4: 不返回密码
}

TEST_F(AuthApiTest, Login_WrongPassword) {
    // 先注册
    gobang::auth::handle_register(user_table_, "testuser", "password123");

    // 错误密码登录
    Json::Value result = gobang::auth::handle_login(
        user_table_, "testuser", "wrongpassword");

    ASSERT_FALSE(result["success"].asBool());                 // 断言 1
    ASSERT_EQ(result["message"].asString(), "用户名或密码错误");  // 断言 2
}

TEST_F(AuthApiTest, Token_Verify) {
    // 注册并登录获取 token
    gobang::auth::handle_register(user_table_, "testuser", "password123");
    Json::Value login_result = gobang::auth::handle_login(
        user_table_, "testuser", "password123");

    std::string token = login_result["token"].asString();
    std::string auth_header = "Bearer " + token;

    // 验证 token
    int64_t user_id = gobang::auth::extract_user_id_from_token(auth_header);

    ASSERT_GT(user_id, 0);                                          // 断言 1
    ASSERT_EQ(user_id, login_result["user"]["id"].asInt64());     // 断言 2
    ASSERT_TRUE(gobang::auth::is_authenticated(auth_header));      // 断言 3
}
