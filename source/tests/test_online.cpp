#include "online.hpp"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

using namespace gobang;

TEST(OnlineManagerTest, UserOnlineDefaultsToHallIdle) {
    OnlineManager mgr;
    mgr.user_online(1001);

    ASSERT_TRUE(mgr.is_online(1001));
    ASSERT_EQ(mgr.get_status(1001), OnlineStatus::HALL_IDLE);
    ASSERT_EQ(mgr.online_count(), 1u);
}

TEST(OnlineManagerTest, DuplicateOnlineOverridesStatusToHallIdle) {
    OnlineManager mgr;
    mgr.user_online(1001);
    ASSERT_TRUE(mgr.set_status(1001, OnlineStatus::MATCHING));

    mgr.user_online(1001);

    ASSERT_EQ(mgr.get_status(1001), OnlineStatus::HALL_IDLE);
    ASSERT_EQ(mgr.online_count(), 1u);
}

TEST(OnlineManagerTest, UserOfflineCleansState) {
    OnlineManager mgr;
    mgr.user_online(1001);
    mgr.user_offline(1001);

    ASSERT_FALSE(mgr.is_online(1001));
    ASSERT_EQ(mgr.get_status(1001), OnlineStatus::OFFLINE);
    ASSERT_EQ(mgr.online_count(), 0u);
}

TEST(OnlineManagerTest, StatusTransitionWorks) {
    OnlineManager mgr;
    mgr.user_online(1001);

    ASSERT_TRUE(mgr.set_status(1001, OnlineStatus::MATCHING));
    ASSERT_EQ(mgr.get_status(1001), OnlineStatus::MATCHING);

    ASSERT_TRUE(mgr.set_status(1001, OnlineStatus::IN_ROOM));
    ASSERT_EQ(mgr.get_status(1001), OnlineStatus::IN_ROOM);

    ASSERT_TRUE(mgr.set_status(1001, OnlineStatus::HALL_IDLE));
    ASSERT_EQ(mgr.get_status(1001), OnlineStatus::HALL_IDLE);
}

TEST(OnlineManagerTest, ConcurrentReadWriteIsSafe) {
    OnlineManager mgr;
    const int user_count = 64;

    std::vector<std::thread> threads;
    for (int i = 0; i < user_count; ++i) {
        threads.push_back(std::thread([&mgr, i]() {
            const int64_t user_id = 1000 + i;
            mgr.user_online(user_id);
            mgr.set_status(user_id, OnlineStatus::MATCHING);
            mgr.get_status(user_id);
            mgr.is_online(user_id);
        }));
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    ASSERT_EQ(mgr.online_count(), static_cast<size_t>(user_count));
    for (int i = 0; i < user_count; ++i) {
        ASSERT_TRUE(mgr.is_online(1000 + i));
        ASSERT_EQ(mgr.get_status(1000 + i), OnlineStatus::MATCHING);
    }
}

TEST(OnlineManagerTest, OfflineUserCannotSetStatus) {
    OnlineManager mgr;
    ASSERT_FALSE(mgr.set_status(42, OnlineStatus::MATCHING));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
